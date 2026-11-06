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
