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
/** @file sal_osx.c
    @brief OS X implementation
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "../sal.h"

#if defined POSH_OS_OSX || defined SAL_DOXYGEN

#include <unistd.h>

extern sal_error_e _SAL_create_thread_pthreads( SAL_Device *device, SAL_THREAD_FUNC fnc, void *args );
extern sal_error_e _SAL_create_mutex_osx( SAL_Device *device, sal_mutex_t *p_mutex );
extern sal_error_e _SAL_lock_mutex_osx( SAL_Device *device, sal_mutex_t mtx );
extern sal_error_e _SAL_destroy_mutex_osx( SAL_Device *device, sal_mutex_t mtx );
extern sal_error_e _SAL_unlock_mutex_osx( SAL_Device *device, sal_mutex_t mtx );

static 
sal_error_e
_SAL_sleep_osx( SAL_Device *device, sal_u32_t duration )
{
    usleep( duration * 1000 );

    return SALERR_OK;
}

extern
sal_error_e
_SAL_create_device_data_coreaudio( SAL_Device *device,
                                   const SAL_SystemParameters *kp_sp,
                                   sal_u32_t desired_channels,
                                   sal_u32_t desired_bits,
                                   sal_u32_t desired_sample_rate );

sal_error_e
_SAL_create_device_data( SAL_Device *device,
			             const SAL_SystemParameters *kp_sp,
			             sal_u32_t desired_channels,
			             sal_u32_t desired_bits,
			             sal_u32_t desired_sample_rate )
{
    device->device_fnc_sleep          = _SAL_sleep_osx;
    device->device_fnc_create_thread  = _SAL_create_thread_pthreads;
    device->device_fnc_create_mutex   = _SAL_create_mutex_osx;
    device->device_fnc_destroy_mutex  = _SAL_destroy_mutex_osx;
    device->device_fnc_lock_mutex     = _SAL_lock_mutex_osx;
    device->device_fnc_unlock_mutex   = _SAL_unlock_mutex_osx;

    return _SAL_create_device_data_coreaudio( device, kp_sp, desired_channels, desired_bits, desired_sample_rate );
}

#endif /* POSH_OS_OSX */
