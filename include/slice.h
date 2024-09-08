#pragma once

#include <string.h>
#include <stdint.h>
#include <stddef.h>

/**
 * A more sane generic slice type. For example, we can use this type to 
 * represent string slices or a view into a `dynarray`.
 * 
 * Slices do not need to be freed. They are non-owning.
 */
typedef struct {
        /**
         * Inclusive begin of the slice.
         */
        void *begin;
        /**
	 * Exclusive end of the slice.
	 */
        void *end;
} slice;

/**
 * Return an iterator to the beginning of this slice. The returned pointer is
 * valid for the inner type.
 */
void *slice_begin(slice self);

/**
 * A pointer one past the end of this iterator.
 */
void *slice_end(slice self);

/**
 * Bounds-checked slice element access of the n-th element. The returned pointer
 * is valid for reads and writes of the inner type.
 */
void *slice_get(slice self, struct type val_type, size_t index);

/**
 * The length of this slice, in multiples of the inner type.
 */
static inline size_t slice_length(slice self, struct type val_type)
{
        return ((size_t)self.end - (size_t)self.begin) / val_type.size;
}

/**
 * Construct a slice with a beggining and an end. 
 * 
 * # Safety 
 * - `begin` and `end` must be pointers aligned to at least the alignment of the
 *   inner type.
 * - All offsets from `begin` in this range must be valid for reads and writes 
 *   of the inner type.
 */
slice slice_new(void *begin, void *end);

/**
 * Do a `memcmp()` over the contents of this buffer. This will work as you 
 * expect for all primitive types e.g. `int`, `long` etc. However, for types 
 * that have padding, e.g. 
 * ```c
 * struct Foo { int a; char b; };
 * ``` 
 * you will need to make sure that the padding is zeroed (or set to some common
 * value). Otherwise two arrays with the same initialization pattern might 
 * not be considered equal.
 * 
 * For reference, the value returned has the same 
 */
int slice_memcmp(slice lhs, slice rhs);

/**
 * Shorthand for doing a `memcmp` on these slices and testing the result against
 * `0`. See `slice_memcmp` for more information.
 */
bool slice_memeq(slice lhs, slice rhs);