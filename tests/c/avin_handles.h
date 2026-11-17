/* The C objects the handle tests bind: each kind counts what is made of it, every reference taken
 * and given back, and every object freed, and stops the process on a double free or a use after
 * free, so a binding that gets ownership wrong fails loudly rather than by chance. A freed object's
 * memory is never reused, so the check that it is dead is always safe to make. The test harness
 * compiles it into the `avin-c-fixture` library beside avin_fixture.c. */
#ifndef AVIN_HANDLES_H
#define AVIN_HANDLES_H

#include <stddef.h>

/* The kinds of object counted, and what is counted of each, for `fx_count`. */
enum fx_kind { FX_OBJECT = 0, FX_LIBRARY = 1, FX_FACE = 2, FX_MMVAR = 3, FX_PLAIN = 4, FX_SHELF = 5, FX_SHAPE = 6, FX_NODE = 7, FX_CONN = 8, FX_TEXT = 9, FX_STREAM = 10 };
enum fx_what { FX_CREATED = 0, FX_RETAINED = 1, FX_RELEASED = 2, FX_FREED = 3, FX_AFTER = 4, FX_BEFORE = 5 };
/* For faces, FX_AFTER counts those freed once their library's other references had gone, the library
 * holding only the face's own, and FX_BEFORE those freed while it held more: which of the two a
 * collection gives back first. */
long fx_count(int kind, int what);

/* A counted object holding an integer: made with one reference, freed when the last is given back. */
typedef struct fx_object fx_object;
fx_object *fx_object_new(int value);
fx_object *fx_object_ref(fx_object *o);
void fx_object_unref(fx_object *o);
int fx_object_value(const fx_object *o);
void fx_object_set(fx_object *o, int value);
int fx_object_refs(const fx_object *o);
/* Null, where an object might have been answered. */
fx_object *fx_object_none(int value);
/* A status: zero where the object holds `want`, and the difference otherwise. */
int fx_object_check(fx_object *o, int want);
/* The object, lent: no reference is taken for the caller. */
fx_object *fx_object_lend(fx_object *o);

/* A library owning the faces made in it, which frees every face still in it when its last reference
 * goes, as a FreeType library does; a face given back after that is freed twice. */
typedef struct fx_library_rec *fx_library;
struct fx_face_rec {
    fx_library library;
    int id;
};
typedef struct fx_face_rec *fx_face;
fx_library fx_library_new(void);
int fx_library_reference(fx_library library);
int fx_library_done(fx_library library);
int fx_library_faces(fx_library library);
/* The library takes the reference it is passed, and gives it back when the library is freed. */
void fx_library_adopt(fx_library library, fx_object *o);
/* The object the library adopted last, lent; null where it has none. */
fx_object *fx_library_adopted(fx_library library);
/* A face made in `library`, which keeps a pointer to it and takes no reference. */
fx_face fx_face_new(fx_library library, int id);
/* A face made in `library` over `memory`, which it keeps a pointer to without a reference. */
fx_face fx_face_new_over(fx_library library, fx_object *memory, int id);
int fx_face_reference(fx_face face);
int fx_face_done(fx_face face);
int fx_face_id(fx_face face);
/* The library a face was made in, lent. */
fx_library fx_face_library(fx_face face);
/* The value of the memory a face was made over, read through the face. */
int fx_face_memory_value(fx_face face);

/* A variation descriptor of a face, freed through the face's library. */
typedef struct fx_mmvar fx_mmvar;
fx_mmvar *fx_mmvar_new(fx_face face);
void fx_mmvar_free(fx_library library, fx_mmvar *mm);
int fx_mmvar_axes(const fx_mmvar *mm);

/* An object with no count, freed once, and a shelf keeping a pointer to one, which it never reads
 * when it is freed itself. */
typedef struct fx_plain fx_plain;
fx_plain *fx_plain_new(int value);
void fx_plain_free(fx_plain *p);
int fx_plain_value(const fx_plain *p);
typedef struct fx_shelf fx_shelf;
fx_shelf *fx_shelf_new(fx_plain *p);
void fx_shelf_free(fx_shelf *s);
int fx_shelf_peek(const fx_shelf *s);

/* Statuses of each C integer shape. */
typedef int fx_status;
fx_status fx_status_of(int code);
enum fx_result { FX_RESULT_OK = 0, FX_RESULT_BAD = 3, FX_RESULT_WORSE = -2 };
enum fx_result fx_result_of(int which);
long long fx_long_status(long long code);
unsigned char fx_byte_status(unsigned char code);

/* A tree of nodes with no reference count, as libssh2's sessions and channels are: freeing a node
 * first frees each node still under it, as `libssh2_session_free` frees its channels, and a node
 * freed after its parent, or twice, stops the process, as a channel freed after its session would
 * be a use after free. For FX_NODE, FX_AFTER counts the nodes freed by their own free, their parent
 * still alive, and FX_BEFORE those freed by a parent's. */
typedef struct fx_node fx_node;
fx_node *fx_node_new(fx_node *parent);
void fx_node_free(fx_node *n);
int fx_node_depth(const fx_node *n);
/* The sum of two nodes' depths, which uses both. */
int fx_node_pair(const fx_node *a, const fx_node *b);
/* Answers strings allocated through the node's root, freed only by `fx_text_free` with that root, as
 * libssh2 answers strings freed with `libssh2_free(session, p)`: `which` 0 answers both and zero, 1
 * null for `name` and zero, 2 both and the status 7, 3 neither and the status 7. FX_TEXT counts the
 * strings made and freed; one freed twice, or with another root, stops the process. */
int fx_node_texts(fx_node *n, int which, char **name, size_t *name_len, char **other);
void fx_text_free(fx_node *root, void *p);
/* Frees a string by itself, whatever root made it. */
void fx_text_release(void *p);
fx_node *fx_node_root(fx_node *n);

/* A connection given a descriptor it does not own, as a libssh2 session is given its socket: freeing
 * it writes `bye` to the descriptor and closes it, as `libssh2_session_free` writes to its socket.
 * FX_CONN counts connections made and freed, FX_AFTER the farewells written and FX_BEFORE those that
 * failed, the descriptor closed already. */
typedef struct fx_conn fx_conn;
fx_conn *fx_conn_new(void);
int fx_conn_start(fx_conn *c, int fd);
/* A new connection, null where `fd` is negative. */
fx_conn *fx_conn_with(int fd);
void fx_conn_free(fx_conn *c, int fd);
/* A stream owning a descriptor, closed when the stream is freed, as an Avin `TcpStream` owns its
 * socket. */
typedef struct fx_stream fx_stream;
fx_stream *fx_stream_new(int fd);
void fx_stream_free(fx_stream *s);
int fx_stream_fd(const fx_stream *s);
/* A pipe: its read end, and its write end through `write_end`. */
int fx_pipe(int *write_end);
int fx_dup(int fd);
int fx_close(int fd);
/* The bytes read from `fd` until every write end is closed, as text; the read end is closed. */
char *fx_drain(int fd);

#endif
