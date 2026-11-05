/* The enums, constants and out parameters the C binding tests bind: enumerations of every shape a
 * mirror meets (negative, sparse, duplicated and extreme values, one enumerator, an unsigned and a
 * `long long` enumeration, upper- and lower-case names, names that start with a digit after their
 * prefix, two enumerations sharing a word), constants of every kind, and functions answering through
 * `out` and `inout` pointers of every kind. The test harness compiles it into the `avin-c-fixture`
 * library beside avin_fixture.c. */
#ifndef AVIN_ENUMS_H
#define AVIN_ENUMS_H

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "avin_handles.h"

/* Upper-case enumerators, a negative and a sparse value, and one whose words end in a digit run. */
typedef enum {
    AE_FORMAT_INVALID = -1,
    AE_FORMAT_ARGB32 = 0,
    AE_FORMAT_RGB24 = 1,
    AE_FORMAT_A8 = 2,
    AE_FORMAT_RGB16_565 = 4,
    AE_FORMAT_RGB30 = 5
} ae_format_t;

/* Lower-case enumerators of a tagged enumeration, one given its value. */
enum ae_image_format { ae_image_none, ae_image_rgb, ae_image_rgba, ae_image_yuv422, ae_image_yuv420p = 7 };

/* Enumerators whose names start with a digit after the prefix. */
typedef enum { AE_PDF_VERSION_1_4, AE_PDF_VERSION_1_5 } ae_pdf_version_t;

/* The least and greatest `int`. */
typedef enum { AE_LIMIT_MIN = INT_MIN, AE_LIMIT_ZERO = 0, AE_LIMIT_MAX = INT_MAX } ae_limit_t;

/* Two enumerators with one value. */
enum ae_alias { AE_ALIAS_FIRST = 3, AE_ALIAS_SAME = 3, AE_ALIAS_OTHER = 9 };

/* One enumerator. */
typedef enum { AE_ONLY_ONE = 42 } ae_only_t;

/* Two enumerations sharing a word, as Cairo's slant and weight share `NORMAL`. */
typedef enum { AE_SLANT_NORMAL, AE_SLANT_ITALIC, AE_SLANT_OBLIQUE } ae_slant_t;
typedef enum { AE_WEIGHT_NORMAL, AE_WEIGHT_BOLD } ae_weight_t;

/* An enumeration clang makes unsigned, with the greatest `unsigned int`. */
typedef enum { AE_BIG_LOW = 0, AE_BIG_HIGH = 0xFFFFFFFFu } ae_big_t;

/* An enumeration as wide as `long long`, at its least and greatest. */
typedef enum { AE_WIDE_MIN = LLONG_MIN, AE_WIDE_ZERO = 0, AE_WIDE_MAX = LLONG_MAX } ae_wide_t;

/* A status, zero for success. */
typedef enum { AE_STATUS_SUCCESS = 0, AE_STATUS_NO_MEMORY = 1, AE_STATUS_INVALID = 2, AE_STATUS_UNLISTED = 3 } ae_status_t;

/* Not a type at all, for the checks. */
#define AE_NOT_A_TYPE 3
typedef double ae_real_t;

/* Constants of every kind. */
#define AE_INT_CONST 42
#define AE_NEG_CONST (-7)
#define AE_INT_MIN_CONST INT_MIN
#define AE_INT_MAX_CONST INT_MAX
#define AE_LONG_MIN_CONST LLONG_MIN
#define AE_LONG_MAX_CONST LLONG_MAX
#define AE_TOO_BIG_CONST ULLONG_MAX
#define AE_SHORT_MAX_CONST 32767
#define AE_EXPR_CONST (1 << 20)
#define AE_CHAR_CONST 'A'
#define AE_REAL_CONST 1.5
#define AE_REAL_INT_CONST 3
#define AE_FLOAT_CONST 0.1f
#define AE_NAN_CONST NAN
#define AE_INF_CONST INFINITY
#define AE_NEG_INF_CONST (-INFINITY)
#define AE_NEG_ZERO_CONST (-0.0)
#define AE_TINY_CONST 4.9406564584124654e-324
#define AE_STRING_CONST "cairo.dest"
#define AE_EMPTY_CONST ""
#define AE_UNICODE_CONST "h\xc3\xa9llo \xe2\x9c\x93 \xf0\x9d\x84\x9e"
#define AE_CONCAT_CONST "two " "parts"
#define AE_BAD_UTF8_CONST "\xff"
static const int ae_static_const = 11;
static const char *const ae_string_pointer = "not a literal";

/* Each enumeration echoed, and its value as an `int` or a `long long`, and the enumerator holding a
 * value, which may be one no mirror lists. */
ae_format_t ae_format_echo(ae_format_t f);
int ae_format_code(ae_format_t f);
ae_format_t ae_format_of(int code);
const ae_format_t ae_format_const(void);
enum ae_image_format ae_image_echo(enum ae_image_format f);
int ae_image_code(enum ae_image_format f);
ae_pdf_version_t ae_pdf_echo(ae_pdf_version_t v);
ae_limit_t ae_limit_echo(ae_limit_t v);
int ae_limit_code(ae_limit_t v);
enum ae_alias ae_alias_echo(enum ae_alias a);
int ae_alias_code(enum ae_alias a);
ae_only_t ae_only_echo(ae_only_t o);
ae_slant_t ae_slant_echo(ae_slant_t s);
ae_weight_t ae_weight_echo(ae_weight_t w);
ae_big_t ae_big_echo(ae_big_t b);
long long ae_big_code(ae_big_t b);
ae_wide_t ae_wide_echo(ae_wide_t w);
long long ae_wide_code(ae_wide_t w);
ae_wide_t ae_wide_of(long long code);
ae_status_t ae_status_of(int code);

/* `out` values of every scalar kind: `which` 0 gives each type's least value, 1 its greatest. */
void ae_signed_out(int which, signed char *a, short *b, int *c, long long *d);
void ae_unsigned_out(int which, unsigned char *a, unsigned short *b, unsigned int *c, unsigned long long *d);
void ae_sizes_out(int which, size_t *a, int8_t *b, uint16_t *c, int64_t *d);
/* `which` 0 gives 1.5 and 0.25, 1 NaN, 2 infinity and minus infinity, 3 minus zero and the least
 * subnormals, 4 the greatest finite values. */
void ae_reals_out(int which, double *a, float *b, ae_real_t *c);
/* A truth value as a `bool` and as an `int`, which is `value` itself. */
void ae_truths_out(int value, bool *a, int *b);
void ae_byte_out(int value, uint8_t *b);
/* A C result followed by two out values: the sum, the lesser and the greater. */
int ae_minmax(int a, int b, int *lo, int *hi);
/* A sum written through one out value, the C result `void`. */
void ae_sum_out(int a, int b, long long *sum);
/* Nothing written: an out value C leaves alone is zero. */
void ae_leave_out(int *a, double *b, const char **c, fx_object **d);

/* `inout` values of every scalar kind: each read and written back changed. */
void ae_signed_inout(signed char *a, short *b, int *c, long long *d);
void ae_unsigned_inout(unsigned char *a, unsigned int *b, unsigned long long *c);
void ae_reals_inout(double *a, float *b);
void ae_truths_inout(bool *a, int *b);
/* A point scaled by a half, as Cairo's `device_to_user` moves one. */
void ae_device_to_user(double *x, double *y);
/* A count told in and answered smaller: the bytes of `text` before its first space. */
void ae_prefix_length(const char *text, size_t *length);

/* Enumerations through pointers. */
void ae_format_out(int code, ae_format_t *f);
void ae_format_next(ae_format_t *f);
ae_format_t ae_parse_format(const char *text, int *consumed);

/* Strings through pointers: `which` 0 gives "argb", 1 "", 2 the Unicode text, 3 null, 4 a string that
 * is not UTF-8. */
void ae_name_out(int which, const char **name);
void ae_two_names(const char **first, char **second);
/* `text` moved past its leading spaces, which points it into the string it was passed. */
void ae_skip_spaces(const char **text);
/* `text` replaced by a string of C's own, or null where it starts with 'n'. */
void ae_replace(const char **text);

/* Statuses with out values. */
ae_status_t ae_divide(int a, int b, int *quotient, int *remainder);
ae_status_t ae_status_name(int code, const char **name);
int ae_checked_half(int value, int *half);
ae_status_t ae_status_format(int code, ae_format_t *f);
ae_status_t ae_status_nothing(int code);
/* A status whose out values are written even when it fails, which the binding never reads. */
int ae_failing_write(int *value);

/* Handles through pointers: an object made, null where `value` is 0, lent, and two made at once. */
int ae_object_make(int value, fx_object **object);
void ae_object_maybe(int value, fx_object **object);
void ae_object_lent(fx_object *of, fx_object **object);
ae_status_t ae_object_pair(int a, int b, fx_object **first, fx_object **second);
/* A face made in a library, which keeps a pointer to it, answered through a pointer. */
int ae_face_make(fx_library library, int id, fx_face *face);
/* A method's receiver first, then out values: the object's value, its tens and its units; and a
 * status answering half the value where it is even. */
int ae_object_digits(fx_object *self, int *tens, int *units);
ae_status_t ae_object_half(fx_object *self, int *half);

#endif
