/* The functions of avin_enums.h. */
#include "avin_enums.h"
#include <float.h>
#include <stddef.h>

ae_format_t ae_format_echo(ae_format_t f) { return f; }
int ae_format_code(ae_format_t f) { return (int)f; }
ae_format_t ae_format_of(int code) { return (ae_format_t)code; }
const ae_format_t ae_format_const(void) { return AE_FORMAT_A8; }
enum ae_image_format ae_image_echo(enum ae_image_format f) { return f; }
int ae_image_code(enum ae_image_format f) { return (int)f; }
ae_pdf_version_t ae_pdf_echo(ae_pdf_version_t v) { return v; }
ae_limit_t ae_limit_echo(ae_limit_t v) { return v; }
int ae_limit_code(ae_limit_t v) { return (int)v; }
enum ae_alias ae_alias_echo(enum ae_alias a) { return a; }
int ae_alias_code(enum ae_alias a) { return (int)a; }
ae_only_t ae_only_echo(ae_only_t o) { return o; }
ae_slant_t ae_slant_echo(ae_slant_t s) { return s; }
ae_weight_t ae_weight_echo(ae_weight_t w) { return w; }
ae_big_t ae_big_echo(ae_big_t b) { return b; }
long long ae_big_code(ae_big_t b) { return (long long)b; }
ae_wide_t ae_wide_echo(ae_wide_t w) { return w; }
long long ae_wide_code(ae_wide_t w) { return (long long)w; }
ae_wide_t ae_wide_of(long long code) { return (ae_wide_t)code; }
ae_status_t ae_status_of(int code) { return (ae_status_t)code; }

void ae_signed_out(int which, signed char *a, short *b, int *c, long long *d) {
    *a = which ? SCHAR_MAX : SCHAR_MIN;
    *b = which ? SHRT_MAX : SHRT_MIN;
    *c = which ? INT_MAX : INT_MIN;
    *d = which ? LLONG_MAX : LLONG_MIN;
}

void ae_unsigned_out(int which, unsigned char *a, unsigned short *b, unsigned int *c, unsigned long long *d) {
    *a = which ? UCHAR_MAX : 0;
    *b = which ? USHRT_MAX : 0;
    *c = which ? UINT_MAX : 0;
    *d = which ? (unsigned long long)LLONG_MAX + (which > 1 ? 1u : 0u) : 0;
}

void ae_sizes_out(int which, size_t *a, int8_t *b, uint16_t *c, int64_t *d) {
    *a = which ? (size_t)LLONG_MAX : 0;
    *b = which ? INT8_MAX : INT8_MIN;
    *c = which ? UINT16_MAX : 0;
    *d = which ? INT64_MAX : INT64_MIN;
}

void ae_reals_out(int which, double *a, float *b, ae_real_t *c) {
    switch (which) {
    case 0: *a = 1.5; *b = 0.25f; *c = -2.5; break;
    case 1: *a = NAN; *b = NAN; *c = NAN; break;
    case 2: *a = INFINITY; *b = -INFINITY; *c = INFINITY; break;
    case 3: *a = -0.0; *b = FLT_TRUE_MIN; *c = DBL_TRUE_MIN; break;
    default: *a = DBL_MAX; *b = FLT_MAX; *c = -DBL_MAX; break;
    }
}

void ae_truths_out(int value, bool *a, int *b) {
    *a = value != 0;
    *b = value;
}

void ae_byte_out(int value, uint8_t *b) { *b = (uint8_t)value; }

int ae_minmax(int a, int b, int *lo, int *hi) {
    *lo = a < b ? a : b;
    *hi = a < b ? b : a;
    return a + b;
}

void ae_sum_out(int a, int b, long long *sum) { *sum = (long long)a + b; }

void ae_leave_out(int *a, double *b, const char **c, fx_object **d) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
}

void ae_signed_inout(signed char *a, short *b, int *c, long long *d) {
    *a = (signed char)(*a + 1);
    *b = (short)(*b * 2);
    *c = -*c;
    *d = *d - 1;
}

void ae_unsigned_inout(unsigned char *a, unsigned int *b, unsigned long long *c) {
    *a = (unsigned char)(*a + 1);
    *b = *b * 2u;
    *c = *c + 1u;
}

void ae_reals_inout(double *a, float *b) {
    *a = *a * 2.0;
    *b = *b * 2.0f;
}

void ae_truths_inout(bool *a, int *b) {
    *a = !*a;
    *b = *b * 3;
}

void ae_device_to_user(double *x, double *y) {
    *x = *x / 2.0;
    *y = *y / 2.0;
}

void ae_prefix_length(const char *text, size_t *length) {
    size_t n = 0;
    while (n < *length && text[n] != ' ' && text[n] != 0) n++;
    *length = n;
}

void ae_format_out(int code, ae_format_t *f) { *f = (ae_format_t)code; }

void ae_format_next(ae_format_t *f) {
    switch (*f) {
    case AE_FORMAT_INVALID: *f = AE_FORMAT_ARGB32; break;
    case AE_FORMAT_ARGB32: *f = AE_FORMAT_RGB24; break;
    case AE_FORMAT_RGB24: *f = AE_FORMAT_A8; break;
    case AE_FORMAT_A8: *f = AE_FORMAT_RGB16_565; break;
    case AE_FORMAT_RGB16_565: *f = AE_FORMAT_RGB30; break;
    default: *f = AE_FORMAT_INVALID; break;
    }
}

ae_format_t ae_parse_format(const char *text, int *consumed) {
    if (text[0] == 'a' && text[1] == '8') { *consumed = 2; return AE_FORMAT_A8; }
    if (text[0] == 'r' && text[1] == 'g' && text[2] == 'b') { *consumed = 3; return AE_FORMAT_RGB24; }
    *consumed = 0;
    return AE_FORMAT_INVALID;
}

void ae_name_out(int which, const char **name) {
    switch (which) {
    case 0: *name = "argb"; break;
    case 1: *name = ""; break;
    case 2: *name = "h\xc3\xa9llo \xe2\x9c\x93 \xf0\x9d\x84\x9e"; break;
    case 3: *name = 0; break;
    default: *name = "\xff"; break;
    }
}

void ae_two_names(const char **first, char **second) {
    static char mutable_name[] = "second";
    *first = "first";
    *second = mutable_name;
}

void ae_skip_spaces(const char **text) {
    while (**text == ' ') (*text)++;
}

void ae_replace(const char **text) {
    *text = (*text)[0] == 'n' ? 0 : "replaced";
}

ae_status_t ae_divide(int a, int b, int *quotient, int *remainder) {
    if (b == 0) return AE_STATUS_INVALID;
    *quotient = a / b;
    *remainder = a % b;
    return AE_STATUS_SUCCESS;
}

ae_status_t ae_status_name(int code, const char **name) {
    if (code != 0) return (ae_status_t)code;
    *name = "named";
    return AE_STATUS_SUCCESS;
}

int ae_checked_half(int value, int *half) {
    if (value % 2 != 0) return -value;
    *half = value / 2;
    return 0;
}

ae_status_t ae_status_format(int code, ae_format_t *f) {
    if (code != 0) return (ae_status_t)code;
    *f = AE_FORMAT_RGB30;
    return AE_STATUS_SUCCESS;
}

ae_status_t ae_status_nothing(int code) { return (ae_status_t)code; }

int ae_failing_write(int *value) {
    *value = 99;
    return 5;
}

int ae_object_make(int value, fx_object **object) {
    if (value < 0) return -1;
    *object = fx_object_new(value);
    return 0;
}

void ae_object_maybe(int value, fx_object **object) { *object = value == 0 ? 0 : fx_object_new(value); }

void ae_object_lent(fx_object *of, fx_object **object) { *object = of; }

ae_status_t ae_object_pair(int a, int b, fx_object **first, fx_object **second) {
    if (a < 0 || b < 0) return AE_STATUS_INVALID;
    *first = fx_object_new(a);
    *second = fx_object_new(b);
    return AE_STATUS_SUCCESS;
}

int ae_face_make(fx_library library, int id, fx_face *face) {
    if (id < 0) return 1;
    *face = fx_face_new(library, id);
    return 0;
}

int ae_object_digits(fx_object *self, int *tens, int *units) {
    int v = fx_object_value(self);
    *tens = v / 10 % 10;
    *units = v % 10;
    return v;
}

ae_status_t ae_object_half(fx_object *self, int *half) {
    int v = fx_object_value(self);
    if (v % 2 != 0) return AE_STATUS_INVALID;
    *half = v / 2;
    return AE_STATUS_SUCCESS;
}
