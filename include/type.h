#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * Contains compile-time accessible type information. Construct with `TYPEINFO`
 * macro e.g.
 * 
 * ```c
 * TYPEINFO(int)
 * ```
 */
struct type {
        size_t size;
};

/**
 * Contruct a new `struct type` with a given `size`.
 */
struct type type_new(size_t size);

/**
 * This is equivalent to using `type_new()` with valid arguments. The idea is
 * to use this macro only with an actual type, so that calls are 
 * self-documenting. It is strongly recommended that you use this macro instead
 * of contructing a `struct type` directly.
 */
#define TYPEINFO(T) type_new(sizeof(T))