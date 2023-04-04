#include "util.h"
#include "defs.h"

/* remove this */
#include "tmp.h"

/* determine what is really necessary for our use case. */
#include <sys/types.h>
#include <sys/uio.h>
#include <dlfcn.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void minidump_init(void);

void *main_frame_addr = 0;

FILE *
_dump_open(void)
{
  /* generate a path name with the following:
   current_vere_ver.unix_time_stamp.dmp
  */
  FILE *ret = fopen("mydump", "rb");
  return ret;
}

void
_dump_close(FILE *dump)
{
  assert(fclose(dump) == 0);
}

#define D10(x) ceil(log10(((x) == 0) ? 2 : ((x) + 1)))

inline static void *
realloc_safe(void *ptr, size_t size)
{
    void *nptr;

    nptr = realloc(ptr, size);
    if (nptr == NULL)
        free(ptr);
    return nptr;
}

int
_backtrace(void **buffer, int size)
{
  int i;
  for (i = 1
         ; getframeaddr(i) != main_frame_addr
         && i != size; i++) {
    buffer[i - 1] = getreturnaddr(i);
  }
  /* i -= 1; */
  /* buffer[i - 1] = getreturnaddr(i); */
  /* i -= 1; */
  
  return i - 1;
}

/* ;;: this ain't gonna cut it becasue dladdr relies on the dynamic symbol table
     being populated. It will not be in a statically linked executable. */

#if 1

char **
_backtrace_symbols(void *const *buffer, int size)
{
    int i, clen, alen, offset;
    char **rval;
    char *cp;
    Dl_info info;

    clen = size * sizeof(char *);
    rval = malloc(clen);
    if (rval == NULL)
        return NULL;
    cp = &(rval[size]);
    for (i = 0; i < size; i++) {
        if (dladdr(buffer[i], &info) != 0) {
            if (info.dli_sname == NULL)
                info.dli_sname = "???";
            if (info.dli_saddr == NULL)
                info.dli_saddr = buffer[i];
            offset = buffer[i] - info.dli_saddr;
            /* "0x01234567 <function+offset> at filename" */
            alen = 2 +                      /* "0x" */
                   (sizeof(void *) * 2) +   /* "01234567" */
                   2 +                      /* " <" */
                   strlen(info.dli_sname) + /* "function" */
                   1 +                      /* "+" */
                   D10(offset) +            /* "offset */
                   5 +                      /* "> at " */
                   strlen(info.dli_fname) + /* "filename" */
                   1;                       /* "\0" */
            rval = realloc_safe(rval, clen + alen);
            if (rval == NULL)
                return NULL;
            snprintf(cp, alen, "%p <%s+%d> at %s",
              buffer[i], info.dli_sname, offset, info.dli_fname);
        } else {
            alen = 2 +                      /* "0x" */
                   (sizeof(void *) * 2) +   /* "01234567" */
                   1;                       /* "\0" */
            rval = realloc_safe(rval, clen + alen);
            if (rval == NULL)
                return NULL;
            snprintf(cp, alen, "%p", buffer[i]);
        }
        rval[i] = cp;
        cp += alen;
    }
    return rval;
}

#endif

void
minidump(void)
{
  c3_dessert(0);

  void *array[10] = {0};
  char **strings = {0};
  int size;

  size = _backtrace (array, 10);
  strings = _backtrace_symbols (array, size);


  /*
     1) open dump file
     2) capture backtrace and possibly other information
     3) write to dumpfile and stderr
     4) close dump file.
   */


  
  
/* #if 0 */
/*   int size = backtrace(buffer, 10); */
/*   strings  = backtrace_symbols(buffer, size); */
/*   c3_dessert(strings != NULL); */

/*   printf ("Obtained %d stack frames.\n", size); */
/*   for (size_t i = 0; i < size; i++) */
/*     printf ("%s\n", strings[i]); */

/*   free(strings); */
/* #else */
/*   void * frame = __builtin_frame_address(0); */
/*   void ** fun_frm_addrs[12] = {0}; */
/*   void *  fun_ret_addrs[12] = {0}; */
/*   fun_frm_addrs[0] = __builtin_frame_address(0); */
/*   fun_ret_addrs[0] = __builtin_return_address(0); */
/*   fun_frm_addrs[1] = __builtin_frame_address(1); */
/*   fun_ret_addrs[1] = __builtin_return_address(1); */
/*   fun_frm_addrs[2] = __builtin_frame_address(2); */
/*   fun_ret_addrs[2] = __builtin_return_address(2); */
/*   fun_frm_addrs[3] = __builtin_frame_address(3); */
/*   fun_ret_addrs[3] = __builtin_return_address(3); */
/*   fun_frm_addrs[4] = __builtin_frame_address(4); */
/*   fun_ret_addrs[4] = __builtin_return_address(4); */
/*   fun_frm_addrs[5] = __builtin_frame_address(5); */
/*   fun_ret_addrs[5] = __builtin_return_address(5); */
/*   fun_frm_addrs[6] = __builtin_frame_address(6); */
/*   fun_ret_addrs[6] = __builtin_return_address(6); */
/*   fun_frm_addrs[7] = __builtin_frame_address(7); */
/*   c3_dessert(0); */
/*   /\* fun_ret_addrs[7] = __builtin_return_address(7); /\\* ;;: crash *\\/ *\/ */
/*   /\* fun_frm_addrs[8] = __builtin_frame_address(8); *\/ */
/*   /\* fun_ret_addrs[8] = __builtin_return_address(8); *\/ */
/*   /\* fun_frm_addrs[9] = __builtin_frame_address(9); *\/ */
/*   /\* fun_ret_addrs[9] = __builtin_return_address(9); *\/ */
/*   /\* fun_frm_addrs[10] = __builtin_frame_address(10); *\/ */
/*   /\* fun_ret_addrs[10] = __builtin_return_address(10); *\/ */
/*   /\* fun_frm_addrs[11] = __builtin_frame_address(11); *\/ */
/*   /\* fun_ret_addrs[11] = __builtin_return_address(11); *\/ */
  
/*   /\* fun_rets[0] = __builtin_return_address(0); *\/ */
/*   /\* fun_rets[1] = __builtin_return_address(1); *\/ */
/*   /\* fun_rets[2] = __builtin_return_address(2); *\/ */
/*   /\* fun_rets[3] = __builtin_return_address(3); *\/ */
/*   /\* fun_rets[4] = __builtin_return_address(11); *\/ */
/* #endif */

  
}
