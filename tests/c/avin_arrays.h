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
#include <sys/types.h>
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

/* Memory of no type, through `void *`: `n` bytes set to `c`; `n` bytes summed, each as an unsigned
 * byte; the two summed in one call; the first `*len` bytes of the table copied, or, for null, its
 * length asked, as `aa_take`; one more byte answered than it was told, which the binding must
 * refuse; and -1 for null with a length of zero, -2 for null with any other, or the length. */
void aa_void_fill(void *p, int c, size_t n);
long long aa_void_sum(const void *p, size_t n);
long long aa_void_sum_two(const void *a, size_t na, const void *b, int nb);
void aa_void_take(void *buf, size_t *len);
void aa_void_overrun(void *buf, size_t *len);
int aa_void_null(const void *p, size_t n);
/* Memory of no type through a pointer to an opaque pointer, as `proc_pid_rusage` takes it:
 * `aa_info_t` is `void *`, and `aa_info` writes a `struct aa_info_v1` through `buffer` (a 16-byte
 * tag of bytes 1 to 16, then the counts 7, 8 and `flavor`) and answers 0 for flavors 1 to 9, and
 * answers -1 with `errno` EINVAL for any other. `aa_counts` writes a `struct aa_counts` of 3, 4 and
 * `k` through `void *`, `aa_counts_pp` the same through `void **`. `aa_info_sum` and `aa_pp_sum` sum
 * `n` bytes, each as an unsigned byte, read through `aa_info_t *` and `void **`; `aa_info_fill` sets
 * `n` bytes to `c` through `aa_info_t *`. `aa_info_octets` writes `n` bytes of 1, 2, … through a pointer
 * to a typedef of an integer type, which is no memory of no type. */
typedef void *aa_info_t;
typedef const void *aa_const_info_t;
struct aa_info_v1 { uint8_t tag[16]; uint64_t a, b, flavor; };
struct aa_counts { int64_t a, b, c; };
int aa_info(int flavor, aa_info_t *buffer);
int aa_info_const(int flavor, aa_const_info_t *buffer);
int aa_counts(void *out, int k);
int aa_counts_pp(void **out, int k);
/* The same counts through `aa_info_t *`; and one `aa_fixed` of `k` through `aa_fixed *`, a pointer to a
 * typedef that is neither the struct nor `void *`. */
int aa_info_counts(aa_info_t *buffer, int k);
int aa_fixed_out(aa_fixed *p, int k);
long long aa_info_sum(aa_info_t *p, size_t n);
long long aa_pp_sum(void **p, size_t n);
void aa_info_fill(aa_info_t *p, int c, size_t n);
int aa_info_octets(aa_octet *p, int n);
/* Memory of no type C allocates: `n` bytes of 1, 2, …, answered through `void **`, its length
 * through `len`, freed with `free`. */
int aa_alloc_bytes(void **out, int n, int *len);

/* Failure through `errno`: -1 with `errno` set to `code` where `code` is not zero, and `value`
 * otherwise, as an `int`, a `long` and an `ssize_t`; one that writes `value` through an
 * out pointer, and -1 with `errno` set to `code` and nothing written where `code` is not zero. */
int aa_errno(int code, int value);
long aa_errno_long(int code, long value);
ssize_t aa_errno_ssize(int code, ssize_t value);
/* The same through a `size_t`, whose -1 is its greatest value. */
size_t aa_errno_size(int code, size_t value);
int aa_errno_out(int code, int value, int *out);
/* Sets `errno` to `code` and answers `value`, whatever it is: a function that sets `errno` and
 * answers no -1 has not failed. */
int aa_errno_set(int code, int value);

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
/* Waits for the flag, then reads three objects, each of which must still be alive: the sum of
 * their values, or -1. */
int aa_wait_objects(const fx_object *a, const fx_object *b, const fx_object *c, int timeout_ms);
/* Waits for the flag, then reads the object, which must still be alive where it is not null: its
 * value, -2 for null, or -1. */
int aa_wait_maybe(const fx_object *o, int timeout_ms);
/* Sleeps, then answers the string's length. */
size_t aa_sleep_strlen(const char *s, int ms);
/* Sleeps, then answers how much of the table it copied, as `aa_take`. */
int aa_sleep_take(uint8_t *buf, size_t *len, int ms);

#endif
