/* The C library the C binding tests call; `avin_fixture.h` says what each function does. */
#include "avin_fixture.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static long allocated;
static long freed;
static char joined[256];

long fixture_length(const char *s) { return s ? (long)strlen(s) : -1; }
int fixture_byte_at(const char *s, int i) { return (unsigned char)s[i]; }
const char *fixture_static(void) { return "static text"; }
const char *fixture_echo_text(const char *s) { return s; }

char *fixture_copy(const char *s) {
    size_t n = strlen(s);
    char *p = malloc(n + 1);
    memcpy(p, s, n + 1);
    allocated++;
    return p;
}

char *fixture_repeat(const char *s, int n) {
    size_t m = strlen(s);
    char *p = malloc(m * (size_t)n + 1);
    for (int i = 0; i < n; i++) memcpy(p + m * (size_t)i, s, m);
    p[m * (size_t)n] = 0;
    allocated++;
    return p;
}

void fixture_free(char *s) { freed++; free(s); }
long fixture_allocated(void) { return allocated; }
long fixture_freed(void) { return freed; }

/* Byte strings that are not UTF-8, each ill-formed at the byte its comment says. */
const char *fixture_invalid(int which) {
    switch (which) {
    case 0: return "ab\xff";             /* a byte no sequence begins with, at 2 */
    case 1: return "a\xc3";              /* a sequence cut short, at 1 */
    case 2: return "\xc0\xaf";           /* an overlong `/`, at 0 */
    case 3: return "xy\xed\xa0\x80";     /* a surrogate, U+D800, at 2 */
    case 4: return "\xf4\x90\x80\x80";   /* past U+10FFFF, at 0 */
    case 5: return "ok \x80";            /* a continuation byte alone, at 3 */
    default: return "\xe2\x82\xac\xf0\x9f\x98\x80"; /* valid: U+20AC then U+1F600 */
    }
}

const char *fixture_maybe(int present) { return present ? "present" : 0; }
char *fixture_maybe_copy(const char *s) { return s ? fixture_copy(s) : 0; }
fixture_text fixture_typedef_text(void) { return (fixture_text)"typedef text"; }
long fixture_typedef_length(fixture_const_text s) { return (long)strlen(s); }

const char *fixture_join(const char *a, const char *b) {
    snprintf(joined, sizeof joined, "%s+%s", a ? a : "(null)", b ? b : "(null)");
    return joined;
}

fixture_truth fixture_truth_of(int x) { return x; }
int fixture_truth_value(fixture_truth t) { return t; }
void fixture_wrong_free(int x) { (void)x; }
int fixture_free_answer(char *s) { free(s); return 0; }
int fixture_count_args(const char **argv) { int n = 0; while (argv[n]) n++; return n; }
long fixture_writable_length(fixture_text s) { return (long)strlen(s); }

/* In the library, and declared by no header. */
int fixture_undeclared(int x) { return x; }
