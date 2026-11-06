/* Functions that call back, for the C binding tests: every scalar kind a callback is called with and
 * answers, strings, enumerations and lent objects passed to one, calls made zero, one and many times,
 * from the calling thread and from threads of their own, a sort and a search of the C library's own
 * through a comparator, and a registry that keeps its listeners and counts every listener it keeps,
 * forgets and is freed with, stopping the process on a double free. */
#ifndef AVIN_CALLBACKS_H
#define AVIN_CALLBACKS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int32_t cb_count;

typedef enum cb_level { CB_LOW = -1, CB_MID = 0, CB_HIGH = 7 } cb_level;

/* Calls `f` with 0, 1, … `n - 1` and answers the sum of its answers. */
long long cb_repeat(int n, int (*f)(void *ud, int i), void *ud);

/* Each answers `f`'s answer for `x`, its own scalar type both ways. */
int8_t cb_i8(int8_t (*f)(void *ud, int8_t x), void *ud, int8_t x);
uint8_t cb_u8(uint8_t (*f)(void *ud, uint8_t x), void *ud, uint8_t x);
int16_t cb_i16(int16_t (*f)(void *ud, int16_t x), void *ud, int16_t x);
uint16_t cb_u16(uint16_t (*f)(void *ud, uint16_t x), void *ud, uint16_t x);
int32_t cb_i32(int32_t (*f)(void *ud, int32_t x), void *ud, int32_t x);
uint32_t cb_u32(uint32_t (*f)(void *ud, uint32_t x), void *ud, uint32_t x);
int64_t cb_i64(int64_t (*f)(void *ud, int64_t x), void *ud, int64_t x);
uint64_t cb_u64(uint64_t (*f)(void *ud, uint64_t x), void *ud, uint64_t x);
size_t cb_size(size_t (*f)(void *ud, size_t x), void *ud, size_t x);
cb_count cb_typedef(cb_count (*f)(void *ud, cb_count x), void *ud, cb_count x);
double cb_double(double (*f)(void *ud, double x), void *ud, double x);
float cb_float(float (*f)(void *ud, float x), void *ud, float x);
bool cb_bool(bool (*f)(void *ud, bool x), void *ud, bool x);
int cb_truth(int (*f)(void *ud, int x), void *ud, int x);
unsigned char cb_byte(unsigned char (*f)(void *ud, unsigned char x), void *ud, unsigned char x);
cb_level cb_level_of(cb_level (*f)(void *ud, cb_level x), void *ud, int x);
/* Calls `f` with the `int` `x` and answers 0, so `f`'s own answer is the only result. */
void cb_void(void (*f)(void *ud, int x), void *ud, int x);
/* Calls `f` with several parameters at once, the `userdata` between them. */
double cb_mixed(double (*f)(int a, void *ud, double b, long long c, bool d), int a, double b, long long c, bool d, void *ud);

/* Calls `f` with `s`, or null where `null` is nonzero, and answers what it answers. */
int cb_string(int (*f)(void *ud, const char *s), void *ud, const char *s, int null);
/* Calls `f` with bytes that are not UTF-8. */
int cb_bad_string(int (*f)(void *ud, const char *s), void *ud);

/* Sorts `values` with the C library's `qsort_r`, through `compare`. */
void cb_sort_ints(int *values, size_t n, int (*compare)(void *ud, int a, int b), void *ud);
/* Finds `key` in sorted `values` with the C library's `bsearch`, through `compare`: its index, or -1. */
long long cb_search_ints(const int *values, size_t n, int key, int (*compare)(void *ud, int a, int b), void *ud);

/* Calls `f` with `x` on a thread of its own, waits for it and answers what `f` answered. */
int cb_on_thread(int (*f)(void *ud, int x), void *ud, int x);
/* Calls `f` with `x` on `threads` threads of their own at once, `calls` times each, and answers the
 * sum of what it answered. */
long long cb_on_threads(int (*f)(void *ud, int x), void *ud, int x, int threads, int calls);

/* A registry of listeners: it keeps each until it is forgotten, and calls every one it holds when it
 * fires. Its counts are of the process, atomic. */
typedef struct cb_registry cb_registry;
cb_registry *cb_registry_new(void);
cb_registry *cb_registry_ref(cb_registry *r);
void cb_registry_unref(cb_registry *r);
int cb_registry_listen(cb_registry *r, void (*listener)(void *ud, int value), void *ud);
void cb_registry_forget(cb_registry *r, void *ud);
int cb_registry_fire(cb_registry *r, int value);
/* Fires on a thread of its own, and waits for it. */
int cb_registry_fire_on_thread(cb_registry *r, int value);
int cb_registry_listeners(const cb_registry *r);
/* Calls `f` with the registry itself, lent, or with null where `null` is nonzero. */
int cb_registry_visit(cb_registry *r, int (*f)(void *ud, cb_registry *self), void *ud, int null);
/* What the process has done: 0 registries made, 1 freed, 2 listeners kept, 3 forgotten. */
long cb_registry_count(int what);

#endif
