/* The chain and the record of the field and conversion tests: see avin_fields.h. */
#include "avin_fields.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long counts[3][4];

static void count(int kind, int what) { __atomic_fetch_add(&counts[kind][what], 1, __ATOMIC_SEQ_CST); }

long fxf_count(int kind, int what) { return __atomic_load_n(&counts[kind][what], __ATOMIC_SEQ_CST); }

static void stop(const char *what) {
    fprintf(stderr, "fixture: %s\n", what);
    fflush(stderr);
    abort();
}

#define LIVE 0x4c495645
#define DEAD 0x44454144

static void check_base(const struct fxf_base_s *b, const char *what) {
    if (!b) stop("a base is null");
    if (b->magic == DEAD) stop(what);
    if (b->magic != LIVE) stop("not a base");
}

static void *made(size_t size) {
    void *p = calloc(1, size);
    if (!p) stop("out of memory");
    return p;
}

static void start_base(struct fxf_base_s *b, int value) {
    b->magic = LIVE;
    b->refs = 1;
    b->value = value;
    count(FXF_BASE, FXF_CREATED);
}

fxf_base fxf_base_new(int value) {
    fxf_base b = made(sizeof *b);
    start_base(b, value);
    return b;
}

fxf_middle fxf_middle_new(int value, int extra) {
    fxf_middle m = made(sizeof *m);
    start_base(&m->base, value);
    m->extra = extra;
    return m;
}

fxf_derived fxf_derived_new(int value, int extra, int more) {
    fxf_derived d = made(sizeof *d);
    start_base(&d->middle.base, value);
    d->middle.extra = extra;
    d->more = more;
    return d;
}

fxf_middle fxf_derived_middle(fxf_derived d) {
    check_base(&d->middle.base, "a derived converted after it was freed");
    return &d->middle;
}

fxf_base fxf_middle_base(fxf_middle m) {
    check_base(&m->base, "a middle converted after it was freed");
    return &m->base;
}

int fxf_base_inc_ref(fxf_base b) {
    check_base(b, "a reference taken to a freed base");
    count(FXF_BASE, FXF_RETAINED);
    return __atomic_add_fetch(&b->refs, 1, __ATOMIC_SEQ_CST);
}

/* The memory is never freed, so a later use is always caught. */
void fxf_base_close(fxf_base b) {
    check_base(b, "a base given back after it was freed");
    count(FXF_BASE, FXF_RELEASED);
    if (__atomic_sub_fetch(&b->refs, 1, __ATOMIC_SEQ_CST) == 0) {
        b->magic = DEAD;
        count(FXF_BASE, FXF_FREED);
    }
}

int fxf_base_value(fxf_base b) {
    check_base(b, "a base read after it was freed");
    return b->value;
}

void fxf_base_set(fxf_base b, int value) {
    check_base(b, "a base written after it was freed");
    b->value = value;
}

int fxf_base_refs(fxf_base b) {
    check_base(b, "a base's count read after it was freed");
    return __atomic_load_n(&b->refs, __ATOMIC_SEQ_CST);
}

int fxf_base_value_or(fxf_base b, int fallback) { return b ? fxf_base_value(b) : fallback; }

int fxf_middle_extra(fxf_middle m) {
    check_base(&m->base, "a middle read after it was freed");
    return m->extra;
}

int fxf_derived_more(fxf_derived d) {
    check_base(&d->middle.base, "a derived read after it was freed");
    return d->more;
}

bool fxf_same(fxf_base a, fxf_base b) { return a == b; }

struct fxf_holder {
    fxf_base held;
};

fxf_holder *fxf_holder_new(fxf_base b) {
    check_base(b, "a freed base held");
    fxf_holder *h = made(sizeof *h);
    h->held = b;
    count(FXF_HOLDER, FXF_CREATED);
    return h;
}

void fxf_holder_free(fxf_holder *h) {
    count(FXF_HOLDER, FXF_FREED);
    free(h);
}

int fxf_holder_peek(const fxf_holder *h) { return fxf_base_value(h->held); }

static fxf_base sunk[64];
static int sunk_count;

void fxf_sink_take(fxf_base b) {
    check_base(b, "a freed base taken");
    if (sunk_count == 64) stop("the sink is full");
    sunk[sunk_count++] = b;
}

int fxf_sink_drain(void) {
    int n = sunk_count;
    while (sunk_count > 0) fxf_base_close(sunk[--sunk_count]);
    return n;
}

static void check_record(const fxf_record *r, const char *what) {
    if (!r) stop("a record is null");
    if (r->magic == DEAD) stop(what);
    if (r->magic != LIVE) stop("not a record");
}

static const char *const tag_texts[3] = {"a", NULL, "ccc"};

fxf_record *fxf_record_new(void) {
    fxf_record sample = {
        .magic = LIVE,
        .refs = 1,
        .tiny = INT8_MIN,
        .small = UINT8_MAX,
        .i16 = INT16_MIN,
        .u16 = UINT16_MAX,
        .i32 = INT32_MIN,
        .u32 = UINT32_MAX,
        .i64 = INT64_MIN,
        .u64 = INT64_MAX,
        .along = 1234567890123L,
        .size = 4096,
        .flag = true,
        .truth = -1,
        .letter = 'A',
        .single = 1.5f,
        .precise = -2.25,
        .shade = FXF_DARK,
        .name = "nombre \xc3\xbc \xe6\x97\xa5\xe6\x9c\xac \xf0\x9f\x98\x80",
        .fixed = 7,
        .text = "seven",
        .origin = {3, -4},
        .count = 5,
        .rows = 3,
        .columns = 4,
        .num_cells = 3,
        .tag_count = 3,
    };
    fxf_record *r = made(sizeof *r);
    memcpy(r, &sample, sizeof *r);
    r->label = strdup("label");
    r->corner = made(sizeof *r->corner);
    r->corner->x = 10;
    r->corner->y = 20;
    r->values = made(5 * sizeof *r->values);
    for (int i = 0; i < 5; i++) r->values[i] = (i + 1) * 10;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++) r->grid[i][j] = i * 10 + j;
    r->cells = made(3 * sizeof *r->cells);
    r->cells[0].header.kind = 0;
    r->cells[0].header.length = 2;
    r->cells[1].point.x = 1.5;
    r->cells[1].point.y = -2.5;
    r->cells[2].header.kind = 3;
    r->cells[2].header.length = 1;
    r->tags = (const char **)tag_texts;
    count(FXF_RECORD, FXF_CREATED);
    return r;
}

fxf_record *fxf_record_ref(fxf_record *r) {
    check_record(r, "a reference taken to a freed record");
    __atomic_fetch_add(&r->refs, 1, __ATOMIC_SEQ_CST);
    count(FXF_RECORD, FXF_RETAINED);
    return r;
}

void fxf_record_unref(fxf_record *r) {
    check_record(r, "a record given back after it was freed");
    count(FXF_RECORD, FXF_RELEASED);
    if (__atomic_sub_fetch(&r->refs, 1, __ATOMIC_SEQ_CST) == 0) {
        r->magic = DEAD;
        count(FXF_RECORD, FXF_FREED);
        if (r->next) fxf_record_unref(r->next);
        if (r->base) fxf_base_close(r->base);
        if (r->derived) fxf_base_close(&r->derived->middle.base);
    }
}

void fxf_record_link(fxf_record *r, fxf_record *next, fxf_base base, fxf_derived derived) {
    check_record(r, "a freed record linked");
    if (next) fxf_record_ref(next);
    if (base) fxf_base_inc_ref(base);
    if (derived) fxf_base_inc_ref(&derived->middle.base);
    r->next = next;
    r->base = base;
    r->derived = derived;
}

void fxf_record_clear_text(fxf_record *r) {
    r->name = NULL;
    r->label = NULL;
}

void fxf_record_bad_text(fxf_record *r) {
    r->name = "ok \xff no";
    r->label = NULL;
}

void fxf_record_u64_max(fxf_record *r) { r->u64 = UINT64_MAX; }

long long fxf_record_sum(const fxf_record *r) {
    check_record(r, "a freed record summed");
    long long sum = (long long)r->tiny + r->small + r->i16 + r->u16 + r->i32 + r->u32 + r->letter + r->truth;
    sum += r->origin.x + r->origin.y + r->corner->x + r->corner->y + r->grid[2][3];
    for (int i = 0; i < r->count; i++) sum += r->values[i];
    sum += (long long)(r->single * 100) + (long long)(r->precise * 100) + (r->flag ? 1000 : 0);
    return sum;
}
