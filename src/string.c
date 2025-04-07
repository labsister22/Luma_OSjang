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
