#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
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

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    // Find the end of dest string
    while (*d != '\0') d++;
    // Copy src to the end of dest
    while ((*d++ = *src++));
    return dest;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        ++s1; ++s2; --n;
    }
    return n ? *(unsigned char *)s1 - *(unsigned char *)s2 : 0;
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
    char *s = str ? str : *saveptr;
    if (!s) {
        return NULL;
    }

    const char *d = delim;
    char *token_start = s;

    // Lewati delimiter di awal string
    while (*s) {
        const char *current_delim = d;
        bool is_delim = false;
        while (*current_delim) {
            if (*s == *current_delim) {
                is_delim = true;
                break;
            }
            current_delim++;
        }
        if (!is_delim) {
            token_start = s;
            break;
        }
        s++;
    }

    if (*token_start == '\0') {
        *saveptr = s;
        return NULL;
    }

    // Cari akhir token
    while (*s) {
        const char *current_delim = d;
        bool is_delim = false;
        while (*current_delim) {
            if (*s == *current_delim) {
                is_delim = true;
                break;
            }
            current_delim++;
        }
        if (is_delim) {
            *s++ = '\0'; // Ganti delimiter dengan null terminator
            break;
        }
        s++;
    }

    *saveptr = s;
    return token_start;
}