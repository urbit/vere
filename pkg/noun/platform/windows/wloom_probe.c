/// @file
///
/// standalone probe for the windows loom mapping assumptions.
///
/// this is not part of the build. it exists to answer, on real hardware,
/// the questions that the sparse loom design rests on and that cannot be
/// settled from the documentation:
///
///   1. does a SEC_RESERVE section actually avoid commit charge, so that a
///      loom larger than RAM plus the paging file can be reserved?
///   2. do a section's *contents* survive unmapping and remapping its view?
///      (the design already depends on this, for boundary moves.)
///   3. does a section's *commitment* survive the same? if not, the sparse
///      loom would silently lose the heap on the first save that moves the
///      image boundary.
///   4. does VirtualProtect fail on uncommitted pages, so that u3e_yolo()
///      must skip them?
///   5. what does an access violation on an uncommitted page report, so
///      that the fault handler can recognize it?
///   6. which mechanism can implement _ce_toss_pages() on windows, where
///      there is no madvise(MADV_DONTNEED)? the free space it is handed
///      lives in the SEC_RESERVE view and contains both reserved pages and
///      the PAGE_NOACCESS guard page, so the answer decides both *what* to
///      call and whether the range has to be walked rather than passed
///      whole.
///
/// build and run:
///
///   zig cc -target x86_64-windows-gnu -o wloom_probe.exe wloom_probe.c
///   ./wloom_probe.exe
///
/// checks 1-5 print PASS or FAIL. FAIL on 3 means the sparse design is
/// unsound as written and must fall back to private placeholder memory.
///
/// check 6 prints measurements rather than verdicts, since there is no
/// single right answer -- it reports, for a committed 1MB block in the
/// section view, how many pages stay resident after each candidate:
///
///   [A] DiscardVirtualMemory      the direct MADV_DONTNEED analogue
///   [B] VirtualAlloc(MEM_RESET)   no pagefile write on eviction
///   [C] VirtualUnlock             the working-set trim hack
///   [D] VirtualFree(MEM_DECOMMIT) can commit charge be reclaimed at all?
///   [E] discard a reserved page   must _ce_toss_pages() walk the range?
///   [F] discard a NOACCESS page   must it skip the guard page?
///
/// residency is measured per page with QueryWorkingSetEx rather than as a
/// process-wide RSS delta, so the numbers are not confounded by whatever
/// else the process is doing.
///
/// NB: this file still resolves VirtualAlloc2 and MapViewOfFile3 with
/// GetProcAddress, though wloom.c now links them. that is deliberate:
/// check 1 is whether they are present at all, which a probe cannot ask
/// if the loader has already refused to start it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <psapi.h>

#ifndef MEM_RESERVE_PLACEHOLDER
#  define MEM_RESERVE_PLACEHOLDER    0x00040000
#endif
#ifndef MEM_REPLACE_PLACEHOLDER
#  define MEM_REPLACE_PLACEHOLDER    0x00004000
#endif
#ifndef MEM_PRESERVE_PLACEHOLDER
#  define MEM_PRESERVE_PLACEHOLDER   0x00000002
#endif
#ifndef MEM_COALESCE_PLACEHOLDERS
#  define MEM_COALESCE_PLACEHOLDERS  0x00000001
#endif

typedef PVOID (WINAPI *valloc2_f)(HANDLE, PVOID, SIZE_T, ULONG, ULONG,
                                  void*, ULONG);
typedef PVOID (WINAPI *mapview3_f)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T,
                                   ULONG, ULONG, void*, ULONG);
typedef BOOL  (WINAPI *unmapex_f)(PVOID, ULONG);

static valloc2_f  VAlloc2;
static mapview3_f MView3;
static unmapex_f  UnmapEx;

static int fail_i = 0;

/* _probe_resident(): how many pages of [bas_v, +len_i) are physically
**                    resident, per QueryWorkingSetEx.
**
**   a per-page answer, rather than a process-wide RSS delta, so the
**   result is not confounded by whatever else the process is doing.
*/
static SIZE_T
_probe_resident(void* bas_v, SIZE_T len_i)
{
  SIZE_T pgs_i = len_i / ((SIZE_T)16 << 10);
  SIZE_T got_i = 0;
  SIZE_T i_i;

  PSAPI_WORKING_SET_EX_INFORMATION* inf_u =
    (PSAPI_WORKING_SET_EX_INFORMATION*)
      calloc(pgs_i, sizeof(PSAPI_WORKING_SET_EX_INFORMATION));

  if ( !inf_u ) {
    return 0;
  }

  for ( i_i = 0; i_i < pgs_i; i_i++ ) {
    inf_u[i_i].VirtualAddress = (char*)bas_v + (i_i * ((SIZE_T)16 << 10));
  }

  if ( QueryWorkingSetEx(GetCurrentProcess(), inf_u,
                         (DWORD)(pgs_i * sizeof(*inf_u))) )
  {
    for ( i_i = 0; i_i < pgs_i; i_i++ ) {
      if ( inf_u[i_i].VirtualAttributes.Valid ) {
        got_i++;
      }
    }
  }

  free(inf_u);
  return got_i;
}

//  vectored handler state: commit the faulting page and retry, exactly as
//  the sparse loom's fault path would.
//
static volatile int   veh_hit_i;
static volatile ULONG_PTR veh_kin_d;

#define PROBE_PAGE ((SIZE_T)16 << 10)

static LONG WINAPI
_probe_veh(struct _EXCEPTION_POINTERS* inf_u)
{
  EXCEPTION_RECORD* rec_u = inf_u->ExceptionRecord;
  void*             adr_v;

  if ( EXCEPTION_ACCESS_VIOLATION != rec_u->ExceptionCode ) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  veh_kin_d = rec_u->ExceptionInformation[0];
  adr_v = (void*)(rec_u->ExceptionInformation[1]
                  & ~(ULONG_PTR)(PROBE_PAGE - 1));

  if ( !VirtualAlloc(adr_v, PROBE_PAGE, MEM_COMMIT, PAGE_READWRITE) ) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  veh_hit_i++;
  return EXCEPTION_CONTINUE_EXECUTION;
}

static void
check(const char* nam_c, int ok_i, const char* det_c)
{
  printf("  [%s] %-52s %s\n", ok_i ? "PASS" : "FAIL", nam_c, det_c ? det_c : "");
  if ( !ok_i ) {
    fail_i++;
  }
}

//  the loom is 16GB in the failing report; use the same, so that the
//  commit-charge question is answered at the size that actually matters.
//
#define LOOM_SIZE ((SIZE_T)16 << 30)
#define GRAN      ((SIZE_T)64 << 10)
#define PAGE      ((SIZE_T)16 << 10)

int
main(void)
{
  HANDLE   sec_h;
  char*    bas_c;
  SIZE_T   haf_i = LOOM_SIZE / 2;
  MEMORY_BASIC_INFORMATION mbi;
  DWORD    old_u;

  {
    HMODULE mod_h = GetModuleHandleW(L"kernelbase.dll");
    if ( !mod_h ) mod_h = LoadLibraryW(L"kernelbase.dll");
    VAlloc2 = (valloc2_f)(void*)GetProcAddress(mod_h, "VirtualAlloc2");
    MView3  = (mapview3_f)(void*)GetProcAddress(mod_h, "MapViewOfFile3");
    UnmapEx = (unmapex_f)(void*)GetProcAddress(mod_h, "UnmapViewOfFileEx");

    check("placeholder apis available",
          VAlloc2 && MView3 && UnmapEx,
          (VAlloc2 && MView3 && UnmapEx) ? "" : "needs windows 10 1803+");
    if ( !(VAlloc2 && MView3 && UnmapEx) ) {
      return 1;
    }
  }

  //  1. SEC_RESERVE avoids commit charge
  //
  sec_h = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL,
                             PAGE_READWRITE | SEC_RESERVE,
                             (DWORD)(LOOM_SIZE >> 32),
                             (DWORD)(LOOM_SIZE & 0xffffffff), NULL);
  {
    char det_c[128] = "";
    if ( !sec_h ) {
      snprintf(det_c, sizeof(det_c), "win32 error %lu", GetLastError());
    }
    check("1. SEC_RESERVE section of 16GB", !!sec_h, det_c);
  }
  if ( !sec_h ) {
    return 1;
  }

  //  for contrast: the same section without SEC_RESERVE, which is what
  //  vere does today. expected to fail on a small box.
  //
  {
    HANDLE cmt_h = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL,
                                      PAGE_READWRITE,
                                      (DWORD)(LOOM_SIZE >> 32),
                                      (DWORD)(LOOM_SIZE & 0xffffffff), NULL);
    printf("  [INFO] committed 16GB section: %s\n",
           cmt_h ? "succeeded (box has the commit charge)"
                 : "failed (this is the bug being fixed)");
    if ( cmt_h ) {
      CloseHandle(cmt_h);
    }
  }

  //  reserve a placeholder and map the section into it, exactly as
  //  u3_wnd_loom_init() does
  //
  bas_c = (char*)VAlloc2(NULL, NULL, LOOM_SIZE,
                         MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
                         PAGE_NOACCESS, NULL, 0);
  {
    char det_c[128] = "";
    if ( !bas_c ) {
      snprintf(det_c, sizeof(det_c), "win32 error %lu", GetLastError());
    }
    check("   placeholder reservation of 16GB", !!bas_c, det_c);
  }
  if ( !bas_c ) {
    return 1;
  }

  if ( !MView3(sec_h, NULL, bas_c, 0, LOOM_SIZE,
               MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0) )
  {
    char det_c[128];
    snprintf(det_c, sizeof(det_c), "win32 error %lu", GetLastError());
    check("   map section into placeholder", 0, det_c);
    return 1;
  }
  check("   map section into placeholder", 1, "");

  //  4. VirtualProtect on an uncommitted page
  //
  {
    BOOL  ok_i = VirtualProtect(bas_c + haf_i, PAGE, PAGE_READONLY, &old_u);
    char  det_c[128];
    snprintf(det_c, sizeof(det_c),
             ok_i ? "succeeded -- yolo need not skip"
                  : "failed (win32 %lu) -- yolo must skip reserved",
             GetLastError());
    check("4. VirtualProtect on uncommitted page", 1, det_c);
    printf("       (informational: result is %s)\n", ok_i ? "BOOL TRUE" : "BOOL FALSE");
  }

  //  5. what an access violation on an uncommitted page reports, and
  //     whether committing from a vectored handler lets the faulting
  //     instruction be retried. this is exactly the mechanism the sparse
  //     loom's fault path would use.
  //
  {
    volatile char* wri_c = (volatile char*)(bas_c + haf_i + GRAN);
    volatile char* rea_c = (volatile char*)(bas_c + haf_i + (2 * GRAN));
    PVOID          veh_v = AddVectoredExceptionHandler(1, _probe_veh);

    check("   install vectored exception handler", !!veh_v, "");

    veh_hit_i = 0;
    *wri_c = 0x5a;
    check("5a. store to uncommitted page is recoverable",
          (1 == veh_hit_i) && (0x5a == *wri_c),
          (1 == veh_hit_i) ? "committed in handler, instruction retried"
                           : "handler did not fire once");
    printf("       (store reported ExceptionInformation[0] = %llu)\n",
           (unsigned long long)veh_kin_d);

    veh_hit_i = 0;
    {
      char got_c = *rea_c;
      check("5b. load from uncommitted page is recoverable",
            (1 == veh_hit_i) && (0 == got_c),
            (1 == veh_hit_i) ? "committed in handler, reads as zero"
                             : "handler did not fire once");
      printf("       (load reported ExceptionInformation[0] = %llu)\n",
             (unsigned long long)veh_kin_d);
    }

    if ( veh_v ) {
      RemoveVectoredExceptionHandler(veh_v);
    }
  }

  //  commit a page inside the view, write a pattern
  //
  {
    char* tgt_c = bas_c + haf_i;

    if ( !VirtualAlloc(tgt_c, PAGE, MEM_COMMIT, PAGE_READWRITE) ) {
      char det_c[128];
      snprintf(det_c, sizeof(det_c), "win32 error %lu", GetLastError());
      check("   commit one page inside the view", 0, det_c);
      return 1;
    }
    check("   commit one page inside the view", 1, "");

    memset(tgt_c, 0xa5, PAGE);

    VirtualQuery(tgt_c, &mbi, sizeof(mbi));
    check("   committed page reports MEM_COMMIT",
          MEM_COMMIT == mbi.State, "");
  }

  //  now the boundary move: unmap the whole view, split the placeholder,
  //  and remap the upper half at the same address. this is what
  //  _wnd_remap() does on every save that moves the image boundary.
  //
  if ( !UnmapEx(bas_c, MEM_PRESERVE_PLACEHOLDER) ) {
    char det_c[128];
    snprintf(det_c, sizeof(det_c), "win32 error %lu", GetLastError());
    check("   unmap view preserving placeholder", 0, det_c);
    return 1;
  }
  check("   unmap view preserving placeholder", 1, "");

  if ( !VirtualFree(bas_c, haf_i, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) ) {
    char det_c[128];
    snprintf(det_c, sizeof(det_c), "win32 error %lu", GetLastError());
    check("   split placeholder at the boundary", 0, det_c);
    return 1;
  }
  check("   split placeholder at the boundary", 1, "");

  if ( !MView3(sec_h, NULL, bas_c + haf_i, (ULONG64)haf_i,
               LOOM_SIZE - haf_i,
               MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0) )
  {
    char det_c[128];
    snprintf(det_c, sizeof(det_c), "win32 error %lu", GetLastError());
    check("   remap upper half at the same address", 0, det_c);
    return 1;
  }
  check("   remap upper half at the same address", 1, "");

  //  2. did the contents survive?
  //
  {
    char*  tgt_c = bas_c + haf_i;
    int    ok_i  = 1;
    SIZE_T i_i;

    VirtualQuery(tgt_c, &mbi, sizeof(mbi));

    //  3. did the commitment survive?
    //
    check("3. commitment survives unmap/remap",
          MEM_COMMIT == mbi.State,
          (MEM_COMMIT == mbi.State)
            ? "still MEM_COMMIT"
            : "NOT committed -- sparse design is unsound");

    if ( MEM_COMMIT == mbi.State ) {
      for ( i_i = 0; i_i < PAGE; i_i++ ) {
        if ( (unsigned char)tgt_c[i_i] != 0xa5 ) {
          ok_i = 0;
          break;
        }
      }
      check("2. contents survive unmap/remap", ok_i,
            ok_i ? "pattern intact" : "pattern lost -- design is unsound");
    }
    else {
      check("2. contents survive unmap/remap", 0, "skipped, not committed");
    }
  }

  //  6. how to implement _ce_toss_pages() on windows.
  //
  //     the free space between the heap and the stack lives in the
  //     SEC_RESERVE view, so whatever replaces madvise(MADV_DONTNEED)
  //     has to work on a section view, and has to cope with the reserved
  //     pages and the PAGE_NOACCESS guard page inside the range.
  //
  //     these checks are informational -- they do not count as failures.
  //     what we want out of them is which mechanism actually frees pages.
  //
  printf("\n--- toss mechanisms (informational) ---\n");
  {
    char*  tos_c = bas_c + haf_i + (16 * GRAN);
    SIZE_T tos_i = 64 * PAGE;                    //  1MB
    DWORD  ret_u;
    SIZE_T i_i;

    if ( !VirtualAlloc(tos_c, tos_i, MEM_COMMIT, PAGE_READWRITE) ) {
      printf("  [SKIP] could not commit the toss block: win32 %lu\n",
             GetLastError());
    }
    else {
      memset(tos_c, 0x5a, tos_i);
      printf("  resident before: %zu of %zu pages\n",
             _probe_resident(tos_c, tos_i), tos_i / PAGE);

      //  A. DiscardVirtualMemory -- the direct MADV_DONTNEED analogue.
      //     returns a win32 error code, not a BOOL. 0 is success.
      //
      ret_u = DiscardVirtualMemory(tos_c, tos_i);
      printf("  [A] DiscardVirtualMemory       -> %s (%lu), resident now %zu\n",
             ret_u ? "FAILED" : "ok", (unsigned long)ret_u,
             _probe_resident(tos_c, tos_i));

      if ( !ret_u ) {
        //  contents are documented as undefined after a discard; linux
        //  zeroes them. record which we get, so nothing comes to depend
        //  on the wrong one.
        //
        int zer_i = 1;
        for ( i_i = 0; i_i < PAGE; i_i++ ) {
          if ( tos_c[i_i] ) { zer_i = 0; break; }
        }
        printf("      contents after discard: %s\n",
               zer_i ? "zeroed (as linux)" : "retained/undefined");
      }

      //  B. MEM_RESET -- contents are worthless, skip the pagefile write
      //     on eviction. does not free pages by itself.
      //
      memset(tos_c, 0x5a, tos_i);
      printf("  [B] VirtualAlloc(MEM_RESET)    -> %s, resident now %zu\n",
             VirtualAlloc(tos_c, tos_i, MEM_RESET, PAGE_READWRITE)
               ? "ok" : "FAILED",
             _probe_resident(tos_c, tos_i));

      //  C. VirtualUnlock -- the working-set trim hack. expected to
      //     return FALSE with ERROR_NOT_LOCKED (158) and trim anyway.
      //
      memset(tos_c, 0x5a, tos_i);
      ret_u = VirtualUnlock(tos_c, tos_i) ? 0 : GetLastError();
      printf("  [C] VirtualUnlock              -> %s (%lu), resident now %zu\n",
             ret_u ? "FALSE" : "TRUE", (unsigned long)ret_u,
             _probe_resident(tos_c, tos_i));

      //  D. can committed section pages be decommitted at all? if not,
      //     commit charge for the sparse loom is a high-water mark.
      //
      printf("  [D] VirtualFree(MEM_DECOMMIT)  -> %s (%lu)\n",
             VirtualFree(tos_c, tos_i, MEM_DECOMMIT) ? "ok" : "FAILED",
             (unsigned long)GetLastError());
    }

    //  the range _ce_toss_pages() is handed also contains reserved pages
    //  and the guard page. if either rejects the call, the windows
    //  implementation must walk the range with VirtualQuery rather than
    //  making one call over the whole span.
    //
    {
      char* res_c = bas_c + haf_i + (64 * GRAN);   //  never committed
      char* nac_c = tos_c;

      printf("  [E] discard a RESERVED page    -> %lu (nonzero => must walk)\n",
             (unsigned long)DiscardVirtualMemory(res_c, PAGE));

      if ( VirtualAlloc(nac_c, PAGE, MEM_COMMIT, PAGE_READWRITE)
           && VirtualProtect(nac_c, PAGE, PAGE_NOACCESS, &old_u) )
      {
        printf("  [F] discard a NOACCESS page    -> %lu (nonzero => must skip guard)\n",
               (unsigned long)DiscardVirtualMemory(nac_c, PAGE));
      }
      else {
        printf("  [F] discard a NOACCESS page    -> setup failed, skipped\n");
      }
    }
  }

  printf("\n%s (%d failure%s)\n",
         fail_i ? "UNSOUND" : "SOUND",
         fail_i, (1 == fail_i) ? "" : "s");

  return fail_i ? 1 : 0;
}
