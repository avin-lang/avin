/* The C library the C binding tests call; `avin_fixture.h` says what each function does. */
#include "avin_fixture.h"

#include <limits.h>

#define FIXTURE_DEFINE(name, T, MIN, MAX) \
    T fixture_echo_##name(T x) { return x; } \
    T fixture_min_##name(void) { return MIN; } \
    T fixture_max_##name(void) { return MAX; } \
    long long fixture_widen_##name(T x) { return (long long)x; }

FIXTURE_DEFINE(schar, signed char, SCHAR_MIN, SCHAR_MAX)
FIXTURE_DEFINE(uchar, unsigned char, 0, UCHAR_MAX)
FIXTURE_DEFINE(short, short, SHRT_MIN, SHRT_MAX)
FIXTURE_DEFINE(ushort, unsigned short, 0, USHRT_MAX)
FIXTURE_DEFINE(int, int, INT_MIN, INT_MAX)
FIXTURE_DEFINE(uint, unsigned int, 0, UINT_MAX)
FIXTURE_DEFINE(long, long, LONG_MIN, LONG_MAX)
FIXTURE_DEFINE(ulong, unsigned long, 0, ULONG_MAX)
FIXTURE_DEFINE(llong, long long, LLONG_MIN, LLONG_MAX)
FIXTURE_DEFINE(ullong, unsigned long long, 0, ULLONG_MAX)
FIXTURE_DEFINE(i8, int8_t, INT8_MIN, INT8_MAX)
FIXTURE_DEFINE(i16, int16_t, INT16_MIN, INT16_MAX)
FIXTURE_DEFINE(i32, int32_t, INT32_MIN, INT32_MAX)
FIXTURE_DEFINE(i64, int64_t, INT64_MIN, INT64_MAX)
FIXTURE_DEFINE(u8, uint8_t, 0, UINT8_MAX)
FIXTURE_DEFINE(u16, uint16_t, 0, UINT16_MAX)
FIXTURE_DEFINE(u32, uint32_t, 0, UINT32_MAX)
FIXTURE_DEFINE(u64, uint64_t, 0, UINT64_MAX)
FIXTURE_DEFINE(size, size_t, 0, SIZE_MAX)
FIXTURE_DEFINE(ssize, ssize_t, -SSIZE_MAX - 1, SSIZE_MAX)
FIXTURE_DEFINE(ptrdiff, ptrdiff_t, PTRDIFF_MIN, PTRDIFF_MAX)
FIXTURE_DEFINE(intptr, intptr_t, INTPTR_MIN, INTPTR_MAX)
FIXTURE_DEFINE(uintptr, uintptr_t, 0, UINTPTR_MAX)
FIXTURE_DEFINE(colour, fixture_colour, FIXTURE_RED, FIXTURE_BLUE)
FIXTURE_DEFINE(tag, enum fixture_tag, FIXTURE_LOW, FIXTURE_HIGH)
FIXTURE_DEFINE(octet, fixture_octet, 0, UINT8_MAX)
FIXTURE_DEFINE(char, char, CHAR_MIN, CHAR_MAX)
FIXTURE_DEFINE(letter, fixture_letter, CHAR_MIN, CHAR_MAX)
FIXTURE_DEFINE(count, fixture_count, INT32_MIN, INT32_MAX)

bool fixture_not(bool b) { return !b; }
_Bool fixture_echo_bool(_Bool b) { return b; }
bool fixture_true(void) { return true; }
int fixture_bool_int(bool b) { return b ? 1 : 0; }
fixture_flag fixture_echo_flag(fixture_flag b) { return b; }

double fixture_echo_double(double x) { return x; }
float fixture_echo_float(float x) { return x; }
double fixture_float_as_double(float x) { return x; }
float fixture_third(void) { return 1.0f / 3.0f; }
fixture_real fixture_echo_real(fixture_real x) { return x; }
fixture_single fixture_echo_single(fixture_single x) { return x; }

double fixture_sum(signed char a, short b, int c, long d, long long e, unsigned char f, unsigned short g, unsigned int h, int8_t i, uint16_t j, float k, double l) {
    return (double)a + b + c + d + (double)e + f + g + h + i + j + k + l;
}

static int stored;

void fixture_store(int x) { stored = x; }
int fixture_bump(void) { return ++stored; }
int fixture_stored(void) { return stored; }
const char *fixture_name(void) { return "fixture"; }

fixture_point fixture_origin(void) { fixture_point p = { 0 }; return p; }
fixture_pointer fixture_nowhere(void) { return 0; }
int fixture_point_x(fixture_point p) { return p.x; }
int fixture_count_up(fixture_pointer p) { return p ? *p : 0; }
fixture_wide_octet fixture_wide(fixture_wide_octet x) { return x; }

/* In the library, and declared by no header. */
int fixture_undeclared(int x) { return x; }
