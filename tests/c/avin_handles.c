/* The counted C objects of the handle tests: see avin_handles.h. Counts and reference counts are
 * atomic, since a `shared` handle may be used from any thread. */
#include "avin_handles.h"
#include "avin_shapes_declared.h"

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static long counts[11][6];

static void count(int kind, int what) { __atomic_fetch_add(&counts[kind][what], 1, __ATOMIC_SEQ_CST); }

long fx_count(int kind, int what) { return __atomic_load_n(&counts[kind][what], __ATOMIC_SEQ_CST); }

static void stop(const char *what) {
    fprintf(stderr, "fixture: %s\n", what);
    fflush(stderr);
    abort();
}

#define LIVE 0x4c495645
#define DEAD 0x44454144

struct fx_object {
    int magic;
    int refs;
    int value;
};

static void check_object(const fx_object *o, const char *what) {
    if (!o) stop("an object is null");
    if (o->magic == DEAD) stop(what);
    if (o->magic != LIVE) stop("not an object");
}

fx_object *fx_object_new(int value) {
    fx_object *o = malloc(sizeof *o);
    if (!o) stop("out of memory");
    o->magic = LIVE;
    o->refs = 1;
    o->value = value;
    count(FX_OBJECT, FX_CREATED);
    return o;
}

fx_object *fx_object_ref(fx_object *o) {
    check_object(o, "a reference taken to a freed object");
    __atomic_fetch_add(&o->refs, 1, __ATOMIC_SEQ_CST);
    count(FX_OBJECT, FX_RETAINED);
    return o;
}

void fx_object_unref(fx_object *o) {
    check_object(o, "an object given back after it was freed");
    count(FX_OBJECT, FX_RELEASED);
    if (__atomic_sub_fetch(&o->refs, 1, __ATOMIC_SEQ_CST) == 0) {
        o->magic = DEAD;
        count(FX_OBJECT, FX_FREED);
    }
}

int fx_object_value(const fx_object *o) {
    check_object(o, "an object used after it was freed");
    return o->value;
}

void fx_object_set(fx_object *o, int value) {
    check_object(o, "an object used after it was freed");
    o->value = value;
}

int fx_object_refs(const fx_object *o) {
    check_object(o, "an object used after it was freed");
    return __atomic_load_n(&o->refs, __ATOMIC_SEQ_CST);
}

fx_object *fx_object_none(int value) { return value == 0 ? 0 : fx_object_new(value); }

int fx_object_check(fx_object *o, int want) { return fx_object_value(o) - want; }

fx_object *fx_object_lend(fx_object *o) {
    check_object(o, "an object lent after it was freed");
    return o;
}

/* Libraries and faces. */

#define MAX_FACES 64

struct fx_library_rec {
    int magic;
    int refs;
    int count;
    fx_face faces[MAX_FACES];
    fx_object *adopted[MAX_FACES];
    int adoptions;
};

struct fx_face_full {
    struct fx_face_rec face;
    int magic;
    int refs;
    fx_object *memory;
};

static void check_library(fx_library library, const char *what) {
    if (!library) stop("a library is null");
    if (library->magic == DEAD) stop(what);
    if (library->magic != LIVE) stop("not a library");
}

static struct fx_face_full *full(fx_face face) { return (struct fx_face_full *)face; }

static void check_face(fx_face face, const char *what) {
    if (!face) stop("a face is null");
    if (full(face)->magic == DEAD) stop(what);
    if (full(face)->magic != LIVE) stop("not a face");
}

fx_library fx_library_new(void) {
    fx_library library = calloc(1, sizeof *library);
    if (!library) stop("out of memory");
    library->magic = LIVE;
    library->refs = 1;
    count(FX_LIBRARY, FX_CREATED);
    return library;
}

int fx_library_reference(fx_library library) {
    check_library(library, "a reference taken to a freed library");
    library->refs += 1;
    count(FX_LIBRARY, FX_RETAINED);
    return 0;
}

static void free_face(fx_face face) {
    full(face)->magic = DEAD;
    count(FX_FACE, FX_FREED);
}

int fx_library_done(fx_library library) {
    check_library(library, "a library given back after it was freed");
    count(FX_LIBRARY, FX_RELEASED);
    library->refs -= 1;
    if (library->refs > 0) return 0;
    /* Every face still in the library goes with it. */
    for (int i = 0; i < library->count; i++)
        if (library->faces[i]) free_face(library->faces[i]);
    for (int i = 0; i < library->adoptions; i++) fx_object_unref(library->adopted[i]);
    library->magic = DEAD;
    count(FX_LIBRARY, FX_FREED);
    return 0;
}

int fx_library_faces(fx_library library) {
    check_library(library, "a library used after it was freed");
    int n = 0;
    for (int i = 0; i < library->count; i++)
        if (library->faces[i]) n++;
    return n;
}

void fx_library_adopt(fx_library library, fx_object *o) {
    check_library(library, "a library used after it was freed");
    check_object(o, "a freed object adopted");
    if (library->adoptions == MAX_FACES) stop("a library adopted too many objects");
    library->adopted[library->adoptions++] = o;
}

fx_object *fx_library_adopted(fx_library library) {
    check_library(library, "a library used after it was freed");
    return library->adoptions == 0 ? 0 : library->adopted[library->adoptions - 1];
}

fx_face fx_face_new_over(fx_library library, fx_object *memory, int id) {
    check_library(library, "a face made in a freed library");
    if (memory) check_object(memory, "a face made over a freed object");
    int slot = -1;
    for (int i = 0; i < library->count; i++)
        if (!library->faces[i]) slot = i;
    if (slot < 0) {
        if (library->count == MAX_FACES) return 0;
        slot = library->count++;
    }
    struct fx_face_full *f = calloc(1, sizeof *f);
    if (!f) stop("out of memory");
    f->face.library = library;
    f->face.id = id;
    f->magic = LIVE;
    f->refs = 1;
    f->memory = memory;
    library->faces[slot] = &f->face;
    count(FX_FACE, FX_CREATED);
    return &f->face;
}

fx_face fx_face_new(fx_library library, int id) { return fx_face_new_over(library, 0, id); }

int fx_face_reference(fx_face face) {
    check_face(face, "a reference taken to a freed face");
    full(face)->refs += 1;
    count(FX_FACE, FX_RETAINED);
    return 0;
}

int fx_face_done(fx_face face) {
    check_face(face, "a face freed twice: its library had freed it already");
    count(FX_FACE, FX_RELEASED);
    full(face)->refs -= 1;
    if (full(face)->refs > 0) return 0;
    /* A face leaves its library, which must still be there. */
    fx_library library = face->library;
    check_library(library, "a face given back after its library was freed");
    count(FX_FACE, library->refs == 1 ? FX_AFTER : FX_BEFORE);
    for (int i = 0; i < library->count; i++)
        if (library->faces[i] == face) library->faces[i] = 0;
    if (full(face)->memory) check_object(full(face)->memory, "a face's memory freed before the face");
    free_face(face);
    return 0;
}

int fx_face_id(fx_face face) {
    check_face(face, "a face used after it was freed");
    return face->id;
}

fx_library fx_face_library(fx_face face) {
    check_face(face, "a face used after it was freed");
    return face->library;
}

int fx_face_memory_value(fx_face face) {
    check_face(face, "a face used after it was freed");
    return full(face)->memory ? fx_object_value(full(face)->memory) : -1;
}

struct fx_mmvar {
    int magic;
    fx_library library;
    int axes;
};

fx_mmvar *fx_mmvar_new(fx_face face) {
    check_face(face, "a variation descriptor of a freed face");
    fx_mmvar *mm = malloc(sizeof *mm);
    if (!mm) stop("out of memory");
    mm->magic = LIVE;
    mm->library = face->library;
    mm->axes = face->id * 2;
    count(FX_MMVAR, FX_CREATED);
    return mm;
}

void fx_mmvar_free(fx_library library, fx_mmvar *mm) {
    if (mm->magic == DEAD) stop("a variation descriptor freed twice");
    check_library(library, "a variation descriptor freed through a freed library");
    if (mm->library != library) stop("a variation descriptor freed through another library");
    mm->magic = DEAD;
    count(FX_MMVAR, FX_FREED);
}

int fx_mmvar_axes(const fx_mmvar *mm) {
    if (mm->magic != LIVE) stop("a variation descriptor used after it was freed");
    return mm->axes;
}

/* Plain objects and shelves. */

struct fx_plain {
    int magic;
    int value;
};

fx_plain *fx_plain_new(int value) {
    fx_plain *p = malloc(sizeof *p);
    if (!p) stop("out of memory");
    p->magic = LIVE;
    p->value = value;
    count(FX_PLAIN, FX_CREATED);
    return p;
}

void fx_plain_free(fx_plain *p) {
    if (p->magic == DEAD) stop("a plain object freed twice");
    p->magic = DEAD;
    count(FX_PLAIN, FX_FREED);
}

int fx_plain_value(const fx_plain *p) {
    if (p->magic != LIVE) stop("a plain object used after it was freed");
    return p->value;
}

struct fx_shelf {
    int magic;
    fx_plain *plain;
};

fx_shelf *fx_shelf_new(fx_plain *p) {
    fx_shelf *s = malloc(sizeof *s);
    if (!s) stop("out of memory");
    s->magic = LIVE;
    s->plain = p;
    count(FX_SHELF, FX_CREATED);
    return s;
}

void fx_shelf_free(fx_shelf *s) {
    if (s->magic == DEAD) stop("a shelf freed twice");
    s->magic = DEAD;
    count(FX_SHELF, FX_FREED);
}

int fx_shelf_peek(const fx_shelf *s) {
    if (s->magic != LIVE) stop("a shelf used after it was freed");
    return fx_plain_value(s->plain);
}

/* Statuses. */

fx_status fx_status_of(int code) { return code; }

enum fx_result fx_result_of(int which) { return which == 0 ? FX_RESULT_OK : which == 1 ? FX_RESULT_BAD : FX_RESULT_WORSE; }

long long fx_long_status(long long code) { return code; }

unsigned char fx_byte_status(unsigned char code) { return code; }

/* Shapes, the second package's. */

struct fx_shape {
    int magic;
    fx_object *object;
    fx_object *absorbed[8];
    int absorptions;
};

fx_shape *fx_shape_new(fx_object *o) {
    check_object(o, "a shape made over a freed object");
    fx_shape *s = calloc(1, sizeof *s);
    if (!s) stop("out of memory");
    s->magic = LIVE;
    s->object = o;
    count(FX_SHAPE, FX_CREATED);
    return s;
}

void fx_shape_free(fx_shape *s) {
    if (s->magic == DEAD) stop("a shape freed twice");
    check_object(s->object, "a shape freed after the object it is over");
    for (int i = 0; i < s->absorptions; i++) fx_object_unref(s->absorbed[i]);
    s->magic = DEAD;
    count(FX_SHAPE, FX_FREED);
}

int fx_shape_value(const fx_shape *s) {
    if (s->magic != LIVE) stop("a shape used after it was freed");
    return fx_object_value(s->object);
}

fx_object *fx_shape_object(fx_shape *s) {
    if (s->magic != LIVE) stop("a shape used after it was freed");
    return s->object;
}

fx_object *fx_shape_copy(const fx_shape *s) { return fx_object_new(fx_shape_value(s)); }

void fx_shape_absorb(fx_shape *s, fx_object *o) {
    if (s->magic != LIVE) stop("a shape used after it was freed");
    check_object(o, "a freed object absorbed");
    if (s->absorptions == 8) stop("a shape absorbed too many objects");
    s->absorbed[s->absorptions++] = o;
}

int fx_shape_matches(const fx_shape *s, const fx_object *o) { return fx_shape_value(s) == fx_object_value(o) ? 0 : 1; }

/* A node's memory is never reused, so a freed node is always safe to find dead. Its children are a
 * list through `next`, guarded by one lock, since a finalizer may free a node on any thread. */
struct fx_node {
    int magic;
    int depth;
    fx_node *parent;
    fx_node *first;
    fx_node *next;
};

static pthread_mutex_t nodes_lock = PTHREAD_MUTEX_INITIALIZER;

static void check_node(const fx_node *n, const char *what) {
    if (!n) stop("a node is null");
    if (n->magic == DEAD) stop(what);
    if (n->magic != LIVE) stop("not a node");
}

fx_node *fx_node_new(fx_node *parent) {
    fx_node *n = calloc(1, sizeof *n);
    if (!n) stop("out of memory");
    n->magic = LIVE;
    pthread_mutex_lock(&nodes_lock);
    if (parent) {
        check_node(parent, "a node made under a freed node");
        n->depth = parent->depth + 1;
        n->parent = parent;
        n->next = parent->first;
        parent->first = n;
    }
    pthread_mutex_unlock(&nodes_lock);
    count(FX_NODE, FX_CREATED);
    return n;
}

/* Frees `n` and every node under it, deepest first; `own` says it is the node's own free. */
static void free_node(fx_node *n, int own) {
    while (n->first) {
        fx_node *c = n->first;
        n->first = c->next;
        free_node(c, 0);
    }
    if (n->parent && own) {
        fx_node **at = &n->parent->first;
        while (*at && *at != n) at = &(*at)->next;
        if (*at) *at = n->next;
    }
    n->magic = DEAD;
    count(FX_NODE, FX_FREED);
    count(FX_NODE, own ? FX_AFTER : FX_BEFORE);
}

void fx_node_free(fx_node *n) {
    pthread_mutex_lock(&nodes_lock);
    check_node(n, "a node freed twice");
    if (n->parent) check_node(n->parent, "a node freed after its parent, which freed it");
    free_node(n, 1);
    pthread_mutex_unlock(&nodes_lock);
}

int fx_node_depth(const fx_node *n) {
    check_node(n, "a freed node used");
    return n->depth;
}

int fx_node_pair(const fx_node *a, const fx_node *b) { return fx_node_depth(a) + fx_node_depth(b); }

fx_node *fx_node_root(fx_node *n) {
    check_node(n, "a freed node used");
    while (n->parent) n = n->parent;
    return n;
}

/* A string a root allocated: its root and whether it is live, before its bytes. */
struct fx_text {
    int magic;
    fx_node *root;
    char bytes[];
};

static char *new_text(fx_node *root, const char *s) {
    size_t n = strlen(s);
    struct fx_text *t = malloc(sizeof *t + n + 1);
    if (!t) stop("out of memory");
    t->magic = LIVE;
    t->root = root;
    memcpy(t->bytes, s, n + 1);
    count(FX_TEXT, FX_CREATED);
    return t->bytes;
}

static struct fx_text *text_of(void *p, const char *what) {
    struct fx_text *t = (struct fx_text *)((char *)p - offsetof(struct fx_text, bytes));
    if (t->magic == DEAD) stop(what);
    if (t->magic != LIVE) stop("not a string a root made");
    return t;
}

int fx_node_texts(fx_node *n, int which, char **name, size_t *name_len, char **other) {
    check_node(n, "a freed node used");
    fx_node *root = fx_node_root(n);
    if (which == 0 || which == 2) {
        *name = new_text(root, "node ñ");
        *name_len = strlen(*name);
    }
    if (which != 3) *other = new_text(root, "other");
    return which >= 2 ? 7 : 0;
}

void fx_text_free(fx_node *root, void *p) {
    check_node(root, "a string freed with a freed root");
    struct fx_text *t = text_of(p, "a string freed twice");
    if (t->root != root) stop("a string freed with another root");
    t->magic = DEAD;
    count(FX_TEXT, FX_FREED);
}

void fx_text_release(void *p) {
    struct fx_text *t = text_of(p, "a string freed twice");
    t->magic = DEAD;
    count(FX_TEXT, FX_FREED);
}

struct fx_conn {
    int magic;
};

fx_conn *fx_conn_new(void) {
    fx_conn *c = malloc(sizeof *c);
    if (!c) stop("out of memory");
    c->magic = LIVE;
    count(FX_CONN, FX_CREATED);
    return c;
}

int fx_conn_start(fx_conn *c, int fd) {
    if (!c || c->magic != LIVE) stop("a freed connection used");
    return fd >= 0 ? 0 : 1;
}

fx_conn *fx_conn_with(int fd) { return fd >= 0 ? fx_conn_new() : 0; }

void fx_conn_free(fx_conn *c, int fd) {
    if (!c || c->magic != LIVE) stop("a connection freed twice");
    c->magic = DEAD;
    count(FX_CONN, FX_FREED);
    if (fd >= 0 && write(fd, "bye", 3) == 3) count(FX_CONN, FX_AFTER);
    else count(FX_CONN, FX_BEFORE);
    if (fd >= 0) close(fd);
}

struct fx_stream {
    int magic;
    int fd;
};

fx_stream *fx_stream_new(int fd) {
    fx_stream *s = malloc(sizeof *s);
    if (!s) stop("out of memory");
    s->magic = LIVE;
    s->fd = fd;
    count(FX_STREAM, FX_CREATED);
    return s;
}

void fx_stream_free(fx_stream *s) {
    if (!s || s->magic != LIVE) stop("a stream freed twice");
    s->magic = DEAD;
    close(s->fd);
    count(FX_STREAM, FX_FREED);
}

int fx_stream_fd(const fx_stream *s) {
    if (!s || s->magic != LIVE) stop("a freed stream used");
    return s->fd;
}

int fx_pipe(int *write_end) {
    int ends[2];
    if (pipe(ends) != 0) return -1;
    *write_end = ends[1];
    return ends[0];
}

int fx_dup(int fd) { return dup(fd); }

int fx_close(int fd) { return close(fd); }

char *fx_drain(int fd) {
    size_t used = 0;
    size_t size = 64;
    char *s = malloc(size);
    if (!s) stop("out of memory");
    for (;;) {
        if (used + 1 == size) {
            size *= 2;
            s = realloc(s, size);
            if (!s) stop("out of memory");
        }
        ssize_t n = read(fd, s + used, size - used - 1);
        if (n <= 0) break;
        used += (size_t)n;
    }
    s[used] = 0;
    close(fd);
    return s;
}
