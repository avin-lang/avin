/* The C library the C binding tests call: for every C scalar type, a function answering its argument
 * as it was given, its least and greatest values, and the argument widened to `long long`, which shows
 * whether a narrow argument arrived extended as its sign says. The test harness compiles it into a
 * library and names it to `pkg-config` as `avin-c-fixture`. */
#ifndef AVIN_FIXTURE_H
#define AVIN_FIXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* An enumeration with no negative value, which clang makes unsigned, and one with a negative value. */
typedef enum { FIXTURE_RED = 1, FIXTURE_GREEN = 2, FIXTURE_BLUE = 300 } fixture_colour;
enum fixture_tag { FIXTURE_LOW = -1, FIXTURE_HIGH = 1 };

typedef uint8_t fixture_octet;
typedef char fixture_letter;
typedef double fixture_real;
typedef float fixture_single;
typedef bool fixture_flag;
typedef int32_t fixture_count;
typedef uint16_t fixture_wide_octet;
typedef struct { int x; } fixture_point;
typedef int *fixture_pointer;

#define FIXTURE_INTEGER(name, T) \
    T fixture_echo_##name(T x); \
    T fixture_min_##name(void); \
    T fixture_max_##name(void); \
    long long fixture_widen_##name(T x);

FIXTURE_INTEGER(schar, signed char)
FIXTURE_INTEGER(uchar, unsigned char)
FIXTURE_INTEGER(short, short)
FIXTURE_INTEGER(ushort, unsigned short)
FIXTURE_INTEGER(int, int)
FIXTURE_INTEGER(uint, unsigned int)
FIXTURE_INTEGER(long, long)
FIXTURE_INTEGER(ulong, unsigned long)
FIXTURE_INTEGER(llong, long long)
FIXTURE_INTEGER(ullong, unsigned long long)
FIXTURE_INTEGER(i8, int8_t)
FIXTURE_INTEGER(i16, int16_t)
FIXTURE_INTEGER(i32, int32_t)
FIXTURE_INTEGER(i64, int64_t)
FIXTURE_INTEGER(u8, uint8_t)
FIXTURE_INTEGER(u16, uint16_t)
FIXTURE_INTEGER(u32, uint32_t)
FIXTURE_INTEGER(u64, uint64_t)
FIXTURE_INTEGER(size, size_t)
FIXTURE_INTEGER(ssize, ssize_t)
FIXTURE_INTEGER(ptrdiff, ptrdiff_t)
FIXTURE_INTEGER(intptr, intptr_t)
FIXTURE_INTEGER(uintptr, uintptr_t)
FIXTURE_INTEGER(colour, fixture_colour)
FIXTURE_INTEGER(tag, enum fixture_tag)
FIXTURE_INTEGER(octet, fixture_octet)
FIXTURE_INTEGER(char, char)
FIXTURE_INTEGER(letter, fixture_letter)
FIXTURE_INTEGER(count, fixture_count)

bool fixture_not(bool b);
_Bool fixture_echo_bool(_Bool b);
bool fixture_true(void);
int fixture_bool_int(bool b);
fixture_flag fixture_echo_flag(fixture_flag b);

double fixture_echo_double(double x);
float fixture_echo_float(float x);
double fixture_float_as_double(float x);
float fixture_third(void);
fixture_real fixture_echo_real(fixture_real x);
fixture_single fixture_echo_single(fixture_single x);

/* Ten integer arguments of mixed widths, so the last are passed on the stack, then two reals. */
double fixture_sum(signed char a, short b, int c, long d, long long e, unsigned char f, unsigned short g, unsigned int h, int8_t i, uint16_t j, float k, double l);

/* A function answering nothing, one whose answer a binding may discard, and what they did. */
void fixture_store(int x);
int fixture_bump(void);
int fixture_stored(void);
const char *fixture_name(void);

fixture_point fixture_origin(void);
fixture_pointer fixture_nowhere(void);
int fixture_point_x(fixture_point p);
int fixture_count_up(fixture_pointer p);
fixture_wide_octet fixture_wide(fixture_wide_octet x);

/* Strings. A length of null is -1. `fixture_copy` and `fixture_repeat` answer memory `fixture_free`
 * frees, and the library counts both, so a test sees that every string it was answered was freed. */
typedef char *fixture_text;
typedef const char *fixture_const_text;
/* An integer used as a truth value, as Cairo's `cairo_bool_t` is. */
typedef int fixture_truth;

long fixture_length(const char *s);
int fixture_byte_at(const char *s, int i);
const char *fixture_static(void);
const char *fixture_echo_text(const char *s);
char *fixture_copy(const char *s);
char *fixture_repeat(const char *s, int n);
void fixture_free(char *s);
long fixture_allocated(void);
long fixture_freed(void);
const char *fixture_invalid(int which);
const char *fixture_maybe(int present);
char *fixture_maybe_copy(const char *s);
fixture_text fixture_typedef_text(void);
long fixture_typedef_length(fixture_const_text s);
const char *fixture_join(const char *a, const char *b);
fixture_truth fixture_truth_of(int x);
int fixture_truth_value(fixture_truth t);
void fixture_wrong_free(int x);
int fixture_free_answer(char *s);
int fixture_count_args(const char **argv);
long fixture_writable_length(fixture_text s);

#endif
