/* The C objects the buffer tests bind: a counted frame owning bytes it lends, which it can replace
 * (freeing the old ones, as MLT converts a frame's image in place), so a view used after its frame
 * was used again would read freed memory; functions over bytes Avin passes; and a surface made over
 * memory Avin gives C, which keeps it, holds user data with a destroy function as Cairo's surfaces
 * do, and may be held by a holder after its Avin handle has gone. The test harness compiles it into
 * the `avin-c-fixture` library beside avin_fixture.c. */
#ifndef AVIN_BUFFERS_H
#define AVIN_BUFFERS_H

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

#endif
