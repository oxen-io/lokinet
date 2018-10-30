/**
 * sysconf.h -- system-dependent macros and settings
 *
 * Copyright (C) 2002-2004 Cosmin Truta.
 * Permission to use and distribute freely.
 * No warranty.
 **/

#ifndef SYSCONF_H
#define SYSCONF_H


 /*****************************************************************************/
 /* Platform identifiers */


 /* Detect Unix. */
#if defined(unix) || defined(__linux__) || defined(BSD) || defined(__CYGWIN__)
  /* Add more systems here. */
# ifndef UNIX
#  define UNIX
# endif
#endif

/* Detect MS-DOS. */
#if defined(__MSDOS__)
# ifndef MSDOS
#  define MSDOS
# endif
#endif

/* TO DO: Detect OS/2. */

/* Detect Windows. */
#if defined(_WIN32) || defined(__WIN32__)
# ifndef WIN32
#  define WIN32
# endif
#endif
#if defined(_WIN64)
# ifndef WIN64
#  define WIN64
# endif
#endif
#if defined(_WINDOWS) || defined(WIN32) || defined(WIN64)
# ifndef WINDOWS
#  define WINDOWS
# endif
#endif

/* Enable POSIX-friendly symbols on Microsoft (Visual) C. */
#ifdef _MSC_VER
# define _POSIX_
#endif


/*****************************************************************************/
/* Library access */


#if defined(UNIX)
# include <unistd.h>
#endif

#if defined(_POSIX_VERSION)
# include <fcntl.h>
# ifndef HAVE_ISATTY
#  define HAVE_ISATTY
# endif
#endif

#if defined(MSDOS) || defined(OS2) || defined(WINDOWS) || defined(__CYGWIN__)
  /* Add more systems here, e.g. MacOS 9 and earlier. */
# include <fcntl.h>
# include <io.h>
# ifndef HAVE_ISATTY
#  define HAVE_ISATTY
# endif
# ifndef HAVE_SETMODE
#  define HAVE_SETMODE
# endif
#endif

/* Standard I/O handles. */
#define STDIN  0
#define STDOUT 1
#define STDERR 2

/* Provide a placeholder for O_BINARY, if it doesn't exist. */
#ifndef O_BINARY
# define O_BINARY 0
#endif


#endif  /* SYSCONF_H */