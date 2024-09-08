#include "./type.h"

struct type type_new(size_t size)
{
        struct type type;
        type.size = size;
        return type;
}