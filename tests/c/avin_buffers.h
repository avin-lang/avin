/* The C objects the buffer tests bind: a counted frame owning bytes it lends, which it can replace
 * (freeing the old ones, as MLT converts a frame's image in place), so a view used after its frame
 * was used again would read freed memory; functions over bytes Avin passes; and a surface made over
 * memory Avin gives C, which keeps it, holds user data with a destroy function as Cairo's surfaces
 * do, and may be held by a holder after its Avin handle has gone. The test harness compiles it into
 * the `avin-c-fixture` library beside avin_fixture.c. */
#ifndef AVIN_BUFFERS_H
#define AVIN_BUFFERS_H

#include <stddef.h>
#include <stdint.h>

/* A frame of `size` bytes, the byte at `i` being `(fill + i) & 255`; one reference. */
typedef struct bf_frame bf_frame;
bf_frame *bf_frame_new(int size, int fill);
bf_frame *bf_frame_ref(bf_frame *f);
void bf_frame_unref(bf_frame *f);
/* How many frames are alive. */
int bf_frames_alive(void);
int bf_frame_size(bf_frame *f);
/* The frame's bytes, lent; writable, and as `const`. */
unsigned char *bf_frame_data(bf_frame *f);
const unsigned char *bf_frame_cdata(bf_frame *f);
uint8_t *bf_frame_bytes(bf_frame *f);
/* The bytes through an out pointer, with a width and height whose product is the size, and a status:
 * zero, or `fail` where `fail` is not zero, writing nothing. */
int bf_frame_image(bf_frame *f, int fail, uint8_t **buffer, int *width, int *height);
/* Frees the bytes and makes `size` new ones from `fill`. */
void bf_frame_replace(bf_frame *f, int size, int fill);
/* The sum of the bytes, and the byte at `i`, read by C. */
long bf_frame_sum(bf_frame *f);
int bf_frame_at(bf_frame *f, int i);
/* Null, where bytes might have been lent. */
unsigned char *bf_frame_null(bf_frame *f);
/* A pointer to one static byte, 7, which a binding may claim is longer; only its first byte is read. */
unsigned char *bf_frame_static(bf_frame *f);
/* A copy of the bytes, allocated with malloc, which the caller frees; null for `none`. */
unsigned char *bf_frame_copy(bf_frame *f, int none, int *n);
/* As `bf_frame_copy`, through an out pointer. */
void bf_frame_copy_out(bf_frame *f, int none, unsigned char **bytes, int *n);

/* The frame's bytes as `int`s, which no view lends. */
int *bf_frame_ints(bf_frame *f);

/* Bytes Avin passes: their sum, and filling them with `value`. */
long bf_sum(const unsigned char *data, int n);
void bf_fill(unsigned char *data, int n, unsigned char value);
/* The sum of `n` `int`s, which a block is not passed to. */
long bf_sum_ints(const int *data, int n);
/* The byte at `i` of `data`, with no length. */
int bf_peek(const uint8_t *data, int i);
/* Whether `data` is null. */
int bf_is_null(const unsigned char *data);
/* Fills `data` with `value` while waiting `ms` milliseconds, so a collection runs meanwhile. */
void bf_fill_slowly(unsigned char *data, int n, unsigned char value, int ms);

/* A surface over `n` bytes at `data`, which it keeps and reads and writes for its life; one
 * reference. Its user data is set with `bf_surface_set_user_data`, whose destroy function it calls
 * when its last reference goes or the data is replaced. */
typedef struct bf_surface bf_surface;
typedef struct { int unused; } bf_user_data_key_t;
bf_surface *bf_surface_new(unsigned char *data, int n);
bf_surface *bf_surface_ref(bf_surface *s);
void bf_surface_unref(bf_surface *s);
int bf_surface_set_user_data(bf_surface *s, const bf_user_data_key_t *key, void *data, void (*destroy)(void *));
/* As `bf_surface_set_user_data`, but failing with 1, setting nothing. */
int bf_surface_refuse_user_data(bf_surface *s, const bf_user_data_key_t *key, void *data, void (*destroy)(void *));
/* Reads and writes the surface's bytes. */
long bf_surface_sum(bf_surface *s);
void bf_surface_paint(bf_surface *s, unsigned char value);
/* The sum of `n` bytes at `other`, which the surface is said to keep too. */
long bf_surface_also(bf_surface *s, const unsigned char *other, int n);
/* How many times a surface's destroy function has run, and how many surfaces are alive. */
int bf_destroys(void);
int bf_surfaces_alive(void);

/* A holder of one surface, taking a reference of its own, as a Cairo context does its target. */
typedef struct bf_holder bf_holder;
bf_holder *bf_holder_new(bf_surface *s);
void bf_holder_free(bf_holder *h);
long bf_holder_sum(bf_holder *h);
void bf_holder_paint(bf_holder *h, unsigned char value);

/* A face shaped as a FreeType face is: its glyph slot holds a bitmap of `rows` rows `pitch` bytes apart,
 * negative for one stored bottom row first, whose buffer the next load frees and replaces, null for a
 * glyph with no pixels. The byte at row `r` and column `c` of a loaded glyph is `(r * 16 + c + fill) &
 * 255`, row 0 being the top. Its fields are published, so field accessors read them; one reference. */
typedef struct {
    int rows;
    int width;
    int pitch;
    unsigned char *buffer;
} bf_bitmap;
typedef struct {
    bf_bitmap bitmap;
} bf_glyph_slot_rec;
typedef struct bf_face {
    int refs;
    int dead;
    int loads;
    bf_glyph_slot_rec *glyph;
    const unsigned char *name;
    int name_length;
    int *ints;
    unsigned char *planes[2];
    int plane_length;
} bf_face;
bf_face *bf_face_new(void);
bf_face *bf_face_ref(bf_face *f);
void bf_face_unref(bf_face *f);
int bf_faces_alive(void);
/* Frees the glyph's buffer and loads one of `rows` rows of `width` bytes, `pitch` apart (its magnitude
 * at least `width`), null where `rows` or `width` is zero. */
void bf_face_load(bf_face *f, int rows, int width, int pitch, int fill);
/* The sum of the loaded glyph's pixels, read by C. */
long bf_face_sum(bf_face *f);

/* Calls `write` with `total` bytes, the byte at `i` being `i & 255`, `chunk` at a time (the last perhaps
 * shorter), stopping at an answer other than zero, which it answers; zero once all are written. Each
 * chunk is freed after its call, so a view kept past it would read freed memory. */
int bf_stream_write(int total, int chunk, int (*write)(void *closure, const unsigned char *data, unsigned int length), void *closure);
/* Calls `read` for `total` bytes, `chunk` at a time, each into fresh memory set to 0xEE, freed after its
 * call, and answers the sum of the bytes read, or minus the first answer other than zero. */
long bf_stream_read(int total, int chunk, int (*read)(void *closure, unsigned char *data, unsigned int length), void *closure);
/* Calls `write` once with null and `length`. */
int bf_stream_null(unsigned int length, int (*write)(void *closure, const unsigned char *data, unsigned int length), void *closure);
/* Calls `seen` once with `n` bytes, `i & 255`, the length first and its type `size_t`. */
void bf_stream_sized(size_t n, void (*seen)(void *closure, size_t length, const uint8_t *data), void *closure);
/* Calls `seen` with a signed length `n` over 4 bytes, whatever `n` is. */
void bf_stream_signed(int n, void (*seen)(void *closure, const unsigned char *data, int length), void *closure);
/* Calls `seen` with two lent pieces of `a` and `b` bytes. */
long bf_stream_pair(int a, int b, long (*seen)(void *closure, const unsigned char *first, unsigned int n, unsigned char *second, unsigned int m), void *closure);
/* Calls `seen` with the bytes as `int`s, which no view lends. */
void bf_stream_ints(int n, void (*seen)(void *closure, const int *data, int length), void *closure);

#endif
