#include <stdint.h>
#include <stddef.h>
#include "header/stdlib/string.h"

void *memset(void *s, int c, size_t n)
{
    uint8_t *buf = (uint8_t *)s;
    for (size_t i = 0; i < n; i++)
        buf[i] = (uint8_t)c;
    return s;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
    uint8_t *dstbuf = (uint8_t *)dest;
    const uint8_t *srcbuf = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++)
        dstbuf[i] = srcbuf[i];
    return dstbuf;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *buf1 = (const uint8_t *)s1;
    const uint8_t *buf2 = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++)
    {
        if (buf1[i] < buf2[i])
            return -1;
        else if (buf1[i] > buf2[i])
            return 1;
    }

    return 0;
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *dstbuf = (uint8_t *)dest;
    const uint8_t *srcbuf = (const uint8_t *)src;
    if (dstbuf < srcbuf)
    {
        for (size_t i = 0; i < n; i++)
            dstbuf[i] = srcbuf[i];
    }
    else
    {
        for (size_t i = n; i != 0; i--)
            dstbuf[i - 1] = srcbuf[i - 1];
    }

    return dest;
}     
size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len] != '\0') len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}
int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        ++s1; ++s2; --n;
    }
    return n ? *(unsigned char *)s1 - *(unsigned char *)s2 : 0;
}
char *strcat(char *dest, const char *src) {
    char *original_dest = dest;
    while (*dest != '\0') {
        dest++;
    }
    while ((*dest++ = *src++) != '\0');
    return original_dest;
}
char *strcpy(char *dest, const char *src) {

    char *original_dest = dest;

    while ((*dest++ = *src++) != '\0');

    return original_dest;

}
int sprintf(char *str, const char *format, ...) {
    // Implementasi sederhana untuk format string
    // Hanya support %s, %d, %c untuk sekarang
    
    const char *src = format;
    char *dst = str;
    int written = 0;
    
    // Untuk demo, implementasi sangat sederhana
    // Copy format string langsung (tanpa formatting)
    while (*src) {
        *dst++ = *src++;
        written++;
    }
    *dst = '\0';
    
    return written;
}

// Implementasi strncpy
char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    
    // Pad dengan null bytes jika src lebih pendek dari n
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    
    return dest;
}