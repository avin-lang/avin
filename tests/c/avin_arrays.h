/* The arrays and blocking calls the C binding tests bind: pointers to the elements of every scalar
 * kind an array crosses as, read and written, with their lengths in every integer type and in every
 * position, two arrays in one call, `inout` lengths C answers how much it used through, element
 * types an array cannot cross as, and calls that wait for a flag another thread sets, so a
 * collection must run while they wait. The test harness compiles it into the `avin-c-fixture`
 * library beside avin_fixture.c. */
#ifndef AVIN_ARRAYS_H
#define AVIN_ARRAYS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "avin_handles.h"

typedef uint8_t aa_octet;
typedef int64_t aa_fixed;
typedef double aa_real;
typedef unsigned long aa_size;

/* Sums of the elements, read through `const` pointers. */
long long aa_sum_bytes(const uint8_t *p, size_t n);
long long aa_sum_chars(const char *p, int n);
long long aa_sum_uchars(const unsigned char *p, int n);
long long aa_sum_octets(const aa_octet *p, aa_size n);
long long aa_sum_ints(const int32_t *p, int n);
long long aa_sum_plain_ints(const int *p, unsigned n);
long long aa_sum_longs(const int64_t *p, long n);
long long aa_sum_fixed(const aa_fixed *p, long long n);
double aa_sum_reals(const double *p, unsigned n);
double aa_sum_aa_reals(const aa_real *p, size_t n);

/* Writers through pointers that are not `const`: what each writes is in the array after the call. */
void aa_fill_bytes(uint8_t *p, size_t n, int start);
void aa_fill_chars(char *p, size_t n);
void aa_scale_ints(int *p, size_t n, int k);
void aa_negate_longs(long long *p, size_t n);
void aa_halve_reals(double *p, size_t n);
/* A writer that only reads where the binding says `const`. */
long long aa_sum_mutable(int *p, size_t n);

/* Lengths: before the array, in narrow and wide types, and two arrays in one call. */
int aa_count_first(int n, const int *p);
int aa_count_uchar(const uint8_t *p, unsigned char n);
int aa_count_schar(const uint8_t *p, signed char n);
int aa_count_short(const uint8_t *p, short n);
unsigned long long aa_count_u64(const uint8_t *p, uint64_t n);
long long aa_count_i64(const uint8_t *p, int64_t n);
/* 1 where `p` is null, then the length times two. */
int aa_null_length(const uint8_t *p, size_t n);
/* The sum of the products of the elements both arrays have, and the difference of their lengths. */
double aa_dot(const double *a, int na, const double *b, int nb, int *difference);
double aa_dot_plain(const double *a, int na, const double *b, int nb);
/* The last element, or -1 for an empty array. */
long long aa_last(const int64_t *p, size_t n);

/* `inout` lengths: a table of 54 bytes copied into as much of `buf` as `*len` says, `*len` set to how
 * much was copied, or, for a null `buf`, to the table's whole length. */
void aa_take(uint8_t *buf, size_t *len);
/* The same with a status: 0, or 142 for any tag but 1, the length then untouched. */
int aa_load(long tag, long offset, uint8_t *buf, unsigned long *len);
/* Answers one more than it was told, which the binding must refuse. */
void aa_overrun(uint8_t *buf, size_t *len);
/* Squares into the first half of `buf`, and answers that half's length, through an `int`. */
void aa_squares(int *buf, int *len);
/* Answers a length an `int` cannot hold. */
void aa_huge(uint8_t *buf, uint64_t *len);

/* Element types no array crosses as. */
long long aa_sum_uints(const unsigned int *p, size_t n);
long long aa_sum_ulongs(const uint64_t *p, size_t n);
double aa_sum_floats(const float *p, size_t n);
long long aa_sum_shorts(const short *p, size_t n);
int aa_sum_bools(const bool *p, size_t n);
long long aa_sum_pointers(const uint8_t **p, size_t n);

/* Calls that wait. The flag is set by `aa_set_flag`, from another thread, and cleared by
 * `aa_clear_flag`; each wait gives up after `timeout_ms`. */
void aa_set_flag(void);
void aa_clear_flag(void);
int aa_wait_flag(int timeout_ms);
/* Waits for the flag, then sums: -1 where it gave up. */
long long aa_wait_sum(const int32_t *p, size_t n, int timeout_ms);
/* Waits for the flag, then writes `p[i] = i`: 1, or 0 where it gave up and wrote nothing. */
int aa_wait_fill(uint8_t *p, size_t n, int timeout_ms);
/* Waits for the flag, then reads the object, which must still be alive: its value, or -1. */
int aa_wait_object(const fx_object *o, int timeout_ms);
/* Sleeps, then answers the string's length. */
size_t aa_sleep_strlen(const char *s, int ms);
/* Sleeps, then answers how much of the table it copied, as `aa_take`. */
int aa_sleep_take(uint8_t *buf, size_t *len, int ms);

#endif
