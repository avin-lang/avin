/* The structs the C binding tests bind: mirrors passed by value in every size class the platforms'
 * calling conventions treat differently (1, 2, 3, 4, 8, 12, 16, 24, 32 and 40 bytes, integers,
 * floats and doubles), answered by value, read through `const` pointers (null among them), filled
 * through `out` and updated through `inout`; a struct padded around every scalar kind, nested
 * structs, an empty one, strings, enum and truth-value fields; arrays of structs read and arrays C
 * allocates and answers; opaque values (a union and a struct with a bit-field) passed and answered by
 * value; and structs a mirror cannot match, for the header check's diagnostics. The test harness
 * compiles it into the `avin-c-fixture` library beside avin_fixture.c. */
#ifndef AVIN_STRUCTS_H
#define AVIN_STRUCTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* An enumeration a mirror's field holds. */
typedef enum { AS_KIND_PLAIN = 0, AS_KIND_BOLD = 5, AS_KIND_FAINT = -3 } as_kind;

/* One struct of each size class, each field filled from a seed: `as_<size>_make(seed)` answers it,
 * `as_<size>_echo` answers it with every field one more, and `as_<size>_sum(a, v, b)` answers `a`,
 * the fields and `b` added. */
typedef struct { char c; } as_one;
typedef struct { int16_t a; } as_two;
typedef struct { uint8_t a, b, c; } as_three;
typedef struct { int32_t v; } as_four;
typedef struct { int32_t a; float b; } as_eight;
typedef struct { float a, b, c; } as_twelve;
typedef struct { int64_t a; double b; } as_sixteen;
typedef struct { int64_t a, b, c; } as_twentyfour;
typedef struct { double a, b, c, d; } as_thirtytwo;
typedef struct { int64_t a, b, c, d, e; } as_forty;

as_one as_one_make(int seed);
as_one as_one_echo(as_one v);
double as_one_sum(int a, as_one v, double b);
as_two as_two_make(int seed);
as_two as_two_echo(as_two v);
double as_two_sum(int a, as_two v, double b);
as_three as_three_make(int seed);
as_three as_three_echo(as_three v);
double as_three_sum(int a, as_three v, double b);
as_four as_four_make(int seed);
as_four as_four_echo(as_four v);
double as_four_sum(int a, as_four v, double b);
as_eight as_eight_make(int seed);
as_eight as_eight_echo(as_eight v);
double as_eight_sum(int a, as_eight v, double b);
as_twelve as_twelve_make(int seed);
as_twelve as_twelve_echo(as_twelve v);
double as_twelve_sum(int a, as_twelve v, double b);
as_sixteen as_sixteen_make(int seed);
as_sixteen as_sixteen_echo(as_sixteen v);
double as_sixteen_sum(int a, as_sixteen v, double b);
as_twentyfour as_twentyfour_make(int seed);
as_twentyfour as_twentyfour_echo(as_twentyfour v);
double as_twentyfour_sum(int a, as_twentyfour v, double b);
as_thirtytwo as_thirtytwo_make(int seed);
as_thirtytwo as_thirtytwo_echo(as_thirtytwo v);
double as_thirtytwo_sum(int a, as_thirtytwo v, double b);
as_forty as_forty_make(int seed);
as_forty as_forty_echo(as_forty v);
double as_forty_sum(int a, as_forty v, double b);

/* Two structs by value beside scalars, which exhausts the argument registers. */
double as_many(as_thirtytwo a, int i, as_forty b, double d, as_sixteen c, as_three e, as_twelve f);

/* A struct padded after a byte before every scalar kind; `which` 0 holds each kind's least value and
 * 1 its greatest. */
typedef struct {
    char c1; int64_t l;
    char c2; int32_t i;
    char c3; int16_t s;
    char c4; double d;
    char c5; float f;
    char c6; bool t;
    char c7; uint64_t ul;
    char c8; uint32_t ui;
    char c9; uint16_t us;
    char c10; int8_t sc;
    char c11; unsigned char uc;
} as_padded;

as_padded as_padded_make(int which);
int as_padded_check(const as_padded *p, int which);

/* A point and a rectangle, which nests it twice, Cairo's matrix, and a struct nesting a rectangle and
 * a struct of strings. */
typedef struct { double x, y; } as_point;
typedef struct { as_point origin; as_point size; } as_rect;
typedef struct { double xx, yx, xy, yy, x0, y0; } as_matrix;
typedef struct { const char *name; int32_t id; as_kind kind; bool on; int truth; } as_named;
typedef struct { as_rect bounds; as_named tag; int8_t level; } as_deep;
struct as_tagged { int32_t x; int32_t y; };
typedef struct { } as_empty;

double as_point_length2(const as_point *p);
int as_point_is_null(const as_point *p);
double as_rect_area(as_rect r);
double as_rect_area_or(const as_rect *r);
as_rect as_rect_grow(as_rect r, double by);
void as_matrix_init_translate(as_matrix *m, double tx, double ty);
void as_matrix_translate(as_matrix *m, double tx, double ty);
as_point as_matrix_transform(const as_matrix *m, as_point p);
void as_matrix_multiply(as_matrix *result, const as_matrix *a, const as_matrix *b);
int as_matrix_scale_status(as_matrix *m, double s);
struct as_tagged as_tagged_swap(struct as_tagged t);
int as_empty_count(as_empty e);
as_empty as_empty_make(void);

/* Strings, enumerations and truth values in structs: `which` picks the string (0 empty, 1 ASCII, 2
 * every UTF-8 width, 3 null, 4 ill-formed, 5 empty with a kind no enumerator has), the kind and the
 * truth values. */
as_named as_named_make(int which);
long long as_named_length(const as_named *n);
long long as_named_by_value(as_named n);
as_deep as_deep_make(int which);
long long as_deep_check(as_deep d);
void as_named_fill(as_named *n, int which);
void as_named_rename(as_named *n);

/* Arrays of structs read through `const` pointers, and arrays C allocates and answers through `out`
 * pointers, freed by `as_glyphs_free`, which counts what it frees. */
typedef struct { unsigned long index; double x, y; } as_glyph;

double as_glyphs_sum(const as_glyph *glyphs, int count);
long long as_names_total(const as_named *names, size_t count);
double as_rects_area(size_t count, const as_rect *rects);
int as_glyphs_make(int n, as_glyph **glyphs, int *count);
void as_glyphs_answer(int n, as_glyph **glyphs, long *count);
void as_glyphs_free(as_glyph *glyphs);
int as_glyphs_allocated(void);
int as_glyphs_freed(void);
void as_reals_make(int n, double **values, size_t *count);
void as_reals_free(double *values);
void as_names_make(int n, as_named **names, int *count);
void as_bad_answer(int which, as_point **points, int *count);

/* Arrays C fills, the caller allocating as many elements as another call says: a dash pattern whose
 * length `as_dash_count` answers, as Cairo's is; glyphs filled with a status, failing for 2; and two
 * arrays filled in one call, `n` integers and `2n` bytes. `as_dash_calls` counts the calls made. */
void as_dash_set(int n);
int as_dash_count(void);
int as_dash_calls(void);
void as_dash_get(double *dashes, double *offset);
int as_glyphs_fill(int n, as_glyph *glyphs);
int as_spread(int n, int32_t *ints, uint8_t *bytes);
/* An array C fills and one it allocates in one call: `n` integers each, and `n` halves. */
int as_mixed(int n, int32_t *filled, double **made, int *count);

/* Several arrays C allocates in one call, each with its own count, freed by its own function, the
 * clusters and flags optional, as Cairo's `cairo_scaled_font_text_to_glyphs` answers them: `n`
 * glyphs and `(n + 1) / 2` clusters, failing with status 7 for a negative `n` once both are
 * allocated. `as_shape_nulls` counts the calls passed null for all three of clusters, their count
 * and the flags. */
typedef struct { int32_t num_bytes; int32_t num_glyphs; } as_cluster;
typedef enum { AS_CLUSTER_FORWARD = 0, AS_CLUSTER_BACKWARD = 1 } as_cluster_flags;

int as_shape(const char *text, int n, as_glyph **glyphs, int *num_glyphs, as_cluster **clusters, int *num_clusters, as_cluster_flags *flags);
void as_clusters_free(as_cluster *clusters);
int as_clusters_allocated(void);
int as_clusters_freed(void);
int as_shape_nulls(void);

/* Opaque values: a union and a struct with a bit-field, which no mirror holds. */
typedef union { int32_t i; float f; void *p; } as_event;
typedef struct { int32_t kind; unsigned bits : 4; unsigned more : 12; } as_flags;

as_event as_event_int(int32_t i);
as_event as_event_float(float f);
int32_t as_event_to_int(as_event self);
float as_event_to_float(as_event self);
as_event as_event_negate(as_event self);
as_flags as_flags_make(int32_t kind, unsigned bits);
unsigned as_flags_bits(as_flags self);
int32_t as_flags_kind(as_flags self);

/* Calls that wait for avin_arrays.h's flag, then answer as their counterparts above do; -1 where
 * they gave up. */
as_rect as_wait_grow(as_rect r, double by, int timeout_ms);
long long as_wait_named(const as_named *n, int timeout_ms);
int32_t as_wait_event(as_event e, int timeout_ms);
double as_wait_glyphs(const as_glyph *glyphs, int count, int timeout_ms);

/* Structs no mirror matches, for the header check. */
typedef struct { int32_t a; int32_t b; } as_pair;
typedef struct { char a; char hidden; int32_t c; } as_hidden;
typedef struct { int32_t values[4]; } as_with_array;
typedef struct { int32_t bits : 3; int32_t rest; } as_bits;
typedef int as_not_struct;
typedef union { int32_t i; float f; } as_some_union;
typedef double as_scalar_value;

int as_pair_sum(as_pair p);

#endif
