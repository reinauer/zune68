#ifndef ZUNE68_PLUGIN_RUNTIME_H
#define ZUNE68_PLUGIN_RUNTIME_H
#include <string.h>

/* Read libnix's declarations and inline definitions before redirecting
 * imported callers to our helpers. Some toolchain releases supply strlcpy. */
size_t zune68_strlcpy(char *dst, const char *src, size_t size);
size_t zune68_strlcat(char *dst, const char *src, size_t size);
#define strlcpy zune68_strlcpy
#define strlcat zune68_strlcat
#endif
