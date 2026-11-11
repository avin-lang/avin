/* The buffer tests' C objects; see avin_buffers.h. A dead frame or surface stops the process at its
 * next use, so a binding that keeps too little alive fails loudly. */
#include "avin_buffers.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct bf_frame {
    int refs;
    int size;
    unsigned char *bytes;
    int dead;
};

static atomic_int frames_alive;
static atomic_int surfaces_alive;
static atomic_int destroys;

static void die(const char *what) {
    fprintf(stderr, "avin_buffers: %s\n", what);
    abort();
}

static unsigned char *made(int size, int fill) {
    unsigned char *b = malloc(size > 0 ? (size_t)size : 1);
    if (!b) die("out of memory");
    for (int i = 0; i < size; i++) b[i] = (unsigned char)((fill + i) & 255);
    return b;
}

bf_frame *bf_frame_new(int size, int fill) {
    bf_frame *f = malloc(sizeof *f);
    if (!f) die("out of memory");
    f->refs = 1;
    f->size = size;
    f->bytes = made(size, fill);
    f->dead = 0;
    atomic_fetch_add(&frames_alive, 1);
    return f;
}

static bf_frame *live(bf_frame *f) {
    if (!f || f->dead) die("a frame used after it was freed");
    return f;
}

bf_frame *bf_frame_ref(bf_frame *f) {
    live(f)->refs++;
    return f;
}

void bf_frame_unref(bf_frame *f) {
    if (--live(f)->refs > 0) return;
    free(f->bytes);
    f->bytes = 0;
    f->dead = 1;
    atomic_fetch_sub(&frames_alive, 1);
}

int bf_frames_alive(void) { return atomic_load(&frames_alive); }
int bf_frame_size(bf_frame *f) { return live(f)->size; }
unsigned char *bf_frame_data(bf_frame *f) { return live(f)->bytes; }
const unsigned char *bf_frame_cdata(bf_frame *f) { return live(f)->bytes; }
uint8_t *bf_frame_bytes(bf_frame *f) { return live(f)->bytes; }

int bf_frame_image(bf_frame *f, int fail, uint8_t **buffer, int *width, int *height) {
    live(f);
    if (fail) return fail;
    *buffer = f->bytes;
    *width = f->size > 0 ? f->size : 0;
    *height = f->size > 0 ? 1 : 0;
    return 0;
}

void bf_frame_replace(bf_frame *f, int size, int fill) {
    live(f);
    free(f->bytes);
    f->bytes = made(size, fill);
    f->size = size;
}

long bf_frame_sum(bf_frame *f) {
    long s = 0;
    for (int i = 0; i < live(f)->size; i++) s += f->bytes[i];
    return s;
}

int bf_frame_at(bf_frame *f, int i) { return live(f)->bytes[i]; }
unsigned char *bf_frame_null(bf_frame *f) { live(f); return 0; }

static unsigned char one_byte = 7;
unsigned char *bf_frame_static(bf_frame *f) { live(f); return &one_byte; }

unsigned char *bf_frame_copy(bf_frame *f, int none, int *n) {
    live(f);
    if (none) {
        *n = 0;
        return 0;
    }
    unsigned char *c = malloc(f->size > 0 ? (size_t)f->size : 1);
    if (!c) die("out of memory");
    memcpy(c, f->bytes, (size_t)f->size);
    *n = f->size;
    return c;
}

void bf_frame_copy_out(bf_frame *f, int none, unsigned char **bytes, int *n) { *bytes = bf_frame_copy(f, none, n); }

int *bf_frame_ints(bf_frame *f) { return (int *)live(f)->bytes; }

long bf_sum_ints(const int *data, int n) {
    long s = 0;
    for (int i = 0; i < n; i++) s += data[i];
    return s;
}

long bf_sum(const unsigned char *data, int n) {
    long s = 0;
    for (int i = 0; i < n; i++) s += data[i];
    return s;
}

void bf_fill(unsigned char *data, int n, unsigned char value) {
    for (int i = 0; i < n; i++) data[i] = value;
}

int bf_peek(const uint8_t *data, int i) { return data[i]; }
int bf_is_null(const unsigned char *data) { return data == 0; }

void bf_fill_slowly(unsigned char *data, int n, unsigned char value, int ms) {
    struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&t, 0);
    for (int i = 0; i < n; i++) data[i] = value;
}

struct bf_surface {
    int refs;
    unsigned char *data;
    int n;
    const bf_user_data_key_t *key;
    void *user;
    void (*destroy)(void *);
    int dead;
    int status;
};

bf_surface *bf_surface_new(unsigned char *data, int n) {
    bf_surface *s = calloc(1, sizeof *s);
    if (!s) die("out of memory");
    s->refs = 1;
    s->data = data;
    s->n = n;
    atomic_fetch_add(&surfaces_alive, 1);
    return s;
}

static bf_surface *live_surface(bf_surface *s) {
    if (!s || s->dead) die("a surface used after it was freed");
    return s;
}

bf_surface *bf_surface_ref(bf_surface *s) {
    live_surface(s)->refs++;
    return s;
}

static void destroyed(bf_surface *s) {
    if (s->destroy) {
        atomic_fetch_add(&destroys, 1);
        s->destroy(s->user);
    }
    s->destroy = 0;
    s->user = 0;
}

void bf_surface_unref(bf_surface *s) {
    if (--live_surface(s)->refs > 0) return;
    destroyed(s);
    s->dead = 1;
    atomic_fetch_sub(&surfaces_alive, 1);
}

int bf_surface_set_user_data(bf_surface *s, const bf_user_data_key_t *key, void *data, void (*destroy)(void *)) {
    live_surface(s);
    if (s->key == key) destroyed(s);
    s->key = key;
    s->user = data;
    s->destroy = destroy;
    return 0;
}

bf_surface *bf_surface_new_checked(unsigned char *data, int n, int stride) {
    bf_surface *s = bf_surface_new(data, n);
    if (stride <= 0) s->status = stride == 0 ? BF_INVALID_STRIDE : -stride;
    return s;
}

bf_status bf_surface_set_user_data_checked(bf_surface *s, const bf_user_data_key_t *key, void *data, void (*destroy)(void *)) {
    if (live_surface(s)->status) return (bf_status)s->status;
    return (bf_status)bf_surface_set_user_data(s, key, data, destroy);
}

int bf_surface_refuse_user_data(bf_surface *s, const bf_user_data_key_t *key, void *data, void (*destroy)(void *)) {
    live_surface(s);
    (void)key;
    (void)data;
    (void)destroy;
    return 1;
}

long bf_surface_sum(bf_surface *s) { return bf_sum(live_surface(s)->data, s->n); }
void bf_surface_paint(bf_surface *s, unsigned char value) { bf_fill(live_surface(s)->data, s->n, value); }
long bf_surface_also(bf_surface *s, const unsigned char *other, int n) {
    live_surface(s);
    return bf_sum(other, n);
}

int bf_destroys(void) { return atomic_load(&destroys); }
int bf_surfaces_alive(void) { return atomic_load(&surfaces_alive); }

struct bf_holder {
    bf_surface *s;
};

bf_holder *bf_holder_new(bf_surface *s) {
    bf_holder *h = malloc(sizeof *h);
    if (!h) die("out of memory");
    h->s = bf_surface_ref(s);
    return h;
}

void bf_holder_free(bf_holder *h) {
    bf_surface_unref(h->s);
    free(h);
}

long bf_holder_sum(bf_holder *h) { return bf_surface_sum(h->s); }
void bf_holder_paint(bf_holder *h, unsigned char value) { bf_surface_paint(h->s, value); }

static atomic_int faces_alive;
static const unsigned char face_name[] = "face";

bf_face *bf_face_new(void) {
    bf_face *f = calloc(1, sizeof *f);
    if (!f) die("out of memory");
    f->refs = 1;
    f->glyph = calloc(1, sizeof *f->glyph);
    if (!f->glyph) die("out of memory");
    f->name = face_name;
    f->name_length = 4;
    f->plane_length = 3;
    for (int p = 0; p < 2; p++) f->planes[p] = made(3, p * 10);
    atomic_fetch_add(&faces_alive, 1);
    return f;
}

static bf_face *live_face(bf_face *f) {
    if (!f || f->dead) die("a face used after it was freed");
    return f;
}

bf_face *bf_face_ref(bf_face *f) {
    live_face(f)->refs++;
    return f;
}

void bf_face_unref(bf_face *f) {
    if (--live_face(f)->refs > 0) return;
    free(f->glyph->bitmap.buffer);
    free(f->glyph);
    for (int p = 0; p < 2; p++) free(f->planes[p]);
    f->glyph = 0;
    f->dead = 1;
    atomic_fetch_sub(&faces_alive, 1);
}

int bf_faces_alive(void) { return atomic_load(&faces_alive); }

void bf_face_load(bf_face *f, int rows, int width, int pitch, int fill) {
    bf_bitmap *b = &live_face(f)->glyph->bitmap;
    /* The old pixels are overwritten before they are freed, so a view still reading them sees it. */
    if (b->buffer) memset(b->buffer, 0xDD, (size_t)(b->rows * (b->pitch < 0 ? -b->pitch : b->pitch)));
    free(b->buffer);
    int stride = pitch < 0 ? -pitch : pitch;
    b->rows = rows;
    b->width = width;
    b->pitch = pitch;
    b->buffer = 0;
    f->loads++;
    if (rows == 0 || width == 0) return;
    b->buffer = malloc((size_t)(rows * stride));
    if (!b->buffer) die("out of memory");
    memset(b->buffer, 0, (size_t)(rows * stride));
    for (int r = 0; r < rows; r++) {
        /* Bottom row first where the pitch is negative. */
        int at = pitch < 0 ? (rows - 1 - r) * stride : r * stride;
        for (int c = 0; c < width; c++) b->buffer[at + c] = (unsigned char)((r * 16 + c + fill) & 255);
    }
}

long bf_face_sum(bf_face *f) {
    bf_bitmap *b = &live_face(f)->glyph->bitmap;
    long sum = 0;
    int stride = b->pitch < 0 ? -b->pitch : b->pitch;
    for (int i = 0; b->buffer && i < b->rows * stride; i++) sum += b->buffer[i];
    return sum;
}

int bf_stream_write(int total, int chunk, int (*write)(void *closure, const unsigned char *data, unsigned int length), void *closure) {
    for (int at = 0; at < total; at += chunk) {
        int n = total - at < chunk ? total - at : chunk;
        unsigned char *piece = malloc((size_t)n + 1);
        if (!piece) die("out of memory");
        for (int i = 0; i < n; i++) piece[i] = (unsigned char)((at + i) & 255);
        int answer = write(closure, piece, (unsigned int)n);
        memset(piece, 0xDD, (size_t)n);
        free(piece);
        if (answer != 0) return answer;
    }
    return 0;
}

long bf_stream_read(int total, int chunk, int (*read)(void *closure, unsigned char *data, unsigned int length), void *closure) {
    long sum = 0;
    for (int at = 0; at < total; at += chunk) {
        int n = total - at < chunk ? total - at : chunk;
        unsigned char *piece = malloc((size_t)n + 1);
        if (!piece) die("out of memory");
        memset(piece, 0xEE, (size_t)n);
        int answer = read(closure, piece, (unsigned int)n);
        for (int i = 0; i < n; i++) sum += piece[i];
        free(piece);
        if (answer != 0) return -answer;
    }
    return sum;
}

int bf_stream_null(unsigned int length, int (*write)(void *closure, const unsigned char *data, unsigned int length), void *closure) {
    return write(closure, 0, length);
}

void bf_stream_sized(size_t n, void (*seen)(void *closure, size_t length, const uint8_t *data), void *closure) {
    uint8_t *piece = malloc(n + 1);
    if (!piece) die("out of memory");
    for (size_t i = 0; i < n; i++) piece[i] = (uint8_t)(i & 255);
    seen(closure, n, piece);
    free(piece);
}

void bf_stream_signed(int n, void (*seen)(void *closure, const unsigned char *data, int length), void *closure) {
    static const unsigned char four[4] = {1, 2, 3, 4};
    seen(closure, four, n);
}

long bf_stream_pair(int a, int b, long (*seen)(void *closure, const unsigned char *first, unsigned int n, unsigned char *second, unsigned int m), void *closure) {
    unsigned char *x = made(a, 0);
    unsigned char *y = made(b, 100);
    long answer = seen(closure, x, (unsigned int)a, y, (unsigned int)b);
    for (int i = 0; i < b; i++) answer += y[i];
    free(x);
    free(y);
    return answer;
}

void bf_stream_ints(int n, void (*seen)(void *closure, const int *data, int length), void *closure) {
    int values[4] = {1, 2, 3, 4};
    seen(closure, values, n);
}
