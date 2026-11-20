/// @file
///
/// Portable scratch-directory helpers.
///
/// Tests that need real files on disk use these instead of hardcoding
/// /tmp and shelling out to `rm -rf`, neither of which exists on windows.
/// mkdtemp() itself is posix; the windows build gets it from compat.h.

#ifndef C3_TMPDIR_H
#define C3_TMPDIR_H

#include "defs.h"
//  NB: load-bearing despite naming no symbol here -- portable.h is what
//  supplies mkdtemp, from <unistd.h> on posix and from compat.h on windows.
//
#include "portable.h"
#include "types.h"

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

  /** Functions.
  **/
    /* c3_tmp_root(): system scratch directory, with no trailing slash.
    */
      static inline const c3_c*
      c3_tmp_root(void)
      {
        const c3_c* dir_c;

#       ifdef U3_OS_windows
          //  windows has no fixed scratch path; the environment names it.
          //
          if ( (dir_c = getenv("TEMP")) && *dir_c ) return dir_c;
          if ( (dir_c = getenv("TMP"))  && *dir_c ) return dir_c;
          return ".";
#       else
          if ( (dir_c = getenv("TMPDIR")) && *dir_c ) return dir_c;
          return "/tmp";
#       endif
      }

    /* c3_tmp_make(): create a unique scratch directory named for [pre_c].
    **
    **   writes the path into [buf_c] and returns it, or 0 on failure
    **   with errno set.  the caller owns the directory and should hand
    **   it to c3_tmp_kill() when done.
    */
      static inline c3_c*
      c3_tmp_make(c3_c* buf_c, size_t len_i, const c3_c* pre_c)
      {
        const c3_c* rot_c = c3_tmp_root();
        size_t      rot_i = strlen(rot_c);

        //  trim trailing separators: $TMPDIR carries one on macos, and
        //  %TEMP% may as well.
        //
        while (  (1 < rot_i)
              && (  ('/'  == rot_c[rot_i - 1])
                 || ('\\' == rot_c[rot_i - 1]) ) )
        {
          rot_i--;
        }

        if ( (c3_i)len_i <= snprintf(buf_c, len_i, "%.*s/%s-XXXXXX",
                                     (c3_i)rot_i, rot_c, pre_c) )
        {
          errno = ENAMETOOLONG;
          return 0;
        }

        return mkdtemp(buf_c);
      }

    /* c3_tmp_kill(): delete [pax_c] and everything beneath it.
    **
    **   best-effort teardown: a path that is already gone is not an error.
    */
      static inline void
      c3_tmp_kill(const c3_c* pax_c)
      {
        DIR* dir_u = c3_opendir(pax_c);

        if ( dir_u ) {
          struct dirent* ent_u;

          while ( (ent_u = readdir(dir_u)) ) {
            struct stat sat_u;
            c3_c        kid_c[2048];

            if (  (0 == strcmp(ent_u->d_name, "."))
               || (0 == strcmp(ent_u->d_name, "..")) )
            {
              continue;
            }

            if ( (c3_i)sizeof(kid_c) <= snprintf(kid_c, sizeof(kid_c), "%s/%s",
                                                 pax_c, ent_u->d_name) )
            {
              continue;
            }

            //  NB: stat(), not d_type: mingw leaves dirent fields unset.
            //
            if ( (0 == stat(kid_c, &sat_u)) && S_ISDIR(sat_u.st_mode) ) {
              c3_tmp_kill(kid_c);
            }
            else {
              c3_unlink(kid_c);
            }
          }

          closedir(dir_u);
        }

        c3_rmdir(pax_c);
      }

#endif /* ifndef C3_TMPDIR_H */
