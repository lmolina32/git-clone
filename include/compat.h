/* compat.h: Portable access to stsat timestamps */

#ifndef COMPAT_H 
#define COMPAT_H

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define ST_CTIME_SEC(st)    ((uint32_t)(st).st_ctimespec.tv_sec)
#define ST_CTIME_NSEC(st)   ((uint32_t)(st).st_ctimespec.tv_nsec)
#define ST_MTIME_SEC(st)    ((uint32_t)(st).st_mtimespec.tv_sec)
#define ST_MTIME_NSEC(st)   ((uint32_t)(st).st_mtimespec.tv_nsec)
#else
#define ST_CTIME_SEC(st)    ((uint32_t)(st).st_ctim.tv_sec)
#define ST_CTIME_NSEC(st)   ((uint32_t)(st).st_ctim.tv_nsec)
#define ST_MTIME_SEC(st)    ((uint32_t)(st).st_mtim.tv_sec)
#define ST_MTIME_NSEC(st)   ((uint32_t)(st).st_mtim.tv_nsec)
#endif

#endif 