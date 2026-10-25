/* The few operating system calls the PC frontend needs that differ between Linux and Windows (mingw-w64).  Header only. */
#ifndef PC_COMPAT_H
#define PC_COMPAT_H

#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define PC_MKDIR(p) _mkdir(p)
#define PC_FSYNC(fd) _commit(fd)
#define PC_DLOPEN(p) ((void *)LoadLibraryA(p))
#define PC_DLSYM(h, n) ((void *)GetProcAddress((HMODULE)(h), n))
#define PC_DLERROR() "LoadLibrary failed"
#define PC_CORE_NAME "snes9x_widescreen_libretro.dll"
/* rename() on Windows refuses to replace an existing file */
static inline double pc_now_s(void) {
  LARGE_INTEGER f, c;
  QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c);
  return (double)c.QuadPart / (double)f.QuadPart;
}
static inline int pc_rename(const char *from, const char *to) { return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1; }
static inline struct tm *pc_localtime(const time_t *t, struct tm *out) { return localtime_s(out, t) == 0 ? out : NULL; }
#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#define PC_MKDIR(p) mkdir(p, 0755)
#define PC_FSYNC(fd) fsync(fd)
#define PC_DLOPEN(p) dlopen(p, RTLD_NOW)
#define PC_DLSYM(h, n) dlsym(h, n)
#define PC_DLERROR() dlerror()
#define PC_CORE_NAME "snes9x_widescreen_libretro.so"
static inline double pc_now_s(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
static inline int pc_rename(const char *from, const char *to) { return rename(from, to); }
static inline struct tm *pc_localtime(const time_t *t, struct tm *out) { return localtime_r(t, out); }
#endif

#endif
