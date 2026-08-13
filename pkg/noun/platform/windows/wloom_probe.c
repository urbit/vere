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
///
/// build and run:
///
///   zig cc -target x86_64-windows-gnu -o wloom_probe.exe wloom_probe.c
///   ./wloom_probe.exe
///
/// every check prints PASS or FAIL. FAIL on 3 means the sparse design is
/// unsound as written and must fall back to private placeholder memory.

#include <stdio.h>
#include <windows.h>

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

  printf("\n%s (%d failure%s)\n",
         fail_i ? "UNSOUND" : "SOUND",
         fail_i, (1 == fail_i) ? "" : "s");

  return fail_i ? 1 : 0;
}
