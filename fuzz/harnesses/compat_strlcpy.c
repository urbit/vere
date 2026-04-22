/* strlcpy / strlcat stubs for vere-flavor fuzz harnesses.
 *
 * avahi-common (pulled in transitively by libvere's mdns support)
 * calls strlcpy/strlcat, which only appeared in glibc 2.38. On older
 * glibc (Ubuntu 22.04 ships 2.35) the linker fails with
 * "undefined reference to strlcpy". These harnesses never exercise
 * the mdns path, so a faithful-enough stub is sufficient to satisfy
 * the linker.
 */

#include <stddef.h>
#include <string.h>

size_t
strlcpy(char* dst, const char* src, size_t siz)
{
  size_t len = strlen(src);
  if (siz) {
    size_t cpy = (len >= siz) ? siz - 1 : len;
    memcpy(dst, src, cpy);
    dst[cpy] = '\0';
  }
  return len;
}

size_t
strlcat(char* dst, const char* src, size_t siz)
{
  size_t dl = strnlen(dst, siz);
  size_t sl = strlen(src);
  if (dl == siz) return siz + sl;
  size_t avail = siz - dl - 1;
  size_t cpy = (sl > avail) ? avail : sl;
  memcpy(dst + dl, src, cpy);
  dst[dl + cpy] = '\0';
  return dl + sl;
}
