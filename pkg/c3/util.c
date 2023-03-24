#include "util.h"

c3_w
_c3_printcap_mem_w(FILE *fil_u, c3_w wor_w, const c3_c *cap_c)
{
  return _c3_printcap_mem_z(fil_u, (c3_z)wor_w << 2, cap_c) >> 2;
}

c3_z
_c3_printcap_mem_z(FILE *fil_u, c3_z byt_z, const c3_c *cap_c)
{
  c3_assert( 0 != fil_u );      /* ;;: I assume this is important from commit
                                     f975ca908b143fb76c104ecc32cb59317ea5b198:
                                     threads output file pointer through memory
                                     marking and printing.

                                    If not necessary, we can get rid of
                                    c3_maid_w */

  c3_z gib_z = (byt_z / (1 << 30));
  c3_z mib_z = (byt_z % (1 << 30)) / (1 << 20);
  c3_z kib_z = (byt_z % (1 << 20)) / (1 << 10);
  c3_z bib_z = (byt_z % (1 << 10));

  if ( gib_z ) {
    fprintf(fil_u, "%s" "GiB/%" PRIc3_z ".%03" PRIc3_z ".%03" PRIc3_z ".%03" PRIc3_z "\r\n",
            cap_c, gib_z, mib_z, kib_z, bib_z);
  }
  else if ( mib_z ) {
    fprintf(fil_u, "%s" "MiB/%" PRIc3_z ".%03" PRIc3_z ".%03" PRIc3_z "\r\n",
            cap_c, mib_z, kib_z, bib_z);
  }
  else if ( kib_z ) {
    fprintf(fil_u, "%s" "KiB/%" PRIc3_z ".%03" PRIc3_z "\r\n",
            cap_c, kib_z, bib_z);
  }
  else {
    fprintf(fil_u, "%s" "B/%" PRIc3_z "\r\n",
            cap_c, bib_z);
  }
  return byt_z;
}
