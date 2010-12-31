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
/** @file sal_pthread_mutex.c
    @brief pthreads mutex implementation (Linux-only)
    @remarks SAL uses pthreads for Linux and OS X, however it only uses
    pthread mutices on Linux since Linux provides recursive mutices and
    OS X does not.  For locking we use NSRecursiveMutex on OS X.
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "../sal.h"

#if defined POSH_OS_LINUX || defined POSH_OS_CYGWIN32 || defined SAL_DOXYGEN

#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

/** @brief pthreads implementation of _SAL_create_mutex() 
    @ingroup pthreads */
sal_error_e
_SAL_create_mutex_pthreads( SAL_Device *device, sal_mutex_t *p_mtx )
{
    if ( p_mtx == 0 || device == 0 )
    {
        _SAL_warning( device, "Invalid parameters to SAL_create_mutex\n" );
        return SALERR_INVALIDPARAM;
    }

    *p_mtx = ( sal_mutex_t * ) device->device_callbacks.alloc( sizeof( pthread_mutex_t ) );

    if ( *p_mtx == 0 )
    {
        _SAL_error( device, "Out of memory allocating mutex\n" );
        return SALERR_OUTOFMEMORY;
    }

#if defined POSH_OS_LINUX 
    {
        pthread_mutexattr_t attr;

        attr.__mutexkind = PTHREAD_MUTEX_RECURSIVE_NP;

        pthread_mutex_init( (pthread_mutex_t * ) (*p_mtx), &attr );
    }
#else
    pthread_mutex_init( (pthread_mutex_t * ) (*p_mtx), NULL );
#endif

    return SALERR_OK;
}

/** @brief pthreads implementation of _SAL_lock_mutex()
    @ingroup pthreads */
sal_error_e
_SAL_lock_mutex_pthreads( SAL_Device *device, sal_mutex_t mutex )
{
	if ( mutex == 0 || device == 0 )
	{
		return SALERR_INVALIDPARAM;
	}
	
	pthread_mutex_lock( ( pthread_mutex_t * ) mutex );
	
	return SALERR_OK;

}

/** @brief pthreads implementation of _SAL_unlock_mutex()
    @ingroup pthreads */
sal_error_e
_SAL_unlock_mutex_pthreads( SAL_Device *device, sal_mutex_t mutex )
{
	if ( mutex == 0 || device == 0 )
	{
		return SALERR_INVALIDPARAM;
	}
	
	pthread_mutex_unlock( ( pthread_mutex_t * ) mutex );
	
	return SALERR_OK;
}

/** @brief pthreads implementation of _SAL_destroy_mutex() 
    @ingroup pthreads */
sal_error_e
_SAL_destroy_mutex_pthreads( SAL_Device *device, sal_mutex_t mutex )
{
    if ( mutex == 0 || device == 0 )
    {
        _SAL_warning( device, "Invalid parameters to SAL_destroy_mutex\n" );
        return SALERR_INVALIDPARAM;
    }

    pthread_mutex_destroy( ( pthread_mutex_t * ) mutex );

    return SALERR_OK;
}

#endif
