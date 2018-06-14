/* Copyright (c) 2016, Pollard Banknote Limited
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** Provide types, macros, and functions for the operating system
 *
 * This file provides a stable way to work with the OS. It provides
 *   - namespaces and typedefs that can be used portably between OS's
 *   - feature macros for enabling specific operating system capabilities
 *
 * @todo May want to rethink OS_POSIX as feature test macros since posix can
 * sorta be in Windows (ex., Cygwin)
 */
#ifndef PBL_CONFIG_OS_H
#define PBL_CONFIG_OS_H

// =============================================================================
// Determine the platform. Ex., Windows or POSIX. Define feature macros

#if ( defined( __unix__ ) || defined( __unix ) || ( defined( __APPLE__ ) && defined( __MACH__ )  ) )
#include <unistd.h>

#ifdef _POSIX_VERSION
// ISO POSIX.1 aka IEEE 1003.1

/// OS_POSIX - unistd.h is included and _POSIX_VERSION is defined
#define OS_POSIX

#if _POSIX_VERSION >= 199506L
// ISO POSIX.1-1996 aka IEEE 1003.1-1996
#define POSIX_THREADS

#if ( _POSIX_VERSION >= 200112L )
// ISO POSIX.1-2001 aka IEEE 1003.1-2001, aka Issue 6, aka SUSv3
#define POSIX_ISSUE_6

#if ( _POSIX_VERSION >= 200809L )
// ISO POSIX.1-2008, aka IEEE 1003.1-2008, aka Issue 7, aka SUSv4
#define POSIX_ISSUE_7
#endif // 2008
#endif // 2001
#endif // 1996
#endif // ifdef _POSIX_VERSION
#endif // if ( defined( __unix__ ) || defined( __unix ) || ( defined( __APPLE__ ) && defined( __MACH__ )))

#if defined( _WIN64 ) || defined( _WIN32 )
#include <windows.h> /* MS Windows operating systems */
#define OS_WINDOWS
#endif

// =============================================================================
// Determine basic types

#ifdef OS_POSIX
#include <sys/types.h>
namespace pbl
{
namespace os
{
// Identifies a user
typedef uid_t user_id_type;

// Identifies a process
typedef pid_t pid_type;
}
}
#else
#ifdef OS_WINDOWS
namespace pbl
{
namespace os
{
typedef DWORD pid_type;
}
}
#endif // Windows
#endif // ifdef OS_POSIX

// =============================================================================
// Determine multithreading types

#ifdef POSIX_THREADS
#include <pthread.h>
namespace pbl
{
namespace os
{
typedef pthread_t thread_type;
typedef pthread_mutex_t mutex_type;
typedef pthread_rwlock_t shared_mutex_type;
typedef pthread_cond_t condition_variable_type;
}
}
#else
#ifdef OS_WINDOWS
namespace pbl
{
namespace os
{
typedef HANDLE thread_type;
typedef CRITICAL_SECTION mutex_type;
}
}
#endif
#endif // ifdef POSIX_THREADS

#endif // PBL_CONFIG_OS_H
