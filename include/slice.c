#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "./type.h"
#include "./slice.h"
#include "./panic.h"

void *slice_begin(slice self)
{
        return self.begin;
}

void *slice_end(slice self)
{
        return self.end;
}

void *slice_get(slice self, struct type val_type, size_t index)
{
        if (index < slice_length(self, val_type)) {
                return &((uint8_t *)self.begin)[index * val_type.size];
        }
        PANIC("index out of bounds: the length is %zu but the index is %zu",
              slice_length(self, val_type), index);
        return NULL; // unreachable
}

slice slice_new(void *begin, void *end)
{
        slice sl;
        sl.begin = begin;
        sl.end = end;
        return sl;
}

int slice_memcmp(slice lhs, slice rhs)
{
        size_t lhs_len = (size_t)(lhs.end - lhs.begin);
        size_t rhs_len = (size_t)(rhs.end - rhs.begin);
        size_t len_ord = rhs_len - lhs_len;
        if (len_ord != 0) {
                return len_ord;
        }
        return memcmp(lhs.begin, rhs.begin, lhs_len);
}

bool slice_memeq(slice lhs, slice rhs)
{
        return (bool)(slice_memcmp(lhs, rhs) == 0);
}