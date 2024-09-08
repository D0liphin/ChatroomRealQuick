#include "./fmt.h"

/** Overestimate the buffer size we need */
#define BUF_SIZE_FOR(INT) (3 * sizeof(INT) + 1)

#define DEFINE_FMT_INTEGRAL(FN_NAME, INT, SPEC)                                 \
        void FN_NAME(struct cstring *restrict f, INT const *value)              \
        {                                                                       \
                char buf[BUF_SIZE_FOR(INT)];                                    \
                int buflen;                                                     \
                snprintf(buf, BUF_SIZE_FOR(INT), SPEC "%n", *value, &buflen);   \
                cstring_extend(f, (uint8_t *)&buf[0], (uint8_t *)&buf[buflen]); \
        }

DEFINE_FMT_INTEGRAL(fmt_char, char, "%d")
DEFINE_FMT_INTEGRAL(fmt_uchar, unsigned char, "%u")
DEFINE_FMT_INTEGRAL(fmt_uchar_hex, unsigned char, "%x")

DEFINE_FMT_INTEGRAL(fmt_int, int, "%d")

DEFINE_FMT_INTEGRAL(fmt_uint, unsigned int, "%u")
DEFINE_FMT_INTEGRAL(fmt_uint_oct, unsigned int, "%o")
DEFINE_FMT_INTEGRAL(fmt_uint_hex, unsigned int, "%x")
DEFINE_FMT_INTEGRAL(fmt_uint_uhex, unsigned int, "%X")

DEFINE_FMT_INTEGRAL(fmt_lint, long int, "%ld")

DEFINE_FMT_INTEGRAL(fmt_ulint, unsigned long int, "%lu")
DEFINE_FMT_INTEGRAL(fmt_ulint_oct, unsigned long int, "%lo")
DEFINE_FMT_INTEGRAL(fmt_ulint_hex, unsigned long int, "%lx")
DEFINE_FMT_INTEGRAL(fmt_ulint_uhex, unsigned long int, "%lX")

DEFINE_FMT_INTEGRAL(fmt_llint, long long int, "%lld")

DEFINE_FMT_INTEGRAL(fmt_ullint, unsigned long long int, "%llu")
DEFINE_FMT_INTEGRAL(fmt_ullint_oct, unsigned long long int, "%llo")
DEFINE_FMT_INTEGRAL(fmt_ullint_hex, unsigned long long int, "%llx")
DEFINE_FMT_INTEGRAL(fmt_ullint_uhex, unsigned long long int, "%llX")

DEFINE_FMT_INTEGRAL(fmt_size, size_t, "%zu")
DEFINE_FMT_INTEGRAL(fmt_size_hex, size_t, "%zx")
DEFINE_FMT_INTEGRAL(fmt_size_uhex, size_t, "%zX")

DEFINE_FMT_INTEGRAL(fmt_ptrdiff, ptrdiff_t, "%td")
DEFINE_FMT_INTEGRAL(fmt_ptrdiff_hex, ptrdiff_t, "%tx")
DEFINE_FMT_INTEGRAL(fmt_ptrdiff_uhex, ptrdiff_t, "%tX")

DEFINE_FMT_INTEGRAL(fmt_intmax, intmax_t, "%jd")
DEFINE_FMT_INTEGRAL(fmt_intmax_hex, intmax_t, "%jx")
DEFINE_FMT_INTEGRAL(fmt_intmax_uhex, intmax_t, "%jX")

DEFINE_FMT_INTEGRAL(fmt_uintmax, uintmax_t, "%ju")
DEFINE_FMT_INTEGRAL(fmt_uintmax_oct, uintmax_t, "%jo")
DEFINE_FMT_INTEGRAL(fmt_uintmax_hex, uintmax_t, "%jx")
DEFINE_FMT_INTEGRAL(fmt_uintmax_uhex, uintmax_t, "%jX")

void fmt_dynarray(struct cstring *restrict f, struct dynarray const *arr, struct type ty,
                  formatter cb)
{
        char *endch = " }";
        cstring_extend_cstr(f, "{ ");

        if (dynarray_length(arr, ty) == 0) {
                cstring_extend_cstr(f, endch);
        }

        char const *it = dynarray_begin(arr);
        char const *last = dynarray_end(arr) - ty.size;
        for (; it != last; it += ty.size) {
                cb(f, (void const *)it);
                cstring_extend_cstr(f, ", ");
        }
        cb(f, (void const *)last);

        cstring_extend_cstr(f, endch);
}