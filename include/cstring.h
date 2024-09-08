#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "./dynarray.h"
#include "./slice.h"

/**
 * Represents a unicode codepoint. This is a typedef for a valid reason -- we 
 * want lints when you try and use a `uint32_t` as this value as it is probably
 * an accident.
 */
typedef struct {
        /**
         * You shouldn't be relying on this field existing. This type is 
         * supposed to be opaque. Use `codepoint_as_uint32()`.
         */
        uint32_t value;
} codepoint;

/**
 * Intended for use as a character literal specifier. E.g. 
 * 
 * ```c
 * CODEPOINT(L'A')
 * CODEPOINT(L'あ')
 * ```
 */
#define CODEPOINT(value) codepoint_new((uint32_t)value)

/**
 * An allocated utf-8 string. This string also has the 'benefit' of always 
 * ending in a null-byte. If either of these properties are undesirable, you
 * should just use a `struct dynarray`.
 * 
 * Common convenience functions are ommitted on purpose, to indicate their 
 * performance cost. For example `.count()` is not present. This should be done
 * by creating a `codepoints` iterator and incrementing a counter yourself. 
 * If you don't want to do all that, you should probably just use a `struct
 * dynarray`.
 * 
 * Some terminology that is used throughout:
 * 
 * - `cstring`: An owned (i.e. heap-allocated) `cstr`.
 * - `cstr`: A null-terminated `char *`. A C string literal is of this type.
 * - `str`: A slice of bytes. This is valid utf8.
 * 
 * # Example
 * Print out a greeting from the user.
 * 
 * ```c
 * struct cstring greeting = cstring_is("user says: ");
 * stdin_readline(&greeting);
 * printf("%s", cstring_as_cstr(greeting));
 * cstring_free(&greeting);
 * ```
 *
 * # Why?
 * Lots of libraries expect ascii, null-terminated strings. This satisfies that 
 * requirement, by allowing you to interpret this as such. Furthermore, it is
 * quite helpful to indicate that something is (or, well, we're writing C, so
 * 'should be') utf8 i.e. "printable".
 */
struct cstring {
        /** 
         * Internal type `char` (or `unsigned char` etc.) This is not to be 
         * confused with `character`.
         */
        struct dynarray buf;
};

/**
 * A valid utf8 slice. Roughly the same as `*mut str`. See the rust docs for
 * more information.
 */
typedef struct {
        /** 
         * Internal type `char` (or `unsigned char` etc.) This is not to be 
         * confused with `character`.
         */
        slice sl;
} str;

/**
 * An iterator over the codepoints in a `struct cstring`. The full iterator 
 * suite is comprised of `*_codepoints()` `codepoints_has_next()` and 
 * `codepoints_next()`. 
 * 
 * This iterator is O(n) over the bytes. 
 */
struct codepoints {
        /**
         * The slice we are iterating over. This slice should update the 
         * `begin` field as the iteration progresses. Slice has internal type
         * `codepoint`.
         */
        slice iter;
};

/**
 * Create a new codepoint from a character. This is intentended to be used only 
 * for character literals that you know to be unicode beforehand.
 */
static inline codepoint codepoint_new(uint32_t value)
{
        codepoint ch;
        ch.value = value;
        return ch;
}

/**
 * Convert this codepoint int a `uint32_t` codepoint.
 */
static inline uint32_t codepoint_as_uint32(codepoint ch)
{
        return ch.value;
}

/**
 * Construct a new, empty `cstring`. This is not zero-allocating. An empty 
 * string contains a single null-byte.
 */
struct cstring cstring_new();

/**
 * Free this `cstring`, invalidating it for any future use.
 */
void cstring_free(struct cstring *self);

/**
 * Equivalent to creating an empty `cstring` and extending it with the contents
 * of the supplied cstring. This is very useful for creating owned strings in 
 * a clean way.
 * 
 * ```c
 * struct cstring greeting = cstring_is("user says: ");
 * stdin_readline(&greeting);
 * printf("%s", cstring_as_cstr(greeting));
 * cstring_free(&greeting);
 * ```
 */
struct cstring cstring_is(char const *cstr);

/** 
 * Encode a `codepoint` as utf8 and append it.
 */
void cstring_push(struct cstring *self, codepoint ch);

/**
 * Extend by the bytes in a cstr. 
 * 
 * # Safety
 * - Must be valid utf8. (deferred UB)
 * - Must be null-terminated
 */
void cstring_extend_cstr(struct cstring *self, char const *cstr);

/**
 * Extend by the bytes in a pointer iterator.
 * 
 * # Safety
 * - Must be valid utf8.
 * - Must be a valid pointer iterator.
 */
void cstring_extend(struct cstring *self, uint8_t const *begin, uint8_t const *end);

/**
 * Get the byte at the specified index. This returns an actual byte because 
 * there's really no reason to be modifying this byte directly in most cases.
 * If you want an ascii string, you should use `struct dynarray`.
 */
uint8_t cstring_get(struct cstring const *self, size_t index);

/**
 * Just returns a pointer to the internal buffer, so this is really quick. It's
 * guaranteed to end in a null-byte, so this can be used to interface with 
 * functions that expect a cstr.
 */
char const *cstring_as_cstr(struct cstring const *self);

/**
 * Compare two strings for value equality.
 */
bool cstring_eq(struct cstring const *lhs, struct cstring const *rhs);

/**
 * Compare to cstrs for value equality.
 */
bool cstr_eq(char const *lhs, char const *rhs);

/**
 * Get a `str` representing this cstring. The returned `str` omits the 
 * final null-byte.
 */
str cstring_as_str(struct cstring const *self);

/**
 * Get an iterator over the codepoints in this cstring.
 */
struct codepoints cstring_codepoints(struct cstring const *self);

/**
 * Return an iterator to the beginning of this str.
 */
uint8_t *str_begin(str self);

/**
 * A pointer one past the end of this iterator.
 */
uint8_t *str_end(str self);

/**
 * Bounds-checked str element access of the n-th byte. 
 */
uint8_t str_get(str self, size_t index);

/**
 * The length of this str in bytes. If you want the count, see 
 * `struct codepoints`.
 */
static inline size_t str_length(str const self)
{
        return (size_t)self.sl.end - (size_t)self.sl.begin;
}

/**
 * Construct a str with a beginning and an end. Returns an empty slice if the
 * provided buffer is not utf8.
 */
str str_new(uint8_t *begin, uint8_t *end);

/**
 * Get an iterator over the codepoints in a str.
 */
struct codepoints str_codepoints(str const self);

/**
 * Advance this iterator. Note that calls to this are undefined if 
 * `codepoints_has_next(self)` returns `false`.
 */
codepoint codepoints_next(struct codepoints *self);

/**
 * Check if a call to `codepoints_next` is valid.
 */
bool codepoints_has_next(struct codepoints *self);