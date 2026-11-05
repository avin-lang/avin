/* The shapes' functions, named by `AVIN_SHAPES_DECLARED`. `fx_object` is the typedef of
 * avin_handles.h, which this header does not include. */
#ifndef AVIN_SHAPES_DECLARED_H
#define AVIN_SHAPES_DECLARED_H

/* A shape keeps a pointer to the object it is made over and takes no reference; freeing the shape
 * reads the object. */
typedef struct fx_shape fx_shape;
fx_shape *fx_shape_new(fx_object *o);
void fx_shape_free(fx_shape *s);
int fx_shape_value(const fx_shape *s);
/* The object a shape is over, lent. */
fx_object *fx_shape_object(fx_shape *s);
/* A new object holding the shape's value, the caller's. */
fx_object *fx_shape_copy(const fx_shape *s);
/* The shape takes the reference to `o` it is passed, and gives it back when the shape is freed. */
void fx_shape_absorb(fx_shape *s, fx_object *o);
/* A status over another package's object. */
int fx_shape_matches(const fx_shape *s, const fx_object *o);

#endif
