/* Integer/string snprintf for the bundled MCCs. No stdio, heap or startup.
 * Floating point and %n are deliberately unsupported (return -1).
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

struct output { char *data; size_t size, count; };

static void put(struct output *out, char c)
{
    if (out->size && out->count < out->size - 1)
        out->data[out->count] = c;
    out->count++;
}

int vsnprintf(char *buffer, size_t size, const char *format, va_list args)
{
    struct output out = {buffer, size, 0};
    int failed = 0;
    while (*format && !failed)
    {
        int left = 0, zero = 0, plus = 0, space = 0, alternate = 0;
        int width = 0, precision = -1, islong = 0, len, padding, zeros = 0;
        unsigned long value;
        unsigned base;
        char number[33], prefix[3], c;
        char *cursor;
        const char *text, *digits = "0123456789abcdef";
        int prefixlen = 0;
        if (*format != '%') { put(&out, *format++); continue; }
        format++;
        if (*format == '%') { put(&out, *format++); continue; }
        for (;; format++)
        {
            if (*format == '-') left = 1;
            else if (*format == '0') zero = 1;
            else if (*format == '+') plus = 1;
            else if (*format == ' ') space = 1;
            else if (*format == '#') alternate = 1;
            else break;
        }
        if (*format == '*')
        {
            width = va_arg(args, int); format++;
            if (width == INT_MIN) { failed = 1; break; }
            if (width < 0) { left = 1; width = -width; }
        }
        else while (*format >= '0' && *format <= '9')
        {
            if (width > INT_MAX / 10 ||
                (width == INT_MAX / 10 && *format - '0' > INT_MAX % 10))
            { failed = 1; break; }
            width = width * 10 + *format++ - '0';
        }
        if (failed) break;
        if (*format == '.')
        {
            format++; precision = 0;
            if (*format == '*') { precision = va_arg(args, int); format++; }
            else while (*format >= '0' && *format <= '9')
            {
                if (precision > INT_MAX / 10 ||
                    (precision == INT_MAX / 10 &&
                     *format - '0' > INT_MAX % 10))
                { failed = 1; break; }
                precision = precision * 10 + *format++ - '0';
            }
        }
        if (failed) break;
        if (*format == 'l') { islong = 1; format++; }
        c = *format;
        if (!c) { failed = 1; break; }
        format++;
        if (c == 's')
        {
            text = va_arg(args, const char *);
            if (!text) text = "(null)";
            len = 0;
            while ((precision < 0 || len < precision) && text[len]) len++;
            zero = 0;
        }
        else if (c == 'c')
        {
            number[0] = va_arg(args, int);
            text = number; len = 1; zero = 0;
        }
        else if (strchr("diuoxXp", c))
        {
            base = c == 'o' ? 8 : (c == 'x' || c == 'X' || c == 'p') ? 16 : 10;
            if (c == 'X') digits = "0123456789ABCDEF";
            if (c == 'p') value = (unsigned long)va_arg(args, void *);
            else if (c == 'd' || c == 'i')
            {
                long signed_value = islong ? va_arg(args, long) : va_arg(args, int);
                value = (unsigned long)signed_value;
                if (signed_value < 0) { prefix[prefixlen++] = '-'; value = 0UL - value; }
                else if (plus) prefix[prefixlen++] = '+';
                else if (space) prefix[prefixlen++] = ' ';
            }
            else value = islong ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
            if ((alternate && value && base == 16) || c == 'p')
            { prefix[prefixlen++] = '0'; prefix[prefixlen++] = c == 'X' ? 'X' : 'x'; }
            cursor = number + sizeof(number);
            if (value || precision != 0)
                do { *--cursor = digits[value % base]; value /= base; } while (value);
            text = cursor;
            len = number + sizeof(number) - text;
            if (alternate && base == 8 && (!len || *text != '0') && precision <= len)
                precision = len + 1;
            if (precision > len) zeros = precision - len;
            if (precision >= 0) zero = 0;
        }
        else { failed = 1; break; }
        padding = width - prefixlen - zeros - len;
        if (!left && !zero) while (padding-- > 0) put(&out, ' ');
        for (int i = 0; i < prefixlen; i++) put(&out, prefix[i]);
        if (!left && zero) while (padding-- > 0) put(&out, '0');
        while (zeros-- > 0) put(&out, '0');
        for (int i = 0; i < len; i++) put(&out, text[i]);
        if (left) while (padding-- > 0) put(&out, ' ');
    }
    if (size) buffer[out.count < size ? out.count : size - 1] = 0;
    return failed || out.count > INT_MAX ? -1 : (int)out.count;
}

int snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list args;
    int result;
    va_start(args, format);
    result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}
int sprintf(char *buffer, const char *format, ...)
{
    va_list args;
    int result;
    va_start(args, format);
    result = vsnprintf(buffer, (size_t)-1, format, args);
    va_end(args);
    return result;
}
