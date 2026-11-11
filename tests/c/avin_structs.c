/* The structs of avin_structs.h. */
#include "avin_structs.h"
#include "avin_arrays.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

as_one as_one_make(int seed) { as_one v = { (char)seed }; return v; }
as_one as_one_echo(as_one v) { v.c += 1; return v; }
double as_one_sum(int a, as_one v, double b) { return a + v.c + b; }

as_two as_two_make(int seed) { as_two v = { (int16_t)(seed * 100) }; return v; }
as_two as_two_echo(as_two v) { v.a += 1; return v; }
double as_two_sum(int a, as_two v, double b) { return a + v.a + b; }

as_three as_three_make(int seed) { as_three v = { (uint8_t)seed, (uint8_t)(seed + 1), (uint8_t)(seed + 2) }; return v; }
as_three as_three_echo(as_three v) { v.a += 1; v.b += 1; v.c += 1; return v; }
double as_three_sum(int a, as_three v, double b) { return a + v.a + v.b + v.c + b; }

as_four as_four_make(int seed) { as_four v = { seed * 1000 }; return v; }
as_four as_four_echo(as_four v) { v.v += 1; return v; }
double as_four_sum(int a, as_four v, double b) { return a + v.v + b; }

as_eight as_eight_make(int seed) { as_eight v = { seed, seed + 0.5f }; return v; }
as_eight as_eight_echo(as_eight v) { v.a += 1; v.b += 1; return v; }
double as_eight_sum(int a, as_eight v, double b) { return a + v.a + v.b + b; }

as_twelve as_twelve_make(int seed) { as_twelve v = { seed + 0.25f, seed + 0.5f, seed + 0.75f }; return v; }
as_twelve as_twelve_echo(as_twelve v) { v.a += 1; v.b += 1; v.c += 1; return v; }
double as_twelve_sum(int a, as_twelve v, double b) { return a + v.a + v.b + v.c + b; }

as_sixteen as_sixteen_make(int seed) { as_sixteen v = { (int64_t)seed << 33, seed + 0.125 }; return v; }
as_sixteen as_sixteen_echo(as_sixteen v) { v.a += 1; v.b += 1; return v; }
double as_sixteen_sum(int a, as_sixteen v, double b) { return a + (double)v.a + v.b + b; }

as_twentyfour as_twentyfour_make(int seed) { as_twentyfour v = { seed, -seed, (int64_t)seed * 3 }; return v; }
as_twentyfour as_twentyfour_echo(as_twentyfour v) { v.a += 1; v.b += 1; v.c += 1; return v; }
double as_twentyfour_sum(int a, as_twentyfour v, double b) { return a + (double)(v.a + v.b + v.c) + b; }

as_thirtytwo as_thirtytwo_make(int seed) { as_thirtytwo v = { seed, seed * 2.0, seed * 3.0, seed * 4.0 }; return v; }
as_thirtytwo as_thirtytwo_echo(as_thirtytwo v) { v.a += 1; v.b += 1; v.c += 1; v.d += 1; return v; }
double as_thirtytwo_sum(int a, as_thirtytwo v, double b) { return a + v.a + v.b + v.c + v.d + b; }

as_forty as_forty_make(int seed) { as_forty v = { seed, seed + 1, seed + 2, seed + 3, seed + 4 }; return v; }
as_forty as_forty_echo(as_forty v) { v.a += 1; v.b += 1; v.c += 1; v.d += 1; v.e += 1; return v; }
double as_forty_sum(int a, as_forty v, double b) { return a + (double)(v.a + v.b + v.c + v.d + v.e) + b; }

double as_many(as_thirtytwo a, int i, as_forty b, double d, as_sixteen c, as_three e, as_twelve f) {
    return a.a + a.b + a.c + a.d + i + (double)(b.a + b.b + b.c + b.d + b.e) + d + (double)c.a + c.b + e.a + e.b + e.c + f.a + f.b + f.c;
}

as_padded as_padded_make(int which) {
    as_padded p;
    memset(&p, 0, sizeof p);
    p.c1 = 'a'; p.c2 = 'b'; p.c3 = 'c'; p.c4 = 'd'; p.c5 = 'e'; p.c6 = 'f'; p.c7 = 'g'; p.c8 = 'h';
    p.c9 = 'i'; p.c10 = 'j'; p.c11 = 'k';
    if (which == 0) {
        p.l = INT64_MIN; p.i = INT32_MIN; p.s = INT16_MIN; p.d = -DBL_MAX; p.f = -FLT_MAX; p.t = false;
        p.ul = 0; p.ui = 0; p.us = 0; p.sc = INT8_MIN; p.uc = 0;
    } else {
        p.l = INT64_MAX; p.i = INT32_MAX; p.s = INT16_MAX; p.d = DBL_MAX; p.f = FLT_MAX; p.t = true;
        p.ul = INT64_MAX; p.ui = UINT32_MAX; p.us = UINT16_MAX; p.sc = INT8_MAX; p.uc = UCHAR_MAX;
    }
    return p;
}

int as_padded_check(const as_padded *p, int which) {
    as_padded q = as_padded_make(which);
    return p->c1 == q.c1 && p->l == q.l && p->c2 == q.c2 && p->i == q.i && p->c3 == q.c3 && p->s == q.s
        && p->c4 == q.c4 && p->d == q.d && p->c5 == q.c5 && p->f == q.f && p->c6 == q.c6 && p->t == q.t
        && p->c7 == q.c7 && p->ul == q.ul && p->c8 == q.c8 && p->ui == q.ui && p->c9 == q.c9 && p->us == q.us
        && p->c10 == q.c10 && p->sc == q.sc && p->c11 == q.c11 && p->uc == q.uc;
}

double as_point_length2(const as_point *p) { return p->x * p->x + p->y * p->y; }
int as_point_is_null(const as_point *p) { return p == NULL; }
double as_rect_area(as_rect r) { return r.size.x * r.size.y; }
double as_rect_area_or(const as_rect *r) { return r ? r->size.x * r->size.y : -1.0; }
as_rect as_rect_grow(as_rect r, double by) {
    r.origin.x -= by; r.origin.y -= by; r.size.x += 2 * by; r.size.y += 2 * by;
    return r;
}

void as_matrix_init_translate(as_matrix *m, double tx, double ty) {
    m->xx = 1; m->yx = 0; m->xy = 0; m->yy = 1; m->x0 = tx; m->y0 = ty;
}

void as_matrix_translate(as_matrix *m, double tx, double ty) {
    m->x0 += m->xx * tx + m->xy * ty;
    m->y0 += m->yx * tx + m->yy * ty;
}

as_point as_matrix_transform(const as_matrix *m, as_point p) {
    as_point r = { m->xx * p.x + m->xy * p.y + m->x0, m->yx * p.x + m->yy * p.y + m->y0 };
    return r;
}

void as_matrix_multiply(as_matrix *result, const as_matrix *a, const as_matrix *b) {
    as_matrix r;
    r.xx = a->xx * b->xx + a->yx * b->xy;
    r.yx = a->xx * b->yx + a->yx * b->yy;
    r.xy = a->xy * b->xx + a->yy * b->xy;
    r.yy = a->xy * b->yx + a->yy * b->yy;
    r.x0 = a->x0 * b->xx + a->y0 * b->xy + b->x0;
    r.y0 = a->x0 * b->yx + a->y0 * b->yy + b->y0;
    *result = r;
}

/* Scales the matrix, or fails with status 1 for a scale of zero, leaving it as it was. */
int as_matrix_scale_status(as_matrix *m, double s) {
    if (s == 0) return 1;
    m->xx *= s; m->yx *= s; m->xy *= s; m->yy *= s;
    return 0;
}

struct as_tagged as_tagged_swap(struct as_tagged t) { struct as_tagged r = { t.y, t.x }; return r; }
int as_empty_count(as_empty e) { (void)e; return (int)sizeof e; }
as_empty as_empty_make(void) { as_empty e; return e; }

static const char *const as_strings[] = { "", "plain", "a\xc3\xa9\xe2\x9c\x93\xf0\x9f\x98\x80z", NULL, "bad\xff" };

as_named as_named_make(int which) {
    as_named n;
    n.name = as_strings[which % 5];
    n.id = which == 0 ? INT32_MIN : which == 1 ? INT32_MAX : which;
    n.kind = which == 0 ? AS_KIND_PLAIN : which == 1 ? AS_KIND_BOLD : which == 2 ? AS_KIND_FAINT : which < 5 ? AS_KIND_PLAIN : (as_kind)42;
    n.on = which % 2 == 1;
    n.truth = which == 0 ? 0 : which == 1 ? -1 : 2;
    return n;
}

long long as_named_length(const as_named *n) {
    if (n->name == NULL) return -1;
    return (long long)strlen(n->name) * 1000 + n->id % 1000 + (n->on ? 100000000 : 0) + (n->truth ? 200000000 : 0) + (n->kind == AS_KIND_FAINT ? 400000000 : 0);
}

long long as_named_by_value(as_named n) { return as_named_length(&n); }

as_deep as_deep_make(int which) {
    as_deep d;
    d.bounds.origin.x = which; d.bounds.origin.y = which + 1;
    d.bounds.size.x = which + 2; d.bounds.size.y = which + 3;
    d.tag = as_named_make(which);
    d.level = (int8_t)(which - 128);
    return d;
}

long long as_deep_check(as_deep d) {
    return (long long)(d.bounds.origin.x + d.bounds.origin.y + d.bounds.size.x + d.bounds.size.y) * 1000000000000LL + as_named_length(&d.tag) + d.level;
}

void as_named_fill(as_named *n, int which) { *n = as_named_make(which); }

/* Replaces the name C was given with its own, and counts it. */
void as_named_rename(as_named *n) {
    n->name = n->name && strcmp(n->name, "old") == 0 ? "renamed" : "other";
    n->id += 1;
}

double as_glyphs_sum(const as_glyph *glyphs, int count) {
    double total = 0;
    for (int i = 0; i < count; i++) total += (double)glyphs[i].index * 1000 + glyphs[i].x * 10 + glyphs[i].y;
    return total;
}

long long as_names_total(const as_named *names, size_t count) {
    long long total = 0;
    for (size_t i = 0; i < count; i++) total += as_named_length(&names[i]);
    return total;
}

double as_rects_area(size_t count, const as_rect *rects) {
    double total = 0;
    for (size_t i = 0; i < count; i++) total += rects[i].size.x * rects[i].size.y;
    return total;
}

static int glyphs_allocated = 0;
static int glyphs_freed = 0;

static as_glyph *glyphs_of(int n) {
    as_glyph *g = malloc(sizeof(as_glyph) * (size_t)(n > 0 ? n : 1));
    for (int i = 0; i < n; i++) { g[i].index = (unsigned long)i + 1; g[i].x = i * 0.5; g[i].y = -i; }
    glyphs_allocated += 1;
    return g;
}

/* Answers `n` glyphs; a negative `n` fails with status 2 after allocating, and zero answers null. */
int as_glyphs_make(int n, as_glyph **glyphs, int *count) {
    if (n == 0) { *glyphs = NULL; *count = 0; return 0; }
    *glyphs = glyphs_of(n < 0 ? 1 : n);
    *count = n < 0 ? 1 : n;
    return n < 0 ? 2 : 0;
}

void as_glyphs_answer(int n, as_glyph **glyphs, long *count) {
    *glyphs = n == 0 ? NULL : glyphs_of(n);
    *count = n;
}

void as_glyphs_free(as_glyph *glyphs) { glyphs_freed += 1; free(glyphs); }
int as_glyphs_allocated(void) { return glyphs_allocated; }
int as_glyphs_freed(void) { return glyphs_freed; }

void as_reals_make(int n, double **values, size_t *count) {
    *values = malloc(sizeof(double) * (size_t)(n > 0 ? n : 1));
    for (int i = 0; i < n; i++) (*values)[i] = i == 0 ? -0.0 : i == 1 ? INFINITY : i * 1.5;
    *count = (size_t)n;
}

void as_reals_free(double *values) { free(values); }

void as_names_make(int n, as_named **names, int *count) {
    *names = malloc(sizeof(as_named) * (size_t)(n > 0 ? n : 1));
    for (int i = 0; i < n; i++) (*names)[i] = as_named_make(i % 3);
    *count = n;
}

/* A null array with a length (0), and a negative length (1). */
void as_bad_answer(int which, as_point **points, int *count) {
    if (which == 0) { *points = NULL; *count = 3; }
    else { *points = malloc(sizeof(as_point)); *count = -1; }
}

static int dash_n = 0;
static int dash_calls = 0;

void as_dash_set(int n) { dash_n = n; }
int as_dash_count(void) { return dash_n; }
int as_dash_calls(void) { return dash_calls; }

void as_dash_get(double *dashes, double *offset) {
    dash_calls += 1;
    for (int i = 0; i < dash_n; i++) dashes[i] = 0.5 + i;
    *offset = dash_n * 2.0;
}

int as_glyphs_fill(int n, as_glyph *glyphs) {
    if (n == 2) return 4;
    for (int i = 0; i < n; i++) { glyphs[i].index = (unsigned long)i + 1; glyphs[i].x = i * 0.5; glyphs[i].y = -i; }
    return 0;
}

int as_spread(int n, int32_t *ints, uint8_t *bytes) {
    for (int i = 0; i < n; i++) ints[i] = i == 0 ? INT32_MIN : i == 1 ? INT32_MAX : i * -3;
    for (int i = 0; i < 2 * n; i++) bytes[i] = (uint8_t)(255 - i);
    return n * 3;
}

static int clusters_allocated = 0;
static int clusters_freed = 0;
static int shape_nulls = 0;

int as_shape(const char *text, int n, as_glyph **glyphs, int *num_glyphs, as_cluster **clusters, int *num_clusters, as_cluster_flags *flags) {
    int m = n < 0 ? 1 : n;
    *glyphs = n == 0 ? NULL : glyphs_of(m);
    *num_glyphs = m;
    if (clusters == NULL && num_clusters == NULL && flags == NULL) shape_nulls += 1;
    if (clusters != NULL) {
        int k = (m + 1) / 2;
        if (k == 0) *clusters = NULL;
        else {
            *clusters = malloc(sizeof(as_cluster) * (size_t)k);
            for (int i = 0; i < k; i++) { (*clusters)[i].num_bytes = (int32_t)strlen(text) + i; (*clusters)[i].num_glyphs = i == k - 1 && m % 2 ? 1 : 2; }
            clusters_allocated += 1;
        }
        if (num_clusters != NULL) *num_clusters = k;
    }
    if (flags != NULL) *flags = text[0] == 'r' ? AS_CLUSTER_BACKWARD : AS_CLUSTER_FORWARD;
    return n < 0 ? 7 : 0;
}

void as_clusters_free(as_cluster *clusters) { clusters_freed += 1; free(clusters); }
int as_clusters_allocated(void) { return clusters_allocated; }
int as_clusters_freed(void) { return clusters_freed; }
int as_shape_nulls(void) { return shape_nulls; }

as_event as_event_int(int32_t i) { as_event e; e.p = NULL; e.i = i; return e; }
as_event as_event_float(float f) { as_event e; e.p = NULL; e.f = f; return e; }
int32_t as_event_to_int(as_event self) { return self.i; }
float as_event_to_float(as_event self) { return self.f; }
as_event as_event_negate(as_event self) { self.i = -self.i; return self; }
as_flags as_flags_make(int32_t kind, unsigned bits) { as_flags f; f.kind = kind; f.bits = bits; f.more = bits * 3; return f; }
unsigned as_flags_bits(as_flags self) { return self.bits + self.more; }
int32_t as_flags_kind(as_flags self) { return self.kind; }

int as_pair_sum(as_pair p) { return p.a + p.b; }

as_rect as_wait_grow(as_rect r, double by, int timeout_ms) {
    if (aa_wait_flag(timeout_ms) != 1) { as_rect none = { { -1, -1 }, { -1, -1 } }; return none; }
    return as_rect_grow(r, by);
}

long long as_wait_named(const as_named *n, int timeout_ms) {
    if (aa_wait_flag(timeout_ms) != 1) return -1;
    return as_named_length(n);
}

int32_t as_wait_event(as_event e, int timeout_ms) {
    if (aa_wait_flag(timeout_ms) != 1) return -1;
    return e.i;
}

double as_wait_glyphs(const as_glyph *glyphs, int count, int timeout_ms) {
    if (aa_wait_flag(timeout_ms) != 1) return -1;
    return as_glyphs_sum(glyphs, count);
}
