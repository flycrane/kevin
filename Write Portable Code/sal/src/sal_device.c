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
/** @file sal_device.c
    @brief Simple Audio File core functions
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "sal.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static
void *
s_alloc( sal_u32_t sz )
{
    return malloc( sz );
}

static
void
s_free( void *p )
{
    free( p );
}

static
void
s_error( const char *kp_msg )
{
    fprintf( stderr, kp_msg );
    fflush( stderr );
    exit( 1 );
}

static
void
s_print( const char *kp_msg )
{
    fprintf( stderr, kp_msg );
    fflush( stderr );
}

/** @defgroup DeviceManagement Device Management
    @{ 
*/

/** @brief Create an audio device with the given attributes.
    @param[out] pp_device pp_device pointer to pointer of a SAL_Device
    @param[in] kp_cb pointer to callbacks structure, may be NULL
    @param[in] kp_sp pointer to system parameters structure, may NOT be NULL
    @param[in] desired_channels  number of desired output channels, 
    may be 0 if you want it to use default preferences
    @param[in] desired_bits number of bits per sample, specify 0 for system default
    @param[in] desired_sample_rate desired sample rate, in samples/second, specify 0 for system default
    @param[in] num_voices desired number of simultaneous voices that can be played.
    @retval SALERR_OK on success, @ref sal_error_e otherwise
*/
sal_error_e
SAL_create_device( SAL_Device **pp_device,
                   const SAL_Callbacks *kp_cb,
                   const SAL_SystemParameters *kp_sp,
                   sal_u32_t desired_channels,
                   sal_u32_t desired_bits,
                   sal_u32_t desired_sample_rate,
                   sal_u32_t num_voices )
{
    sal_error_e err;
    SAL_Device *p_device = 0;

    if ( pp_device == 0 || kp_sp == 0 || num_voices <= 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    if ( kp_cb == 0)
    {
        p_device = ( struct SAL_Device_s * ) malloc( sizeof( struct SAL_Device_s ) );
        memset( p_device, 0, sizeof( *p_device ) );

        p_device->device_callbacks.alloc   = s_alloc;
        p_device->device_callbacks.free    = s_free;
        p_device->device_callbacks.warning = s_print;
        p_device->device_callbacks.error   = s_error;
    }
    else
    {
        if ( kp_cb->cb_size != sizeof( SAL_Callbacks ) )
        {
            return SALERR_WRONGVERSION;
        }

        if ( ( kp_cb->alloc == 0 ) != ( kp_cb->free == 0 ) )
        {
            return SALERR_INVALIDPARAM;
        }

        p_device = ( struct SAL_Device_s * ) kp_cb->alloc( sizeof( struct SAL_Device_s ) );
        memset( p_device, 0, sizeof( *p_device ) );
        p_device->device_callbacks = *kp_cb;
    }

    /* allocate voices */
    p_device->device_voices     = ( struct SAL_Voice_s * ) p_device->device_callbacks.alloc( sizeof( struct SAL_Voice_s ) * num_voices );
    memset( p_device->device_voices, 0, sizeof( struct SAL_Voice_s ) * num_voices );
    p_device->device_max_voices = num_voices;

    if ( ( err = _SAL_create_device_data( p_device, kp_sp, desired_channels, desired_bits, desired_sample_rate ) ) != SALERR_OK )
    {
        p_device->device_callbacks.free( p_device->device_voices );
        p_device->device_callbacks.free( p_device );
        return err;
    }

    _SAL_create_mutex( p_device, &(p_device->device_mutex));

    p_device->device_info.di_bytes_per_sample = p_device->device_info.di_bits / 8; 
    p_device->device_info.di_bytes_per_frame  = p_device->device_info.di_bytes_per_sample * p_device->device_info.di_channels;

    *pp_device = p_device;

    return SALERR_OK;
}

/** @brief Destroys a device previous created with SAL_create_device().
    @param[in] p_device pointer to device to destroy
*/
sal_error_e
SAL_destroy_device( SAL_Device *p_device )
{
    sal_voice_t v;
    if ( p_device == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    /* stop all sounds */
    for ( v = 0; v < p_device->device_max_voices; v++ )
    {
        SAL_stop_voice( p_device, v );
    }
	
    p_device->device_fnc_destroy( p_device );
	
    if ( p_device->device_mutex )
    {
        _SAL_destroy_mutex( p_device, p_device->device_mutex );
        p_device->device_mutex = 0;
    }

    p_device->device_callbacks.free( p_device->device_voices );
    p_device->device_callbacks.free( p_device );

    return SALERR_OK;
}


/** @internal
    @brief Locks access to the specified device.
    @param[in] device device to lock access to
    @returns SALERR_OK on success, @ref sal_error_e otherwise
    The underlying mutex implementation is expected to be recursive, that is
    the same thread may lock a mutex multiple times.  Unlocking the device
    is achieved by calling _SAL_unlock_device().
*/
sal_error_e
_SAL_lock_device( SAL_Device *device )
{
    if ( device == 0 || device->device_mutex == 0 )
    {
        return SALERR_INVALIDPARAM;
    }
	
    if ( _SAL_lock_mutex( device, device->device_mutex ) != SALERR_OK )
    {
        device->device_callbacks.error( "Could not lock mutex!" );
        return SALERR_SYSTEMFAILURE;
    }
	
    return SALERR_OK;
}

/** @internal
    @brief Unlocks access to the specified device.
    @param[in] device device to unlock access to
    @returns SALERR_OK on success, @ref sal_error_e otherwise
    The underlying mutex implementation is expected to be recursive, that is
    the same thread may lock a mutex multiple times.  Locking the device
    is achieved by calling _SAL_lock_device().
*/
sal_error_e
_SAL_unlock_device( SAL_Device *device )
{
    if ( device == 0 || device->device_mutex == 0 )
    {
        return SALERR_INVALIDPARAM;
    }
	
    if ( _SAL_unlock_mutex( device, device->device_mutex ) != SALERR_OK )
    {
        device->device_callbacks.error( "Could not unlock mutex!" );
        return SALERR_SYSTEMFAILURE;
    }
	
    return SALERR_OK;
}

/** Retrieves information about the device.
    @param[in] p_device pointer to the device to query
    @param[in,out] p_info pointer to a SAL_DeviceInfo struct.  Note that its di_size structure
    <i>must</i> be filled in appropriately or a SALERR_WRONGVERSION will be
    returned!!
    @retval SALERR_OK on success
    @retval SALERR_WRONGVERSION if the SAL_DeviceInfo's di_size member is not the expected value
    @remarks Here is an example of proper usage:
    @code
    SAL_DeviceInfo dinfo;

    memset( &dinfo, 0, sizeof( dinfo ) );
    dinfo.di_size = sizeof( dinfo );
    if ( SAL_get_device_info( device, &dinfo ) == SALERR_OK )
    {
    printf( "Device's name = %s\n", dinfo.di_name );
    }
    @endcode
*/
sal_error_e 
SAL_get_device_info( SAL_Device *p_device,
                     SAL_DeviceInfo *p_info )
{
    if ( p_device == 0 || p_info == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    if ( p_info->di_size != sizeof( SAL_DeviceInfo ) )
    {
        return SALERR_WRONGVERSION;
    }

    _SAL_lock_device( p_device );

    *p_info = p_device->device_info;

    _SAL_unlock_device( p_device );

    return SALERR_OK;
}

/** @} */
