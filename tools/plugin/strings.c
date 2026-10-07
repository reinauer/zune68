/* Private BSD string helpers independent of the installed libnix version. */
#include "runtime.h"

size_t strlcpy(char *dst, const char *src, size_t size)
{
    size_t length = strlen(src);
    if (size)
    {
        size_t copied = length < size - 1 ? length : size - 1;
        memcpy(dst, src, copied);
        dst[copied] = 0;
    }
    return length;
}

size_t strlcat(char *dst, const char *src, size_t size)
{
    size_t length = 0;
    while (length < size && dst[length])
        length++;
    if (length == size) return length + strlen(src);
    return length + strlcpy(dst + length, src, size - length);
}
