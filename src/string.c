#include <stddef.h>
#include <stdint.h>

void *memset(void *s, int c, size_t n)
{
  uint8_t *p = (uint8_t *)s;
  while (n--)
  {
    *p++ = (uint8_t)c;
  }
  return s;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;
  while (n--)
  {
    *d++ = *s++;
  }
  return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
  const uint8_t *p1 = s1, *p2 = s2;
  while (n--)
  {
    if (*p1 != *p2)
      return *p1 - *p2;
    p1++, p2++;
  }
  return 0;
}

void *memmove(void *dest, const void *src, size_t n)
{
  uint8_t *d = dest;
  const uint8_t *s = src;
  if (d < s)
  {
    while (n--)
      *d++ = *s++;
  }
  else
  {
    d += n;
    s += n;
    while (n--)
      *(--d) = *(--s);
  }
  return dest;
}

size_t strlen(const char *str) {
  size_t len = 0;
  while (str[len]) {
      len++;
  }
  return len;
}

char *strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++));
  return dest;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
      s1++;
      s2++;
  }
  return *(const unsigned char*)s1 - *(const unsigned char*)s2;
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