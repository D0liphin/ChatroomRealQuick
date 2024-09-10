#pragma once

#include "./include/dynarray.h"
#include "./include/slice.h"
#include "./include/type.h"
#include "./include/cstring.h"

/**
 * A formatter is any function that can convert a value into a string without 
 * mutating it. The resulting value is pushed to the end of the first parameter,
 * which is the output `cstring`. This API prioritises ease-of-use over speed.
 * 
 * A formatter for most commonly used format specifiers exists. They are named
 * using the complete type, e.g. `fmt_uint` or `fmt_int` etc. 
 * 
 * Since they are generated with macros, we can't really make docstrings on 
 * them (not that that does anything in C). They should all be self explanatory
 * but to find the function you are looking for:
 * 
 * 
 * ```fmt_int```: `%d` (decimal format for `int`)
 * ```fmt_uint```: `%u` (decimal format for `unsigned int`)
 * ```fmt_uint_oct```: `%o` (octal format for `unsigned int`)
 * ```fmt_uint_hex```: `%x` (hexadecimal format for `unsigned int`, lowercase)
 * ```fmt_uint_uhex```: `%X` (hexadecimal format for `unsigned int`, uppercase)
 * ```fmt_lint```: `%ld` (decimal format for `long int`)
 * ```fmt_ulint```: `%lu` (decimal format for `unsigned long int`)
 * ```fmt_ulint_oct```: `%lo` (octal format for `unsigned long int`)
 * ```fmt_ulint_hex```: `%lx` (hexadecimal format for `unsigned long int`, lowercase)
 * ```fmt_ulint_uhex```: `%lX` (hexadecimal format for `unsigned long int`, uppercase)
 * ```fmt_llint```: `%lld` (decimal format for `long long int`)
 * ```fmt_ullint```: `%llu` (decimal format for `unsigned long long int`)
 * ```fmt_ullint_oct```: `%llo` (octal format for `unsigned long long int`)
 * ```fmt_ullint_hex```: `%llx` (hexadecimal format for `unsigned long long int`, lowercase)
 * ```fmt_ullint_uhex```: `%llX` (hexadecimal format for `unsigned long long int`, uppercase)
 * ```fmt_size```: `%zu` (decimal format for `size_t`)
 * ```fmt_size_hex```: `%zx` (hexadecimal format for `size_t`, lowercase)
 * ```fmt_size_uhex```: `%zX` (hexadecimal format for `size_t`, uppercase)
 * ```fmt_ptrdiff```: `%td` (decimal format for `ptrdiff_t`)
 * ```fmt_ptrdiff_hex```: `%tx` (hexadecimal format for `ptrdiff_t`, lowercase)
 * ```fmt_ptrdiff_uhex```: `%tX` (hexadecimal format for `ptrdiff_t`, uppercase)
 * ```fmt_intmax```: `%jd` (decimal format for `intmax_t`)
 * ```fmt_intmax_hex```: `%jx` (hexadecimal format for `intmax_t`, lowercase)
 * ```fmt_intmax_uhex```: `%jX` (hexadecimal format for `intmax_t`, uppercase)
 * ```fmt_uintmax```: `%ju` (decimal format for `uintmax_t`)
 * ```fmt_uintmax_oct```: `%jo` (octal format for `uintmax_t`)
 * ```fmt_uintmax_hex```: `%jx` (hexadecimal format for `uintmax_t`, lowercase)
 * ```fmt_uintmax_uhex```: `%jX` (hexadecimal format for `uintmax_t`, uppercase)
 * ```
 */
typedef void (*formatter)(struct cstring *restrict, void const *);

#define DECLARE_FMT_INTEGRAL(FN_NAME, INT, SPEC) \
        void FN_NAME(struct cstring *restrict f, INT const *value);
DECLARE_FMT_INTEGRAL(fmt_int, int, "%d")

DECLARE_FMT_INTEGRAL(fmt_char, char, "%d")
DECLARE_FMT_INTEGRAL(fmt_uchar, unsigned char, "%u")
DECLARE_FMT_INTEGRAL(fmt_uchar_hex, unsigned char, "%x")

DECLARE_FMT_INTEGRAL(fmt_uint, unsigned int, "%u")
DECLARE_FMT_INTEGRAL(fmt_uint_oct, unsigned int, "%o")
DECLARE_FMT_INTEGRAL(fmt_uint_hex, unsigned int, "%x")
DECLARE_FMT_INTEGRAL(fmt_uint_uhex, unsigned int, "%X")

DECLARE_FMT_INTEGRAL(fmt_lint, long int, "%ld")

DECLARE_FMT_INTEGRAL(fmt_ulint, unsigned long int, "%lu")
DECLARE_FMT_INTEGRAL(fmt_ulint_oct, unsigned long int, "%lo")
DECLARE_FMT_INTEGRAL(fmt_ulint_hex, unsigned long int, "%lx")
DECLARE_FMT_INTEGRAL(fmt_ulint_uhex, unsigned long int, "%lX")

DECLARE_FMT_INTEGRAL(fmt_llint, long long int, "%lld")

DECLARE_FMT_INTEGRAL(fmt_ullint, unsigned long long int, "%llu")
DECLARE_FMT_INTEGRAL(fmt_ullint_oct, unsigned long long int, "%llo")
DECLARE_FMT_INTEGRAL(fmt_ullint_hex, unsigned long long int, "%llx")
DECLARE_FMT_INTEGRAL(fmt_ullint_uhex, unsigned long long int, "%llX")

DECLARE_FMT_INTEGRAL(fmt_size, size_t, "%zu")
DECLARE_FMT_INTEGRAL(fmt_size_hex, size_t, "%zx")
DECLARE_FMT_INTEGRAL(fmt_size_uhex, size_t, "%zX")

DECLARE_FMT_INTEGRAL(fmt_ptrdiff, ptrdiff_t, "%td")
DECLARE_FMT_INTEGRAL(fmt_ptrdiff_hex, ptrdiff_t, "%tx")
DECLARE_FMT_INTEGRAL(fmt_ptrdiff_uhex, ptrdiff_t, "%tX")

DECLARE_FMT_INTEGRAL(fmt_intmax, intmax_t, "%jd")
DECLARE_FMT_INTEGRAL(fmt_intmax_hex, intmax_t, "%jx")
DECLARE_FMT_INTEGRAL(fmt_intmax_uhex, intmax_t, "%jX")

DECLARE_FMT_INTEGRAL(fmt_uintmax, uintmax_t, "%ju")
DECLARE_FMT_INTEGRAL(fmt_uintmax_oct, uintmax_t, "%jo")
DECLARE_FMT_INTEGRAL(fmt_uintmax_hex, uintmax_t, "%jx")
DECLARE_FMT_INTEGRAL(fmt_uintmax_uhex, uintmax_t, "%jX")

/**
 * Format a dynammic array with a callback. e.g.
 ```c
 fmt_dynarray(&my_string, &arr, TYPEINFO(int), (formatter)fmt_int);
 ``` 
 * Would produce something like 
 ```plaintext
 { 1, 2, 3, 4 }
 ```
 */
void fmt_dynarray(struct cstring *f, struct dynarray *arr, struct type ty, formatter cb);