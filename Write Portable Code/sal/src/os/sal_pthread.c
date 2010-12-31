/*
Copyright (c) 2004, Brian Hook
All rights reserved.

http://www.bookofhook.com/sal

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * The names of this package'ss contributors contributors may not
      be used to endorse or promote products derived from this
      software without specific prior written permission.


THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/** @file sal_pthread.c
    @brief pthreads implementation (Linux and OS X)
    @remarks SAL uses pthreads for Linux and OS X, however it only uses
    pthread mutices on Linux since Linux provides recursive mutexes and
    OS X does not.  For locking mutexes we use NSRecursiveMutex on OS X.
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "../sal.h"

#if defined POSH_OS_UNIX || defined POSH_OS_CYGWIN32 || defined POSH_OS_OSX || defined SAL_DOXYGEN

#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

/** @defgroup pthreads Pthreads Implementation functions
    @ingroup Implementations
*/

/** @brief pthreads implementation for _SAL_create_thread()  
    @ingroup pthreads */
sal_error_e
_SAL_create_thread_pthreads( SAL_Device *device, SAL_THREAD_FUNC fnc, void *args )
{
   pthread_attr_t attr;
   pthread_t tid;
   int result;

   if ( device == 0 || fnc == 0 || args == 0 )
   {
      return SALERR_INVALIDPARAM;
   }

   pthread_attr_init(&attr);

   result = pthread_create( &tid, &attr, (void* (*)(void *))fnc, args );

   if ( result != 0 )
   {
      return SALERR_SYSTEMFAILURE;
   }

   return SALERR_OK;
}

#endif
