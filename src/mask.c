#include "mask.h"

/* FNV-1a 64-bit content digest used for --mask output. */
unsigned long long mask_fnv1a(const char *s) {
  unsigned long long h = 1469598103934665603ULL;
  while (*s) {
    h ^= (unsigned char)*s++;
    h *= 1099511628211ULL;
  }
  return h;
}
