/* What `sqlite.sysl` cannot say itself.
 *
 * `sqlite3_bind_text64` and `sqlite3_bind_blob64` take a destructor, and the
 * one that asks SQLite to copy the bytes before the call returns is
 * `SQLITE_TRANSIENT`, the cast `((sqlite3_destructor_type)-1)`: a function
 * pointer no signature describes. Each call is made here, so the Sysl side hands
 * over a pointer and a length and keeps nothing alive past the call.
 *
 * The rest is how a statement the collector frees is finalized without waiting.
 * The collector runs a handle's finalizer on whichever thread collects, while the
 * statement's connection may be in a call on another thread, a step waiting out
 * a lock among them, holding the connection's mutex all the while. Finalizing
 * there would hold up the collection, and with it every thread, until that call
 * returned. So a statement whose connection is busy is put on the connection's
 * pending list, under a lock of its own that is never held across a wait, and the
 * connection finalizes the list at its next call.
 */
#include <sqlite3.h>
#include <stdlib.h>

typedef struct avin_sqlite_pending {
    sqlite3_mutex *lock;
    sqlite3_stmt **items;
    int count;
    int capacity;
} avin_sqlite_pending;

avin_sqlite_pending *avin_sqlite_pending_new(void) {
    avin_sqlite_pending *p = calloc(1, sizeof *p);
    if (p == NULL) return NULL;
    p->lock = sqlite3_mutex_alloc(SQLITE_MUTEX_FAST);
    return p;
}

/* Finalizes `stmt` now where its connection is idle, and otherwise leaves it for
 * the connection's next call. Where the list cannot grow, it waits for the
 * connection rather than leak the statement. */
void avin_sqlite_release(sqlite3 *db, avin_sqlite_pending *p, sqlite3_stmt *stmt) {
    sqlite3_mutex *m = sqlite3_db_mutex(db);
    if (m == NULL || p == NULL || sqlite3_mutex_try(m) == SQLITE_OK) {
        sqlite3_finalize(stmt);
        if (m != NULL && p != NULL) sqlite3_mutex_leave(m);
        return;
    }
    sqlite3_mutex_enter(p->lock);
    if (p->count == p->capacity) {
        int grown = p->capacity == 0 ? 8 : p->capacity * 2;
        sqlite3_stmt **items = realloc(p->items, (size_t)grown * sizeof *items);
        if (items == NULL) {
            sqlite3_mutex_leave(p->lock);
            sqlite3_finalize(stmt);
            return;
        }
        p->items = items;
        p->capacity = grown;
    }
    p->items[p->count++] = stmt;
    sqlite3_mutex_leave(p->lock);
}

/* Finalizes every statement left pending, from the connection's own thread. */
void avin_sqlite_drain(avin_sqlite_pending *p) {
    if (p == NULL) return;
    for (;;) {
        sqlite3_mutex_enter(p->lock);
        if (p->count == 0) {
            sqlite3_mutex_leave(p->lock);
            return;
        }
        sqlite3_stmt *stmt = p->items[--p->count];
        sqlite3_mutex_leave(p->lock);
        sqlite3_finalize(stmt);
    }
}

/* Finalizes what is pending and frees the list, as the connection closes. */
void avin_sqlite_pending_free(avin_sqlite_pending *p) {
    if (p == NULL) return;
    avin_sqlite_drain(p);
    sqlite3_mutex_free(p->lock);
    free(p->items);
    free(p);
}

/* Text of `n` bytes of UTF-8, copied by SQLite. The length is given, so text
 * holding a zero byte is bound whole. An empty text is bound from the address of
 * a byte SQLite does not read: a null pointer would bind NULL instead. */
int avin_sqlite_bind_text(sqlite3_stmt *stmt, int index, const char *value, long long n) {
    static const char empty = 0;
    if (n <= 0) {
        value = &empty;
        n = 0;
    }
    return sqlite3_bind_text64(stmt, index, value, (sqlite3_uint64)n, SQLITE_TRANSIENT, SQLITE_UTF8);
}

/* A blob of `n` bytes, copied by SQLite. An empty blob is a value, not NULL, so it
 * is bound from a byte's address as empty text is. */
int avin_sqlite_bind_blob(sqlite3_stmt *stmt, int index, const void *value, long long n) {
    static const char empty = 0;
    if (n <= 0) {
        value = &empty;
        n = 0;
    }
    return sqlite3_bind_blob64(stmt, index, value, (sqlite3_uint64)n, SQLITE_TRANSIENT);
}
