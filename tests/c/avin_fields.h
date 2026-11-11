/* The C objects the field and conversion tests bind. A chain of structs models C's inheritance, each
 * with its parent as its first member, as MLT's producers, services and property bags do; a record
 * publishes fields of every kind a field accessor reads or writes. Both are counted as the objects of
 * avin_handles.h are, and stop the process on a use after free; their memory is never reused. The test
 * harness compiles this into the `avin-c-fixture` library beside avin_handles.c. */
#ifndef AVIN_FIELDS_H
#define AVIN_FIELDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The kinds counted, and what is counted of each, for `fxf_count`. */
enum fxf_kind { FXF_BASE = 0, FXF_RECORD = 1, FXF_HOLDER = 2 };
enum fxf_what { FXF_CREATED = 0, FXF_RETAINED = 1, FXF_RELEASED = 2, FXF_FREED = 3 };
long fxf_count(int kind, int what);

/* The chain: a base holding the count, a middle whose first member is a base, a derived whose first
 * member is a middle. Each is made with one reference, and freed with the last, whichever type it was
 * made as. */
struct fxf_base_s {
    int magic;
    int refs;
    int value;
};
typedef struct fxf_base_s *fxf_base;
struct fxf_middle_s {
    struct fxf_base_s base;
    int extra;
};
typedef struct fxf_middle_s *fxf_middle;
struct fxf_derived_s {
    struct fxf_middle_s middle;
    int more;
};
typedef struct fxf_derived_s *fxf_derived;

fxf_base fxf_base_new(int value);
fxf_middle fxf_middle_new(int value, int extra);
fxf_derived fxf_derived_new(int value, int extra, int more);
/* The conversions up the chain, as MLT's `mlt_producer_service` is: the same object, lent. */
fxf_middle fxf_derived_middle(fxf_derived d);
fxf_base fxf_middle_base(fxf_middle m);
int fxf_base_inc_ref(fxf_base b);
void fxf_base_close(fxf_base b);
int fxf_base_value(fxf_base b);
void fxf_base_set(fxf_base b, int value);
int fxf_base_refs(fxf_base b);
/* The value, or `fallback` for null. */
int fxf_base_value_or(fxf_base b, int fallback);
int fxf_middle_extra(fxf_middle m);
int fxf_derived_more(fxf_derived d);
/* Whether two pointers are the same object. */
bool fxf_same(fxf_base a, fxf_base b);

/* A holder keeping a pointer to a base without a reference of its own, which it reads when asked
 * and never when it is freed. */
typedef struct fxf_holder fxf_holder;
fxf_holder *fxf_holder_new(fxf_base b);
void fxf_holder_free(fxf_holder *h);
int fxf_holder_peek(const fxf_holder *h);
/* A sink taking the reference it is passed, given back by `fxf_sink_drain`. */
void fxf_sink_take(fxf_base b);
int fxf_sink_drain(void);

/* A record with a field of each kind. */
enum fxf_shade { FXF_LIGHT = 1, FXF_DARK = -3 };
struct fxf_point {
    int x;
    int y;
};
/* The kinds a cell's header holds, as Cairo's `cairo_path_data_type_t` does. */
enum fxf_path_kind { FXF_MOVE_TO, FXF_LINE_TO, FXF_CURVE_TO, FXF_CLOSE_PATH };
/* A union of a header and a point, as Cairo's `cairo_path_data_t` is. */
union fxf_cell {
    struct {
        int kind;
        int length;
    } header;
    struct {
        double x;
        double y;
    } point;
};
/* Structs read and written whole through fields: a box, as FreeType's `FT_BBox` is; a size's
 * metrics, holding another struct and an enum, reached through a pointer, as `FT_Size_Metrics` is
 * through `face->size`, here `record->scaled`; and a struct holding a string, which is only read. */
typedef struct fxf_box {
    long x_min;
    long y_min;
    long x_max;
    long y_max;
} fxf_box;
struct fxf_metrics {
    unsigned short x_ppem;
    long ascender;
    struct fxf_point origin;
    enum fxf_shade shade;
};
struct fxf_size {
    struct fxf_metrics metrics;
};
struct fxf_labelled {
    const char *label;
    int id;
};
/* Typedefs of `char`, as FreeType's `FT_String` is, one through another and one `const`, and of the
 * byte types, which are no text. */
typedef char fxf_string;
typedef fxf_string fxf_string2;
typedef const char fxf_cstring;
typedef unsigned char fxf_ubyte;
typedef signed char fxf_sbyte;
/* A struct whose name is bytes, which no mirror reads as a string. */
struct fxf_raw_named {
    const fxf_ubyte *name;
    int id;
};
typedef struct fxf_record fxf_record;
struct fxf_record {
    int magic;
    int refs;
    int8_t tiny;
    uint8_t small;
    int16_t i16;
    uint16_t u16;
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;
    long along;
    size_t size;
    bool flag;
    int truth;
    char letter;
    float single;
    double precise;
    enum fxf_shade shade;
    const char *name;
    char *label;
    const int fixed;
    char text[8];
    struct fxf_point origin;
    struct fxf_point *corner;
    int count;
    int *values;
    int rows;
    int columns;
    int grid[3][4];
    int num_cells;
    union fxf_cell *cells;
    int tag_count;
    const char **tags;
    fxf_record *next;
    fxf_base base;
    fxf_derived derived;
    fxf_box box;
    struct fxf_size *scaled;
    struct fxf_labelled labelled;
    struct fxf_point corners[3];
    const struct fxf_point anchor;
    const fxf_string *family_name;
    fxf_string2 *style_name;
    fxf_ubyte *raw;
    const fxf_sbyte *signed_raw;
};
/* A record with every field set to a fixed sample: see avin_fields.c. */
fxf_record *fxf_record_new(void);
fxf_record *fxf_record_ref(fxf_record *r);
void fxf_record_unref(fxf_record *r);
/* Gives `r` a next record, and the bases its handle fields point at, each holding a reference the
 * record gives back when it is freed. */
void fxf_record_link(fxf_record *r, fxf_record *next, fxf_base base, fxf_derived derived);
/* Sets the name and label to null, or to bytes that are not UTF-8. */
void fxf_record_clear_text(fxf_record *r);
void fxf_record_bad_text(fxf_record *r);
/* Sets the unsigned 64-bit field to its greatest, past every signed integer's. */
void fxf_record_u64_max(fxf_record *r);
/* Copies the record's `count` values into memory the caller allocates, as Cairo's `cairo_get_dash`
 * does; `fxf_record_set_count` changes how many there are, no more than five. */
void fxf_record_copy_values(const fxf_record *r, int *into);
void fxf_record_set_count(fxf_record *r, int count);
/* The values a writer stored, read back by C. */
long long fxf_record_sum(const fxf_record *r);
/* The record's box, size metrics and corners as text, read by C, in a buffer of the record's own. */
const char *fxf_record_shapes(fxf_record *r);
/* Stores a shade no variant holds into the size's metrics. */
void fxf_record_bad_metrics(fxf_record *r);
/* Strings through typedefs of `char`: the family and style names, the length of a string passed as
 * one, `const` or not, and the family name written back through an `out` pointer. */
const fxf_string *fxf_family(const fxf_record *r);
fxf_string2 *fxf_style(const fxf_record *r);
long fxf_string_length(const fxf_string *s);
long fxf_string2_length(fxf_string2 const *s);
long fxf_cstring_length(fxf_cstring *s);
long fxf_mutable_length(fxf_string *s);
void fxf_family_out(const fxf_record *r, const fxf_string **name);
/* Bytes, no text: the record's raw bytes, and the length of a run of signed ones. */
const fxf_ubyte *fxf_raw(const fxf_record *r);
long fxf_sbyte_length(const fxf_sbyte *s);
/* Calls `f` with the family name, as a `const fxf_string *`, and answers what it answered. */
int fxf_with_family(const fxf_record *r, int (*f)(const fxf_string *name, void *userdata), void *userdata);

#endif
