#include "avin_callbacks.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

long long cb_repeat(int n, int (*f)(void *ud, int i), void *ud) {
    long long sum = 0;
    for (int i = 0; i < n; i++) sum += f(ud, i);
    return sum;
}

int8_t cb_i8(int8_t (*f)(void *ud, int8_t x), void *ud, int8_t x) { return f(ud, x); }
uint8_t cb_u8(uint8_t (*f)(void *ud, uint8_t x), void *ud, uint8_t x) { return f(ud, x); }
int16_t cb_i16(int16_t (*f)(void *ud, int16_t x), void *ud, int16_t x) { return f(ud, x); }
uint16_t cb_u16(uint16_t (*f)(void *ud, uint16_t x), void *ud, uint16_t x) { return f(ud, x); }
int32_t cb_i32(int32_t (*f)(void *ud, int32_t x), void *ud, int32_t x) { return f(ud, x); }
uint32_t cb_u32(uint32_t (*f)(void *ud, uint32_t x), void *ud, uint32_t x) { return f(ud, x); }
int64_t cb_i64(int64_t (*f)(void *ud, int64_t x), void *ud, int64_t x) { return f(ud, x); }
uint64_t cb_u64(uint64_t (*f)(void *ud, uint64_t x), void *ud, uint64_t x) { return f(ud, x); }
size_t cb_size(size_t (*f)(void *ud, size_t x), void *ud, size_t x) { return f(ud, x); }
cb_count cb_typedef(cb_count (*f)(void *ud, cb_count x), void *ud, cb_count x) { return f(ud, x); }
double cb_double(double (*f)(void *ud, double x), void *ud, double x) { return f(ud, x); }
float cb_float(float (*f)(void *ud, float x), void *ud, float x) { return f(ud, x); }
bool cb_bool(bool (*f)(void *ud, bool x), void *ud, bool x) { return f(ud, x); }
int cb_truth(int (*f)(void *ud, int x), void *ud, int x) { return f(ud, x); }
unsigned char cb_byte(unsigned char (*f)(void *ud, unsigned char x), void *ud, unsigned char x) { return f(ud, x); }
cb_level cb_level_of(cb_level (*f)(void *ud, cb_level x), void *ud, int x) { return f(ud, (cb_level)x); }
void cb_void(void (*f)(void *ud, int x), void *ud, int x) { f(ud, x); }
double cb_mixed(double (*f)(int a, void *ud, double b, long long c, bool d), int a, double b, long long c, bool d, void *ud) { return f(a, ud, b, c, d); }

int cb_string(int (*f)(void *ud, const char *s), void *ud, const char *s, int null) { return f(ud, null ? 0 : s); }

int cb_bad_string(int (*f)(void *ud, const char *s), void *ud) {
    static const char bad[] = { 'o', 'k', (char)0xC3, 0 };
    return f(ud, bad);
}

struct cb_compare { int (*compare)(void *, int, int); void *ud; };

#if defined(__APPLE__)
static int cb_sort_thunk(void *t, const void *a, const void *b) {
    struct cb_compare *c = t;
    return c->compare(c->ud, *(const int *)a, *(const int *)b);
}
#else
static int cb_sort_thunk(const void *a, const void *b, void *t) {
    struct cb_compare *c = t;
    return c->compare(c->ud, *(const int *)a, *(const int *)b);
}
#endif

void cb_sort_ints(int *values, size_t n, int (*compare)(void *ud, int a, int b), void *ud) {
    struct cb_compare c = { compare, ud };
#if defined(__APPLE__)
    qsort_r(values, n, sizeof(int), &c, cb_sort_thunk);
#else
    qsort_r(values, n, sizeof(int), cb_sort_thunk, &c);
#endif
}

static _Thread_local struct cb_compare *cb_searching;

static int cb_search_thunk(const void *key, const void *element) {
    return cb_searching->compare(cb_searching->ud, *(const int *)key, *(const int *)element);
}

long long cb_search_ints(const int *values, size_t n, int key, int (*compare)(void *ud, int a, int b), void *ud) {
    struct cb_compare c = { compare, ud };
    struct cb_compare *outer = cb_searching;
    cb_searching = &c;
    const int *found = bsearch(&key, values, n, sizeof(int), cb_search_thunk);
    cb_searching = outer;
    return found ? (long long)(found - values) : -1;
}

struct cb_call { int (*f)(void *, int); void *ud; int x; int calls; long long answer; };

static void *cb_call_thread(void *p) {
    struct cb_call *c = p;
    c->answer = 0;
    for (int i = 0; i < c->calls; i++) c->answer += c->f(c->ud, c->x);
    return 0;
}

int cb_on_thread(int (*f)(void *ud, int x), void *ud, int x) {
    struct cb_call c = { f, ud, x, 1, 0 };
    pthread_t t;
    if (pthread_create(&t, 0, cb_call_thread, &c) != 0) abort();
    pthread_join(t, 0);
    return (int)c.answer;
}

long long cb_on_threads(int (*f)(void *ud, int x), void *ud, int x, int threads, int calls) {
    struct cb_call c[16];
    pthread_t t[16];
    if (threads > 16) threads = 16;
    for (int i = 0; i < threads; i++) {
        c[i] = (struct cb_call){ f, ud, x, calls, 0 };
        if (pthread_create(&t[i], 0, cb_call_thread, &c[i]) != 0) abort();
    }
    long long sum = 0;
    for (int i = 0; i < threads; i++) {
        pthread_join(t[i], 0);
        sum += c[i].answer;
    }
    return sum;
}

struct cb_listener { void (*f)(void *, int); void *ud; struct cb_listener *next; };

struct cb_registry {
    atomic_int refs;
    int alive;
    pthread_mutex_t lock;
    struct cb_listener *listeners;
};

static atomic_long cb_counts[4];

long cb_registry_count(int what) { return atomic_load(&cb_counts[what]); }

cb_registry *cb_registry_new(void) {
    cb_registry *r = calloc(1, sizeof *r);
    if (!r) abort();
    atomic_store(&r->refs, 1);
    r->alive = 1;
    pthread_mutex_init(&r->lock, 0);
    atomic_fetch_add(&cb_counts[0], 1);
    return r;
}

static void cb_registry_live(const cb_registry *r, const char *what) {
    if (!r->alive) {
        fprintf(stderr, "avin_callbacks: %s of a freed registry\n", what);
        abort();
    }
}

cb_registry *cb_registry_ref(cb_registry *r) {
    cb_registry_live(r, "a reference");
    atomic_fetch_add(&r->refs, 1);
    return r;
}

void cb_registry_unref(cb_registry *r) {
    cb_registry_live(r, "a release");
    if (atomic_fetch_sub(&r->refs, 1) != 1) return;
    if (r->listeners) {
        fprintf(stderr, "avin_callbacks: a registry freed with a listener it was never told to forget\n");
        abort();
    }
    r->alive = 0;
    atomic_fetch_add(&cb_counts[1], 1);
    /* The memory is kept, so a use after the free is caught. */
}

int cb_registry_listen(cb_registry *r, void (*listener)(void *ud, int value), void *ud) {
    cb_registry_live(r, "a listen");
    struct cb_listener *l = malloc(sizeof *l);
    if (!l) abort();
    pthread_mutex_lock(&r->lock);
    for (struct cb_listener *k = r->listeners; k; k = k->next)
        if (k->ud == ud) {
            fprintf(stderr, "avin_callbacks: one userdata listened twice\n");
            abort();
        }
    l->f = listener;
    l->ud = ud;
    l->next = r->listeners;
    r->listeners = l;
    pthread_mutex_unlock(&r->lock);
    atomic_fetch_add(&cb_counts[2], 1);
    return 0;
}

void cb_registry_forget(cb_registry *r, void *ud) {
    cb_registry_live(r, "a forget");
    pthread_mutex_lock(&r->lock);
    struct cb_listener **at = &r->listeners;
    while (*at && (*at)->ud != ud) at = &(*at)->next;
    if (!*at) {
        fprintf(stderr, "avin_callbacks: a listener forgotten twice, or never listened\n");
        abort();
    }
    struct cb_listener *l = *at;
    *at = l->next;
    pthread_mutex_unlock(&r->lock);
    free(l);
    atomic_fetch_add(&cb_counts[3], 1);
}

int cb_registry_fire(cb_registry *r, int value) {
    cb_registry_live(r, "a fire");
    int n = 0;
    for (struct cb_listener *k = r->listeners; k; k = k->next) {
        k->f(k->ud, value);
        n++;
    }
    return n;
}

struct cb_firing { cb_registry *r; int value; int fired; };

static void *cb_fire_thread(void *p) {
    struct cb_firing *f = p;
    f->fired = cb_registry_fire(f->r, f->value);
    return 0;
}

int cb_registry_fire_on_thread(cb_registry *r, int value) {
    struct cb_firing f = { r, value, 0 };
    pthread_t t;
    if (pthread_create(&t, 0, cb_fire_thread, &f) != 0) abort();
    pthread_join(t, 0);
    return f.fired;
}

int cb_registry_listeners(const cb_registry *r) {
    int n = 0;
    for (struct cb_listener *k = r->listeners; k; k = k->next) n++;
    return n;
}

int cb_registry_visit(cb_registry *r, int (*f)(void *ud, cb_registry *self), void *ud, int null) {
    cb_registry_live(r, "a visit");
    return f(ud, null ? 0 : r);
}
