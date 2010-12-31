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
/** @file sal_nsrecursivelock.m
    @brief NSRecursiveLock Obj-C implementation for SAL
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "../sal.h"

#if defined POSH_OS_OSX || defined SAL_DOXYGEN

#include <Foundation/NSLock.h>

/** @internal 
    @brief OS X implementation of mutex
    @ingroup osx */
sal_error_e
_SAL_create_mutex_osx( SAL_Device *device, sal_mutex_t *p_mutex )
{
	if ( device == 0 || p_mutex == 0 )
	{
		return SALERR_INVALIDPARAM;
	}
	
	*p_mutex = [NSRecursiveLock new];
	
	return SALERR_OK;
}

/** @internal
    @brief OS X implementation of _SAL_lock_mutex()
    @ingroup osx */
sal_error_e
_SAL_lock_mutex_osx( SAL_Device *device, sal_mutex_t mtx )
{
	NSRecursiveLock *rl = ( NSRecursiveLock * ) mtx;
	
	if ( device == 0 || mtx == 0 )
	{
		return SALERR_INVALIDPARAM;
	}
	
	[rl lock];
	
	return SALERR_OK;
}

/** @internal
    @brief OS X implementation of _SAL_destroy_mutex()
    @ingroup osx */
sal_error_e
_SAL_destroy_mutex_osx( SAL_Device *device, sal_mutex_t mtx )
{
	NSRecursiveLock *rl = ( NSRecursiveLock * ) mtx;
	
	if ( device == 0 || mtx == 0 )
	{
		return SALERR_INVALIDPARAM;
	}
	
	[rl release];
	
	return SALERR_OK;
}

/** @internal
    @brief OS X implementation of _SAL_unlock_mutex()
    @ingroup osx */
sal_error_e
_SAL_unlock_mutex_osx( SAL_Device *device, sal_mutex_t mtx )
{
	NSRecursiveLock *rl = ( NSRecursiveLock * ) mtx;
	
	if ( device == 0 || mtx == 0 )
	{
		return SALERR_INVALIDPARAM;
	}
	
	[rl unlock];
	
	return SALERR_OK;
}

#endif /* POSH_OS_OSX */

