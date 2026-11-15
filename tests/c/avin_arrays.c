/* The functions of avin_arrays.h. */
#include "avin_arrays.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

long long aa_sum_bytes(const uint8_t *p, size_t n) { long long s = 0; for (size_t i = 0; i < n; i++) s += p[i]; return s; }
long long aa_sum_chars(const char *p, int n) { long long s = 0; for (int i = 0; i < n; i++) s += (unsigned char)p[i]; return s; }
long long aa_sum_uchars(const unsigned char *p, int n) { long long s = 0; for (int i = 0; i < n; i++) s += p[i]; return s; }
long long aa_sum_octets(const aa_octet *p, aa_size n) { long long s = 0; for (aa_size i = 0; i < n; i++) s += p[i]; return s; }
long long aa_sum_ints(const int32_t *p, int n) { long long s = 0; for (int i = 0; i < n; i++) s += p[i]; return s; }
long long aa_sum_plain_ints(const int *p, unsigned n) { long long s = 0; for (unsigned i = 0; i < n; i++) s += p[i]; return s; }
long long aa_sum_longs(const int64_t *p, long n) { long long s = 0; for (long i = 0; i < n; i++) s += p[i]; return s; }
long long aa_sum_fixed(const aa_fixed *p, long long n) { long long s = 0; for (long long i = 0; i < n; i++) s += p[i]; return s; }
double aa_sum_reals(const double *p, unsigned n) { double s = 0; for (unsigned i = 0; i < n; i++) s += p[i]; return s; }
double aa_sum_aa_reals(const aa_real *p, size_t n) { double s = 0; for (size_t i = 0; i < n; i++) s += p[i]; return s; }

void aa_fill_bytes(uint8_t *p, size_t n, int start) { for (size_t i = 0; i < n; i++) p[i] = (uint8_t)(start + (int)i); }
void aa_fill_chars(char *p, size_t n) { for (size_t i = 0; i < n; i++) p[i] = (char)('a' + (int)(i % 26)); }
void aa_scale_ints(int *p, size_t n, int k) { for (size_t i = 0; i < n; i++) p[i] *= k; }
void aa_negate_longs(long long *p, size_t n) { for (size_t i = 0; i < n; i++) p[i] = -p[i]; }
void aa_halve_reals(double *p, size_t n) { for (size_t i = 0; i < n; i++) p[i] /= 2; }
long long aa_sum_mutable(int *p, size_t n) { long long s = 0; for (size_t i = 0; i < n; i++) s += p[i]; return s; }

int aa_count_first(int n, const int *p) { return n > 0 ? n + p[n - 1] - p[n - 1] : n; }
int aa_count_uchar(const uint8_t *p, unsigned char n) { (void)p; return n; }
int aa_count_schar(const uint8_t *p, signed char n) { (void)p; return n; }
int aa_count_short(const uint8_t *p, short n) { (void)p; return n; }
unsigned long long aa_count_u64(const uint8_t *p, uint64_t n) { (void)p; return n; }
long long aa_count_i64(const uint8_t *p, int64_t n) { (void)p; return n; }
int aa_null_length(const uint8_t *p, size_t n) { return (p == NULL ? 1 : 0) + 2 * (int)n; }

double aa_dot(const double *a, int na, const double *b, int nb, int *difference) {
    *difference = na - nb;
    return aa_dot_plain(a, na, b, nb);
}

double aa_dot_plain(const double *a, int na, const double *b, int nb) {
    double s = 0;
    for (int i = 0; i < na && i < nb; i++) s += a[i] * b[i];
    return s;
}

long long aa_last(const int64_t *p, size_t n) { return n == 0 ? -1 : p[n - 1]; }

/* 54 bytes, as a TrueType `head` table is long, starting with its version 1.0. */
static const uint8_t table[54] = { 0, 1, 0, 0, 0, 1, 0, 0, 1, 2, 3, 4, 0x5f, 0x0f, 0x3c, 0xf5 };

void aa_take(uint8_t *buf, size_t *len) {
    if (buf == NULL) { *len = sizeof table; return; }
    size_t n = *len < sizeof table ? *len : sizeof table;
    memcpy(buf, table, n);
    *len = n;
}

int aa_load(long tag, long offset, uint8_t *buf, unsigned long *len) {
    if (tag != 1) return 142;
    if (buf == NULL) { *len = sizeof table - (unsigned long)offset; return 0; }
    unsigned long n = *len < sizeof table - (unsigned long)offset ? *len : sizeof table - (unsigned long)offset;
    memcpy(buf, table + offset, n);
    *len = n;
    return 0;
}

void aa_overrun(uint8_t *buf, size_t *len) { (void)buf; *len = *len + 1; }

void aa_void_fill(void *p, int c, size_t n) { memset(p, c, n); }

long long aa_void_sum(const void *p, size_t n) {
    const unsigned char *b = p;
    long long s = 0;
    for (size_t i = 0; i < n; i++) s += b[i];
    return s;
}

long long aa_void_sum_two(const void *a, size_t na, const void *b, int nb) {
    return aa_void_sum(a, na) * 1000 + aa_void_sum(b, (size_t)nb);
}

void aa_void_take(void *buf, size_t *len) { aa_take(buf, len); }

void aa_void_overrun(void *buf, size_t *len) { (void)buf; *len = *len + 1; }

int aa_void_null(const void *p, size_t n) { return p == NULL ? (n == 0 ? -1 : -2) : (int)n; }

int aa_info(int flavor, aa_info_t *buffer) {
    if (flavor < 1 || flavor > 9) { errno = EINVAL; return -1; }
    struct aa_info_v1 *info = (struct aa_info_v1 *)buffer;
    for (int i = 0; i < 16; i++) info->tag[i] = (uint8_t)(i + 1);
    info->a = 7;
    info->b = 8;
    info->flavor = (uint64_t)flavor;
    return 0;
}
int aa_info_const(int flavor, aa_const_info_t *buffer) { return aa_info(flavor, (aa_info_t *)buffer); }
int aa_counts(void *out, int k) { struct aa_counts *c = out; c->a = 3; c->b = 4; c->c = k; return 0; }
int aa_counts_pp(void **out, int k) { return aa_counts((void *)out, k); }
int aa_info_counts(aa_info_t *buffer, int k) { return aa_counts((void *)buffer, k); }
int aa_fixed_out(aa_fixed *p, int k) { *p = k; return 0; }
long long aa_info_sum(aa_info_t *p, size_t n) { return aa_void_sum((const void *)p, n); }
long long aa_pp_sum(void **p, size_t n) { return aa_void_sum((const void *)p, n); }
void aa_info_fill(aa_info_t *p, int c, size_t n) { memset((void *)p, c, n); }
int aa_info_octets(aa_octet *p, int n) { for (int i = 0; i < n; i++) p[i] = (aa_octet)(i + 1); return n; }
int aa_alloc_bytes(void **out, int n, int *len) {
    uint8_t *p = malloc(n > 0 ? (size_t)n : 1);
    if (!p) return -1;
    for (int i = 0; i < n; i++) p[i] = (uint8_t)(i + 1);
    *out = p;
    *len = n;
    return 0;
}

int aa_errno(int code, int value) { if (code != 0) { errno = code; return -1; } return value; }
long aa_errno_long(int code, long value) { if (code != 0) { errno = code; return -1; } return value; }
ssize_t aa_errno_ssize(int code, ssize_t value) { if (code != 0) { errno = code; return -1; } return value; }
size_t aa_errno_size(int code, size_t value) { if (code != 0) { errno = code; return (size_t)-1; } return value; }
int aa_errno_out(int code, int value, int *out) { if (code != 0) { errno = code; return -1; } *out = value; return 0; }
int aa_errno_set(int code, int value) { errno = code; return value; }

void aa_squares(int *buf, int *len) {
    int half = *len / 2;
    for (int i = 0; i < half; i++) buf[i] = i * i;
    *len = half;
}

void aa_huge(uint8_t *buf, uint64_t *len) { (void)buf; *len = 1ull << 40; }

long long aa_sum_uints(const unsigned int *p, size_t n) { long long s = 0; for (size_t i = 0; i < n; i++) s += p[i]; return s; }
long long aa_sum_ulongs(const uint64_t *p, size_t n) { long long s = 0; for (size_t i = 0; i < n; i++) s += (long long)p[i]; return s; }
double aa_sum_floats(const float *p, size_t n) { double s = 0; for (size_t i = 0; i < n; i++) s += p[i]; return s; }
long long aa_sum_shorts(const short *p, size_t n) { long long s = 0; for (size_t i = 0; i < n; i++) s += p[i]; return s; }
int aa_sum_bools(const bool *p, size_t n) { int s = 0; for (size_t i = 0; i < n; i++) s += p[i]; return s; }
long long aa_sum_pointers(const uint8_t **p, size_t n) { long long s = 0; for (size_t i = 0; i < n; i++) s += p[i] != NULL; return s; }

static int flag;

void aa_set_flag(void) { __atomic_store_n(&flag, 1, __ATOMIC_SEQ_CST); }
void aa_clear_flag(void) { __atomic_store_n(&flag, 0, __ATOMIC_SEQ_CST); }

static void sleep_ms(int ms) {
    struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&t, NULL);
}

int aa_wait_flag(int timeout_ms) {
    for (int waited = 0; waited < timeout_ms; waited++) {
        if (__atomic_load_n(&flag, __ATOMIC_SEQ_CST)) return 1;
        sleep_ms(1);
    }
    return __atomic_load_n(&flag, __ATOMIC_SEQ_CST);
}

long long aa_wait_sum(const int32_t *p, size_t n, int timeout_ms) {
    if (!aa_wait_flag(timeout_ms)) return -1;
    long long s = 0;
    for (size_t i = 0; i < n; i++) s += p[i];
    return s;
}

int aa_wait_fill(uint8_t *p, size_t n, int timeout_ms) {
    if (!aa_wait_flag(timeout_ms)) return 0;
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)i;
    return 1;
}

int aa_wait_object(const fx_object *o, int timeout_ms) {
    if (!aa_wait_flag(timeout_ms)) return -1;
    return fx_object_value(o);
}

int aa_wait_objects(const fx_object *a, const fx_object *b, const fx_object *c, int timeout_ms) {
    if (!aa_wait_flag(timeout_ms)) return -1;
    return fx_object_value(a) + fx_object_value(b) + fx_object_value(c);
}

int aa_wait_maybe(const fx_object *o, int timeout_ms) {
    if (!aa_wait_flag(timeout_ms)) return -1;
    return o == NULL ? -2 : fx_object_value(o);
}

size_t aa_sleep_strlen(const char *s, int ms) {
    sleep_ms(ms);
    return strlen(s);
}

int aa_sleep_take(uint8_t *buf, size_t *len, int ms) {
    sleep_ms(ms);
    aa_take(buf, len);
    return 0;
}
