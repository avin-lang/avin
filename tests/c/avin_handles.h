/* The C objects the handle tests bind: each kind counts what is made of it, every reference taken
 * and given back, and every object freed, and stops the process on a double free or a use after
 * free, so a binding that gets ownership wrong fails loudly rather than by chance. A freed object's
 * memory is never reused, so the check that it is dead is always safe to make. The test harness
 * compiles it into the `avin-c-fixture` library beside avin_fixture.c. */
#ifndef AVIN_HANDLES_H
#define AVIN_HANDLES_H

/* The kinds of object counted, and what is counted of each, for `fx_count`. */
enum fx_kind { FX_OBJECT = 0, FX_LIBRARY = 1, FX_FACE = 2, FX_MMVAR = 3, FX_PLAIN = 4, FX_SHELF = 5, FX_SHAPE = 6 };
enum fx_what { FX_CREATED = 0, FX_RETAINED = 1, FX_RELEASED = 2, FX_FREED = 3, FX_AFTER = 4, FX_BEFORE = 5 };
/* For faces, FX_AFTER counts those freed once their library's other references had gone, the library
 * holding only the face's own, and FX_BEFORE those freed while it held more: which of the two a
 * collection gives back first. */
long fx_count(int kind, int what);

/* A counted object holding an integer: made with one reference, freed when the last is given back. */
typedef struct fx_object fx_object;
fx_object *fx_object_new(int value);
fx_object *fx_object_ref(fx_object *o);
void fx_object_unref(fx_object *o);
int fx_object_value(const fx_object *o);
void fx_object_set(fx_object *o, int value);
int fx_object_refs(const fx_object *o);
/* Null, where an object might have been answered. */
fx_object *fx_object_none(int value);
/* A status: zero where the object holds `want`, and the difference otherwise. */
int fx_object_check(fx_object *o, int want);
/* The object, lent: no reference is taken for the caller. */
fx_object *fx_object_lend(fx_object *o);

/* A library owning the faces made in it, which frees every face still in it when its last reference
 * goes, as FreeType's `FT_Done_FreeType` does; a face given back after that is freed twice. */
typedef struct fx_library_rec *fx_library;
struct fx_face_rec {
    fx_library library;
    int id;
};
typedef struct fx_face_rec *fx_face;
fx_library fx_library_new(void);
int fx_library_reference(fx_library library);
int fx_library_done(fx_library library);
int fx_library_faces(fx_library library);
/* The library takes the reference it is passed, and gives it back when the library is freed. */
void fx_library_adopt(fx_library library, fx_object *o);
/* The object the library adopted last, lent; null where it has none. */
fx_object *fx_library_adopted(fx_library library);
/* A face made in `library`, which keeps a pointer to it and takes no reference. */
fx_face fx_face_new(fx_library library, int id);
/* A face made in `library` over `memory`, which it keeps a pointer to without a reference. */
fx_face fx_face_new_over(fx_library library, fx_object *memory, int id);
int fx_face_reference(fx_face face);
int fx_face_done(fx_face face);
int fx_face_id(fx_face face);
/* The library a face was made in, lent. */
fx_library fx_face_library(fx_face face);
/* The value of the memory a face was made over, read through the face. */
int fx_face_memory_value(fx_face face);

/* A variation descriptor of a face, freed through the face's library. */
typedef struct fx_mmvar fx_mmvar;
fx_mmvar *fx_mmvar_new(fx_face face);
void fx_mmvar_free(fx_library library, fx_mmvar *mm);
int fx_mmvar_axes(const fx_mmvar *mm);

/* An object with no count, freed once, and a shelf keeping a pointer to one, which it never reads
 * when it is freed itself. */
typedef struct fx_plain fx_plain;
fx_plain *fx_plain_new(int value);
void fx_plain_free(fx_plain *p);
int fx_plain_value(const fx_plain *p);
typedef struct fx_shelf fx_shelf;
fx_shelf *fx_shelf_new(fx_plain *p);
void fx_shelf_free(fx_shelf *s);
int fx_shelf_peek(const fx_shelf *s);

/* Statuses of each C integer shape. */
typedef int fx_status;
fx_status fx_status_of(int code);
enum fx_result { FX_RESULT_OK = 0, FX_RESULT_BAD = 3, FX_RESULT_WORSE = -2 };
enum fx_result fx_result_of(int which);
long long fx_long_status(long long code);
unsigned char fx_byte_status(unsigned char code);

#endif
