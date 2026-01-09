#include "portable.h"
#include <fcntl.h>
#include <sys/utime.h>
#include <windows.h>
#include <tlhelp32.h>
#include "errno.h"

// Shared memory emulation for shm_open/shm_unlink
// -----------------------------------------------------------------------

#define SHM_FD_BASE   10000    // base value for shm pseudo-fds
#define SHM_MAX_SLOTS 8        // max concurrent shm objects

typedef struct {
  char   nam_c[256];   // Windows name (e.g. "Local\\spin_stack_page_123")
  HANDLE han_h;        // file mapping handle
  size_t siz_i;        // size set by ftruncate
  int    use_i;        // slot in use
} shm_slot;

static shm_slot _shm_slots[SHM_MAX_SLOTS] = {0};

/* _shm_convert_name(): convert POSIX shm name to Windows format.
**   "/spin_stack_page_123" -> "Local\\spin_stack_page_123"
*/
static void
_shm_convert_name(const char* posix_name, char* win_name, size_t win_len)
{
  const char* src = posix_name;
  if ( '/' == *src ) {
    src++;  // skip leading slash
  }
  snprintf(win_name, win_len, "Local\\%s", src);
}

/* _shm_fd_to_slot(): convert shm fd to slot index, or -1 if invalid.
*/
static int
_shm_fd_to_slot(int fd)
{
  if ( fd < SHM_FD_BASE || fd >= SHM_FD_BASE + SHM_MAX_SLOTS ) {
    return -1;
  }
  int idx = fd - SHM_FD_BASE;
  if ( !_shm_slots[idx].use_i ) {
    return -1;
  }
  return idx;
}

/* _shm_is_fd(): return non-zero if fd is a shm pseudo-fd.
*/
static int
_shm_is_fd(int fd)
{
  return _shm_fd_to_slot(fd) >= 0;
}

// forward declarations for shm functions used by mmap/ftruncate
static int   shm_ftruncate(int fd, off_t length);
static void* shm_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);

// set default CRT file mode to binary
// note that mingw binmode.o does nothing
#undef _fmode
int _fmode = _O_BINARY;

// set standard I/O fds to binary too, because
// MSVCRT creates them before MingW sets _fmode
static void __attribute__ ((constructor)) _set_stdio_to_binary()
{
    _setmode(0, _O_BINARY);
    _setmode(1, _O_BINARY);
    _setmode(2, _O_BINARY);
}

// from https://github.com/git/git/blob/master/compat/mingw.c
// -----------------------------------------------------------------------

int err_win_to_posix(DWORD winerr)
{
	int error = ENOSYS;
	switch(winerr) {
	case ERROR_ACCESS_DENIED: error = EACCES; break;
	case ERROR_ACCOUNT_DISABLED: error = EACCES; break;
	case ERROR_ACCOUNT_RESTRICTION: error = EACCES; break;
	case ERROR_ALREADY_ASSIGNED: error = EBUSY; break;
	case ERROR_ALREADY_EXISTS: error = EEXIST; break;
	case ERROR_ARITHMETIC_OVERFLOW: error = ERANGE; break;
	case ERROR_BAD_COMMAND: error = EIO; break;
	case ERROR_BAD_DEVICE: error = ENODEV; break;
	case ERROR_BAD_DRIVER_LEVEL: error = ENXIO; break;
	case ERROR_BAD_EXE_FORMAT: error = ENOEXEC; break;
	case ERROR_BAD_FORMAT: error = ENOEXEC; break;
	case ERROR_BAD_LENGTH: error = EINVAL; break;
	case ERROR_BAD_PATHNAME: error = ENOENT; break;
	case ERROR_BAD_PIPE: error = EPIPE; break;
	case ERROR_BAD_UNIT: error = ENODEV; break;
	case ERROR_BAD_USERNAME: error = EINVAL; break;
	case ERROR_BROKEN_PIPE: error = EPIPE; break;
	case ERROR_BUFFER_OVERFLOW: error = ENAMETOOLONG; break;
	case ERROR_BUSY: error = EBUSY; break;
	case ERROR_BUSY_DRIVE: error = EBUSY; break;
	case ERROR_CALL_NOT_IMPLEMENTED: error = ENOSYS; break;
	case ERROR_CANNOT_MAKE: error = EACCES; break;
	case ERROR_CANTOPEN: error = EIO; break;
	case ERROR_CANTREAD: error = EIO; break;
	case ERROR_CANTWRITE: error = EIO; break;
	case ERROR_CRC: error = EIO; break;
	case ERROR_CURRENT_DIRECTORY: error = EACCES; break;
	case ERROR_DEVICE_IN_USE: error = EBUSY; break;
	case ERROR_DEV_NOT_EXIST: error = ENODEV; break;
	case ERROR_DIRECTORY: error = EINVAL; break;
	case ERROR_DIR_NOT_EMPTY: error = ENOTEMPTY; break;
	case ERROR_DISK_CHANGE: error = EIO; break;
	case ERROR_DISK_FULL: error = ENOSPC; break;
	case ERROR_DRIVE_LOCKED: error = EBUSY; break;
	case ERROR_ENVVAR_NOT_FOUND: error = EINVAL; break;
	case ERROR_EXE_MARKED_INVALID: error = ENOEXEC; break;
	case ERROR_FILENAME_EXCED_RANGE: error = ENAMETOOLONG; break;
	case ERROR_FILE_EXISTS: error = EEXIST; break;
	case ERROR_FILE_INVALID: error = ENODEV; break;
	case ERROR_FILE_NOT_FOUND: error = ENOENT; break;
	case ERROR_GEN_FAILURE: error = EIO; break;
	case ERROR_HANDLE_DISK_FULL: error = ENOSPC; break;
	case ERROR_INSUFFICIENT_BUFFER: error = ENOMEM; break;
	case ERROR_INVALID_ACCESS: error = EACCES; break;
	case ERROR_INVALID_ADDRESS: error = EFAULT; break;
	case ERROR_INVALID_BLOCK: error = EFAULT; break;
	case ERROR_INVALID_DATA: error = EINVAL; break;
	case ERROR_INVALID_DRIVE: error = ENODEV; break;
	case ERROR_INVALID_EXE_SIGNATURE: error = ENOEXEC; break;
	case ERROR_INVALID_FLAGS: error = EINVAL; break;
	case ERROR_INVALID_FUNCTION: error = ENOSYS; break;
	case ERROR_INVALID_HANDLE: error = EBADF; break;
	case ERROR_INVALID_LOGON_HOURS: error = EACCES; break;
	case ERROR_INVALID_NAME: error = EINVAL; break;
	case ERROR_INVALID_OWNER: error = EINVAL; break;
	case ERROR_INVALID_PARAMETER: error = EINVAL; break;
	case ERROR_INVALID_PASSWORD: error = EPERM; break;
	case ERROR_INVALID_PRIMARY_GROUP: error = EINVAL; break;
	case ERROR_INVALID_SIGNAL_NUMBER: error = EINVAL; break;
	case ERROR_INVALID_TARGET_HANDLE: error = EIO; break;
	case ERROR_INVALID_WORKSTATION: error = EACCES; break;
	case ERROR_IO_DEVICE: error = EIO; break;
	case ERROR_IO_INCOMPLETE: error = EINTR; break;
	case ERROR_LOCKED: error = EBUSY; break;
	case ERROR_LOCK_VIOLATION: error = EACCES; break;
	case ERROR_LOGON_FAILURE: error = EACCES; break;
	case ERROR_MAPPED_ALIGNMENT: error = EINVAL; break;
	case ERROR_META_EXPANSION_TOO_LONG: error = E2BIG; break;
	case ERROR_MORE_DATA: error = EPIPE; break;
	case ERROR_NEGATIVE_SEEK: error = ESPIPE; break;
	case ERROR_NOACCESS: error = EFAULT; break;
	case ERROR_NONE_MAPPED: error = EINVAL; break;
	case ERROR_NOT_ENOUGH_MEMORY: error = ENOMEM; break;
	case ERROR_NOT_READY: error = EAGAIN; break;
	case ERROR_NOT_SAME_DEVICE: error = EXDEV; break;
	case ERROR_NO_DATA: error = EPIPE; break;
	case ERROR_NO_MORE_SEARCH_HANDLES: error = EIO; break;
	case ERROR_NO_PROC_SLOTS: error = EAGAIN; break;
	case ERROR_NO_SUCH_PRIVILEGE: error = EACCES; break;
	case ERROR_OPEN_FAILED: error = EIO; break;
	case ERROR_OPEN_FILES: error = EBUSY; break;
	case ERROR_OPERATION_ABORTED: error = EINTR; break;
	case ERROR_OUTOFMEMORY: error = ENOMEM; break;
	case ERROR_PASSWORD_EXPIRED: error = EACCES; break;
	case ERROR_PATH_BUSY: error = EBUSY; break;
	case ERROR_PATH_NOT_FOUND: error = ENOENT; break;
	case ERROR_PIPE_BUSY: error = EBUSY; break;
	case ERROR_PIPE_CONNECTED: error = EPIPE; break;
	case ERROR_PIPE_LISTENING: error = EPIPE; break;
	case ERROR_PIPE_NOT_CONNECTED: error = EPIPE; break;
	case ERROR_PRIVILEGE_NOT_HELD: error = EACCES; break;
	case ERROR_READ_FAULT: error = EIO; break;
	case ERROR_SEEK: error = EIO; break;
	case ERROR_SEEK_ON_DEVICE: error = ESPIPE; break;
	case ERROR_SHARING_BUFFER_EXCEEDED: error = ENFILE; break;
	case ERROR_SHARING_VIOLATION: error = EACCES; break;
	case ERROR_STACK_OVERFLOW: error = ENOMEM; break;
	case ERROR_SUCCESS: error = 0; break;
	case ERROR_SWAPERROR: error = ENOENT; break;
	case ERROR_TOO_MANY_MODULES: error = EMFILE; break;
	case ERROR_TOO_MANY_OPEN_FILES: error = EMFILE; break;
	case ERROR_UNRECOGNIZED_MEDIA: error = ENXIO; break;
	case ERROR_UNRECOGNIZED_VOLUME: error = ENODEV; break;
	case ERROR_WAIT_NO_CHILDREN: error = ECHILD; break;
	case ERROR_WRITE_FAULT: error = EIO; break;
	case ERROR_WRITE_PROTECT: error = EROFS; break;
	}
	return error;
}

int link(const char *path1, const char *path2)
{
  if ( CreateHardLinkA(path2, path1, NULL) ) {
    return 0;
  }

  errno = err_win_to_posix(GetLastError());
  return -1;
}

// from msys2 mingw-packages-dev patches
// -----------------------------------------------------------------------

static DWORD __map_mmap_prot_page(const int prot)
{
    DWORD protect = 0;

    if (prot == PROT_NONE)
        return protect;

    if ((prot & PROT_EXEC) != 0)
    {
        protect = ((prot & PROT_WRITE) != 0) ?
                    PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    }
    else
    {
        protect = ((prot & PROT_WRITE) != 0) ?
                    PAGE_READWRITE : PAGE_READONLY;
    }

    return protect;
}

static DWORD __map_mmap_prot_file(const int prot)
{
    DWORD desiredAccess = 0;

    if (prot == PROT_NONE)
        return desiredAccess;

    if ((prot & PROT_READ) != 0)
        desiredAccess |= FILE_MAP_READ;
    if ((prot & PROT_WRITE) != 0)
        desiredAccess |= FILE_MAP_WRITE;
    if ((prot & PROT_EXEC) != 0)
        desiredAccess |= FILE_MAP_EXECUTE;

    return desiredAccess;
}

void* mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
    HANDLE fm, h;

    void * map = MAP_FAILED;

    const DWORD dwFileOffsetLow = (sizeof(off_t) <= sizeof(DWORD)) ?
                    (DWORD)off : (DWORD)(off & 0xFFFFFFFFL);
    const DWORD dwFileOffsetHigh = (sizeof(off_t) <= sizeof(DWORD)) ?
                    (DWORD)0 : (DWORD)((off >> 32) & 0xFFFFFFFFL);
    const DWORD protect = __map_mmap_prot_page(prot);
    const DWORD desiredAccess = __map_mmap_prot_file(prot);

    errno = 0;

    if (len == 0
        /* Usupported protection combinations */
        || prot == PROT_EXEC)
    {
        errno = EINVAL;
        return MAP_FAILED;
    }

    // check if this is a shm pseudo-fd
    if ( _shm_is_fd(fildes) ) {
        return shm_mmap(addr, len, prot, flags, fildes, off);
    }

    if ((flags & MAP_ANON) == 0)
    {
        h = (HANDLE)_get_osfhandle(fildes);

        if (h == INVALID_HANDLE_VALUE)
        {
            errno = EBADF;
            return MAP_FAILED;
        }
    }
    else h = INVALID_HANDLE_VALUE;

    fm = CreateFileMapping(h, NULL, protect, 0, len, NULL);

    if (fm == NULL)
    {
        errno = err_win_to_posix(GetLastError());
        return MAP_FAILED;
    }

    map = MapViewOfFileEx(fm, desiredAccess, dwFileOffsetHigh, dwFileOffsetLow, len, addr);
    errno = err_win_to_posix(GetLastError());

    CloseHandle(fm);

    if (map == NULL)
        return MAP_FAILED;

    if ((flags & MAP_FIXED) != 0 && map != addr)
    {
        UnmapViewOfFile(map);
        errno = EEXIST;
        return MAP_FAILED;
    }

    return map;
}

int munmap(void *addr, size_t len)
{
    if (UnmapViewOfFile(addr))
        return 0;

    errno = err_win_to_posix(GetLastError());
    return -1;
}

int msync(void *addr, size_t len, int flags)
{
    if (FlushViewOfFile(addr, len))
        return 0;

    errno = err_win_to_posix(GetLastError());
    return -1;
}

// -----------------------------------------------------------------------

// vere uses kill() only to kill lockfile owner with SIGTERM or SIGKILL
// Windows does not have signals, so I handle SIGKILL as TerminateProcess()
// and return an error in all other cases
int kill(pid_t pid, int sig)
{
    if (pid > 0 && sig == SIGKILL) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);

        if (TerminateProcess(h, -1)) {
            CloseHandle(h);
            return 0;
        }

        errno = err_win_to_posix(GetLastError());
        CloseHandle(h);
        return -1;
    }

    errno = EINVAL;
    return -1;
}

// libgcc built for mingw has included an implementation of mprotect
// via VirtualProtect since olden days, but it takes int rather than size_t
// and therefore fails or does unexpected things for >2GB blocks on 64-bit
// https://github.com/gcc-mirror/gcc/blob/master/libgcc/libgcc2.c
int mprotect (void *addr, size_t len, int prot)
{
    DWORD np, op;

    if (prot == (PROT_READ | PROT_WRITE | PROT_EXEC))
        np = PAGE_EXECUTE_READWRITE;
    else if (prot == (PROT_READ | PROT_EXEC))
        np = PAGE_EXECUTE_READ;
    else if (prot == (PROT_EXEC))
        np = PAGE_EXECUTE;
    else if (prot == (PROT_READ | PROT_WRITE))
        np = PAGE_READWRITE;
    else if (prot == (PROT_READ))
        np = PAGE_READONLY;
    else if (prot == 0)
        np = PAGE_NOACCESS;
    else
    {
        errno = EINVAL;
        return -1;
    }

    if (VirtualProtect (addr, len, np, &op))
        return 0;

    // NB: return code of ntdll!RtlGetLastNtStatus() is useful
    // for diagnosing obscure VirtualProtect failures
    errno = err_win_to_posix(GetLastError());
    return -1;
}

int utimes(const char *path, const struct timeval times[2])
{
    struct _utimbuf utb = {.actime = times[0].tv_sec, .modtime = times[1].tv_sec};
    return _utime(path, &utb);
}

int fdatasync(int fildes)
{
    HANDLE h = (HANDLE)_get_osfhandle(fildes);

    if (h == INVALID_HANDLE_VALUE)
    {
        errno = EBADF;
        return -1;
    }

    if (FlushFileBuffers(h))
    {
        errno = 0;
        return 0;
    }
    else
    {
        errno = err_win_to_posix(GetLastError());
        return -1;
    }
}

int ftruncate(int fildes, off_t length)
{
    // check if this is a shm pseudo-fd
    if ( _shm_is_fd(fildes) ) {
        return shm_ftruncate(fildes, length);
    }

    // regular file: use _chsize_s (64-bit safe)
    if ( 0 == _chsize_s(fildes, length) ) {
        return 0;
    }

    errno = err_win_to_posix(GetLastError());
    return -1;
}

intmax_t mdb_get_filesize(HANDLE han_u)
{
    LARGE_INTEGER li;
    GetFileSizeEx(han_u, &li);
    return li.QuadPart;
}

char *realpath(const char *path, char *resolved_path)
{
  //  XX  MAX_PATH
  //
  return _fullpath(resolved_path, path, MAX_PATH);
}

long sysconf(int name)
{
  SYSTEM_INFO si;

  if ( _SC_PAGESIZE != name ) {
    return -1;
  }
  GetNativeSystemInfo(&si);
  return si.dwPageSize;
}


// musl memmem for windows

static char *twobyte_memmem(const unsigned char *h, size_t k, const unsigned char *n)
{
	uint16_t nw = n[0]<<8 | n[1], hw = h[0]<<8 | h[1];
	for (h++, k--; k; k--, hw = hw<<8 | *++h)
		if (hw == nw) return (char *)h-1;
	return 0;
}

static char *threebyte_memmem(const unsigned char *h, size_t k, const unsigned char *n)
{
	uint32_t nw = n[0]<<24 | n[1]<<16 | n[2]<<8;
	uint32_t hw = h[0]<<24 | h[1]<<16 | h[2]<<8;
	for (h+=2, k-=2; k; k--, hw = (hw|*++h)<<8)
		if (hw == nw) return (char *)h-2;
	return 0;
}

static char *fourbyte_memmem(const unsigned char *h, size_t k, const unsigned char *n)
{
	uint32_t nw = n[0]<<24 | n[1]<<16 | n[2]<<8 | n[3];
	uint32_t hw = h[0]<<24 | h[1]<<16 | h[2]<<8 | h[3];
	for (h+=3, k-=3; k; k--, hw = hw<<8 | *++h)
		if (hw == nw) return (char *)h-3;
	return 0;
}

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define BITOP(a,b,op) \
 ((a)[(size_t)(b)/(8*sizeof *(a))] op (size_t)1<<((size_t)(b)%(8*sizeof *(a))))

static char *twoway_memmem(const unsigned char *h, const unsigned char *z, const unsigned char *n, size_t l)
{
	size_t i, ip, jp, k, p, ms, p0, mem, mem0;
	size_t byteset[32 / sizeof(size_t)] = { 0 };
	size_t shift[256];

	/* Computing length of needle and fill shift table */
	for (i=0; i<l; i++)
		BITOP(byteset, n[i], |=), shift[n[i]] = i+1;

	/* Compute maximal suffix */
	ip = -1; jp = 0; k = p = 1;
	while (jp+k<l) {
		if (n[ip+k] == n[jp+k]) {
			if (k == p) {
				jp += p;
				k = 1;
			} else k++;
		} else if (n[ip+k] > n[jp+k]) {
			jp += k;
			k = 1;
			p = jp - ip;
		} else {
			ip = jp++;
			k = p = 1;
		}
	}
	ms = ip;
	p0 = p;

	/* And with the opposite comparison */
	ip = -1; jp = 0; k = p = 1;
	while (jp+k<l) {
		if (n[ip+k] == n[jp+k]) {
			if (k == p) {
				jp += p;
				k = 1;
			} else k++;
		} else if (n[ip+k] < n[jp+k]) {
			jp += k;
			k = 1;
			p = jp - ip;
		} else {
			ip = jp++;
			k = p = 1;
		}
	}
	if (ip+1 > ms+1) ms = ip;
	else p = p0;

	/* Periodic needle? */
	if (memcmp(n, n+p, ms+1)) {
		mem0 = 0;
		p = MAX(ms, l-ms-1) + 1;
	} else mem0 = l-p;
	mem = 0;

	/* Search loop */
	for (;;) {
		/* If remainder of haystack is shorter than needle, done */
		if (z-h < l) return 0;

		/* Check last byte first; advance by shift on mismatch */
		if (BITOP(byteset, h[l-1], &)) {
			k = l-shift[h[l-1]];
			if (k) {
				if (mem0 && mem && k < p) k = l-p;
				h += k;
				mem = 0;
				continue;
			}
		} else {
			h += l;
			mem = 0;
			continue;
		}

		/* Compare right half */
		for (k=MAX(ms+1,mem); k<l && n[k] == h[k]; k++);
		if (k < l) {
			h += k-ms;
			mem = 0;
			continue;
		}
		/* Compare left half */
		for (k=ms+1; k>mem && n[k-1] == h[k-1]; k--);
		if (k <= mem) return (char *)h;
		h += p;
		mem = mem0;
	}
}

void *memmem(const void *h0, size_t k, const void *n0, size_t l)
{
	const unsigned char *h = h0, *n = n0;

	/* Return immediately on empty needle */
	if (!l) return (void *)h;

	/* Return immediately when needle is longer than haystack */
	if (k<l) return 0;

	/* Use faster algorithms for short needles */
	h = memchr(h0, *n, k);
	if (!h || l==1) return (void *)h;
	k -= h - (const unsigned char *)h0;
	if (l==2) return twobyte_memmem(h, k, n);
	if (l==3) return threebyte_memmem(h, k, n);
	if (l==4) return fourbyte_memmem(h, k, n);

	return twoway_memmem(h, h+k, n, l);
}

uint32_t getppid()
{
  HANDLE hSnapshot;
  PROCESSENTRY32 pe32;
  DWORD ppid = 0, pid = GetCurrentProcessId();

  hSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
  __try{
    if( hSnapshot == INVALID_HANDLE_VALUE ) __leave;

    ZeroMemory( &pe32, sizeof( pe32 ) );
    pe32.dwSize = sizeof( pe32 );
    if( !Process32First( hSnapshot, &pe32 ) ) __leave;

    do{
      if( pe32.th32ProcessID == pid ){
        ppid = pe32.th32ParentProcessID;
        break;
      }
    }while( Process32Next( hSnapshot, &pe32 ) );

  }
  __finally{
    if( hSnapshot != INVALID_HANDLE_VALUE ) CloseHandle( hSnapshot );
  }
  return ppid;
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset)
{
  DWORD len = 0;

  OVERLAPPED overlapped = {0};

  overlapped.OffsetHigh = (sizeof(off_t) <= sizeof(DWORD)) ?
                          (DWORD)0 : (DWORD)((offset >> 32) & 0xFFFFFFFFL);
  overlapped.Offset     = (sizeof(off_t) <= sizeof(DWORD)) ?
                          (DWORD)offset : (DWORD)(offset & 0xFFFFFFFFL);

  HANDLE h = (HANDLE)_get_osfhandle(fd);

  if ( INVALID_HANDLE_VALUE == h ) {
    errno = EBADF;
    return -1;
  }

  if ( !ReadFile(h, buf, count, &len, &overlapped) ) {
    DWORD err = GetLastError();

    if ( ERROR_HANDLE_EOF != err ) {
      errno = err_win_to_posix(err);
      return -1;
    }
  }

  return (ssize_t)len;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
{
  DWORD len = 0;

  OVERLAPPED overlapped = {0};

  overlapped.OffsetHigh = (sizeof(off_t) <= sizeof(DWORD)) ?
                          (DWORD)0 : (DWORD)((offset >> 32) & 0xFFFFFFFFL);
  overlapped.Offset     = (sizeof(off_t) <= sizeof(DWORD)) ?
                          (DWORD)offset : (DWORD)(offset & 0xFFFFFFFFL);

  HANDLE h = (HANDLE)_get_osfhandle(fd);

  if ( INVALID_HANDLE_VALUE == h ) {
    errno = EBADF;
    return -1;
  }

  if ( !WriteFile(h, buf, count, &len, &overlapped) ) {
    errno = err_win_to_posix(GetLastError());
    return -1;
  }

  return (ssize_t)len;
}

// POSIX shared memory emulation
// -----------------------------------------------------------------------

/* shm_open(): create or open a POSIX shared memory object.
**
**   On Windows, we use named file mappings backed by the page file.
**   The actual mapping is deferred to mmap() since we don't know
**   the size until ftruncate() is called.
*/
int shm_open(const char *name, int oflag, mode_t mode)
{
  (void)mode;  // Windows doesn't use POSIX permissions for shared memory

  // find a free slot
  int idx;
  for ( idx = 0; idx < SHM_MAX_SLOTS; idx++ ) {
    if ( !_shm_slots[idx].use_i ) {
      break;
    }
  }

  if ( SHM_MAX_SLOTS == idx ) {
    errno = EMFILE;  // too many open shm objects
    return -1;
  }

  // convert POSIX name to Windows name
  _shm_convert_name(name, _shm_slots[idx].nam_c, sizeof(_shm_slots[idx].nam_c));

  // try to open existing mapping first if not O_EXCL
  if ( !(oflag & O_EXCL) ) {
    HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, _shm_slots[idx].nam_c);
    if ( NULL != h ) {
      _shm_slots[idx].han_h = h;
      _shm_slots[idx].siz_i = 0;  // size unknown for existing mapping
      _shm_slots[idx].use_i = 1;
      return SHM_FD_BASE + idx;
    }
    // if O_CREAT not set, fail
    if ( !(oflag & O_CREAT) ) {
      errno = ENOENT;
      return -1;
    }
  }

  // O_CREAT: will create the mapping in ftruncate/mmap when we know the size
  _shm_slots[idx].han_h = NULL;
  _shm_slots[idx].siz_i = 0;
  _shm_slots[idx].use_i = 1;

  return SHM_FD_BASE + idx;
}

/* shm_unlink(): remove a shared memory object name.
**
**   On Windows, named objects are reference-counted and automatically
**   cleaned up when the last handle is closed. We just mark our slot
**   as available; the actual cleanup happens when all processes close
**   their handles.
*/
int shm_unlink(const char *name)
{
  char win_name[256];
  _shm_convert_name(name, win_name, sizeof(win_name));

  // find and close any local handle to this name
  for ( int idx = 0; idx < SHM_MAX_SLOTS; idx++ ) {
    if ( _shm_slots[idx].use_i &&
         0 == strcmp(_shm_slots[idx].nam_c, win_name) ) {
      if ( NULL != _shm_slots[idx].han_h ) {
        CloseHandle(_shm_slots[idx].han_h);
      }
      _shm_slots[idx].use_i = 0;
      _shm_slots[idx].han_h = NULL;
      _shm_slots[idx].nam_c[0] = 0;
      return 0;
    }
  }

  // not found locally - that's OK, the object may exist in another process
  return 0;
}

/* shm_ftruncate(): set size of shared memory object.
**
**   On Windows, we create the actual file mapping here since
**   CreateFileMapping requires the size upfront.
*/
static int shm_ftruncate(int fd, off_t length)
{
  int idx = _shm_fd_to_slot(fd);
  if ( idx < 0 ) {
    errno = EBADF;
    return -1;
  }

  // if we already have a handle with correct size, nothing to do
  if ( NULL != _shm_slots[idx].han_h && _shm_slots[idx].siz_i == (size_t)length ) {
    return 0;
  }

  // close existing handle if any
  if ( NULL != _shm_slots[idx].han_h ) {
    CloseHandle(_shm_slots[idx].han_h);
    _shm_slots[idx].han_h = NULL;
  }

  // create the named file mapping with the specified size
  // cast to 64-bit to avoid shift overflow on 32-bit off_t
  uint64_t len64 = (uint64_t)length;
  DWORD siz_hi = (DWORD)((len64 >> 32) & 0xFFFFFFFFL);
  DWORD siz_lo = (DWORD)(len64 & 0xFFFFFFFFL);

  HANDLE h = CreateFileMappingA(
    INVALID_HANDLE_VALUE,   // backed by page file
    NULL,                   // default security
    PAGE_READWRITE,         // read/write access
    siz_hi,                 // size high
    siz_lo,                 // size low
    _shm_slots[idx].nam_c   // name
  );

  if ( NULL == h ) {
    errno = err_win_to_posix(GetLastError());
    return -1;
  }

  _shm_slots[idx].han_h = h;
  _shm_slots[idx].siz_i = length;
  return 0;
}

/* shm_mmap(): map shared memory object into address space.
*/
static void* shm_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
  (void)flags;  // always MAP_SHARED for shm
  (void)off;    // always 0 for shm

  int idx = _shm_fd_to_slot(fd);
  if ( idx < 0 ) {
    errno = EBADF;
    return MAP_FAILED;
  }

  // if no handle yet, try to open existing mapping (for O_RDONLY case)
  if ( NULL == _shm_slots[idx].han_h ) {
    HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, _shm_slots[idx].nam_c);
    if ( NULL == h ) {
      errno = err_win_to_posix(GetLastError());
      return MAP_FAILED;
    }
    _shm_slots[idx].han_h = h;
  }

  DWORD access = 0;
  if ( prot & PROT_WRITE ) {
    access = FILE_MAP_WRITE;
  } else if ( prot & PROT_READ ) {
    access = FILE_MAP_READ;
  }
  if ( prot & PROT_EXEC ) {
    access |= FILE_MAP_EXECUTE;
  }

  void* ptr = MapViewOfFileEx(
    _shm_slots[idx].han_h,
    access,
    0,    // offset high
    0,    // offset low
    len,
    addr
  );

  if ( NULL == ptr ) {
    errno = err_win_to_posix(GetLastError());
    return MAP_FAILED;
  }

  return ptr;
}
