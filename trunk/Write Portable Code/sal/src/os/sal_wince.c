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
/** 
    @file sal_wince.c
    @author Brian Hook
    @date    2004
    @brief Windows CE/PocketPC operating system support for the Simple Audio Library
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "../sal.h"

#if defined POSH_OS_WINCE || defined SAL_DOXYGEN

#include <windows.h>
#include <string.h>

extern
sal_error_e 
_SAL_create_device_data_waveout( SAL_Device *device, 
                                 const SAL_SystemParameters *kp_sp, 
                                 sal_u32_t desired_channels, 
                                 sal_u32_t desired_bits, 
                                 sal_u32_t desired_sample_rate );

/** @internal
    @brief Parameter structure for the WinCE thread bridge function 
*/
typedef struct _SAL_WinCEBridgeFunctionParameters_s
{
    SAL_THREAD_FUNC bfp_fnc;      /**< thread function */
	void           *bfp_targs;    /**< pointer to thread arguments */
} _SAL_WinCEBridgeFunctionParameters;

static 
DWORD 
WINAPI s_bridge_function( LPVOID lpParameter )
{
	_SAL_WinCEBridgeFunctionParameters *bfp = ( _SAL_WinCEBridgeFunctionParameters * ) lpParameter;

	bfp->bfp_fnc( bfp->bfp_targs );

	return 1;
}

static
sal_error_e
_SAL_create_thread_wince( SAL_Device *device, SAL_THREAD_FUNC fnc, void *targs )
{
    HANDLE hThread;
    _SAL_WinCEBridgeFunctionParameters bfp;

    bfp.bfp_fnc = fnc;
    bfp.bfp_targs = targs;

    if ( device == 0 || fnc == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    if ( ( hThread = CreateThread( NULL,                             /* lpThreadAttributes, ignored on WinCE */
		                           0,                                /* stack size, ignored on WinCE */
								   s_bridge_function,                /* thread start function */
								   &bfp,                             /* lpParameter */
                                   0,                                /* creation flags */
                                   NULL ) ) == (HANDLE)-1 )          /* pointer to thread ID */
    {
        return SALERR_SYSTEMFAILURE;
    }

    SetThreadPriority( hThread, THREAD_PRIORITY_HIGHEST );

    return SALERR_OK;
}

static
sal_error_e
_SAL_create_mutex_wince( SAL_Device *device, sal_mutex_t *p_mtx )
{
    if ( p_mtx == 0 || device == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    *p_mtx = CreateMutex( NULL,   /* security attributes */
                          FALSE,  /* initial owner */
                          NULL ); /* name */

    if ( *p_mtx == 0 )
    {
        return SALERR_SYSTEMFAILURE;
    }

    return SALERR_OK;
}

static
sal_error_e
_SAL_destroy_mutex_wince( SAL_Device *device, sal_mutex_t mtx )
{
    if ( mtx == 0 || device == 0 )
    {
        return SALERR_INVALIDPARAM;
    }
    
    if ( mtx )
    {
        CloseHandle( mtx );
    }
    
    return SALERR_OK;
}

static
sal_error_e
_SAL_lock_mutex_wince( SAL_Device *device, sal_mutex_t mtx )
{
    if ( mtx == 0 || device == 0 )
    {
        return SALERR_INVALIDPARAM;
    }
    
    if ( WaitForSingleObject( mtx, INFINITE ) == WAIT_OBJECT_0 )
    {
        return SALERR_OK;
    }
    
    return SALERR_SYSTEMFAILURE;
}

static
sal_error_e
_SAL_unlock_mutex_wince( SAL_Device *device, sal_mutex_t mtx )
{
    if ( mtx == 0 || device == 0 )
    {
        return SALERR_INVALIDPARAM;
    }
    
    if ( ReleaseMutex( mtx ) == FALSE )
    {
        return SALERR_SYSTEMFAILURE;
    }
    
    return SALERR_OK;
}

static
sal_error_e
_SAL_sleep_wince( SAL_Device *device, sal_u32_t duration )
{
    Sleep( duration );

    return SALERR_OK;
}

sal_error_e
_SAL_create_device_data( SAL_Device *device, const SAL_SystemParameters *kp_sp, sal_u32_t desired_channels, sal_u32_t desired_bits, sal_u32_t desired_sample_rate )
{
    device->device_fnc_create_thread = _SAL_create_thread_wince;
    device->device_fnc_create_mutex  = _SAL_create_mutex_wince;
    device->device_fnc_lock_mutex    = _SAL_lock_mutex_wince;
    device->device_fnc_unlock_mutex  = _SAL_unlock_mutex_wince;
    device->device_fnc_destroy_mutex = _SAL_destroy_mutex_wince;
    device->device_fnc_sleep         = _SAL_sleep_wince;

    if ( kp_sp->sp_flags & SAL_SPF_WAVEOUT )
    {
#ifdef SAL_SUPPORT_WAVEOUT
        return _SAL_create_device_data_waveout( device, kp_sp, desired_channels, desired_bits, desired_sample_rate );
#endif
    }
    else
    {
#ifdef SAL_SUPPORT_DIRECTSOUND
        return _SAL_create_device_data_dsound( device, kp_sp, desired_channels, desired_bits, desired_sample_rate );
#endif
    }
    return SALERR_UNIMPLEMENTED;
}

#endif /* POSH_OS_WINCE */
