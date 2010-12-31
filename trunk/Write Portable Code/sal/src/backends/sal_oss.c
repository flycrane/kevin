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
/** @file sal_oss.c
    @brief Linux/OSS SAL routines
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "../sal.h"

#ifdef SAL_DOXYGEN
#  define SAL_SUPPORT_OSS /**< If defined, OSS support will be compiled in */
#endif

#if ( ( defined POSH_OS_LINUX || defined POSH_OS_CYGWIN32 ) && defined SAL_SUPPORT_OSS ) || defined SAL_DOXYGEN

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>

/** @internal
    @brief private device data for the OSS subsystem */
typedef struct SAL_OSSData
{
    int oss_fd;                 /**< file descriptor for OSS */
    int oss_kill_audio_thread;  /**< set to 1 when the thread should die */
    sal_byte_t *oss_mix_buffer;      /**< mixing buffer */
    int         oss_mix_buffer_size; /**< size of the mix buffer, in bytes */
    int         oss_buffer_length_ms; /**< length of the mix buffer, in millseconds */
} SAL_OSSData;

static void destroy_device_data_oss( SAL_Device *device );

static
void
s_oss_audio_thread( void *args )
{
    SAL_Device *device = ( SAL_Device *) args;
    SAL_OSSData *ossd = ( SAL_OSSData * ) device->device_data;
    int bytes_to_fill = 0;
    audio_buf_info info;

    while ( 1 )
    {
        _SAL_lock_device( device );
       
        /* time to quit? */
        if ( ossd->oss_kill_audio_thread )
        {
            ossd->oss_kill_audio_thread = 0;
            _SAL_unlock_device( device );
            return;
        }
       
        /* determine how much space we have to fill */
        ioctl( ossd->oss_fd, SNDCTL_DSP_GETOSPACE, &info );
       
        bytes_to_fill = ( info.bytes > ossd->oss_mix_buffer_size ) ? ossd->oss_mix_buffer_size : info.bytes;
       
        if ( bytes_to_fill )
        {
            _SAL_mix_chunk( device, ossd->oss_mix_buffer, bytes_to_fill );
            write( ossd->oss_fd, ossd->oss_mix_buffer, bytes_to_fill );
        }
       
        _SAL_unlock_device( device );
       
        SAL_sleep( device, 10 );
    }
}

/** @internal
    @brief OSS specific device creation function
    @param[in] device pointer to output device
    @param[in] kp_sp pointer to system parameters structure, may NOT be NULL
    @param[in] desired_channels  number of desired output channels, 
    may be 0 if you want it to use default preferences
    @param[in] desired_bits number of bits per sample, specify 0 for system default
    @param[in] desired_sample_rate desired sample rate, in samples/second, specify 0 for system default
*/
sal_error_e
_SAL_create_device_data_oss( SAL_Device *device, 
                             const SAL_SystemParameters *kp_sp,
                             sal_u32_t desired_channels, 
                             sal_u32_t desired_bits, 
                             sal_u32_t desired_sample_rate )
{
    SAL_OSSData *ossd = 0;
    const char *device_name = "/dev/dsp";

    desired_channels    = ( desired_channels == 0 ) ? DEFAULT_AUDIO_CHANNELS : desired_channels;
    desired_bits        = ( desired_bits == 0 ) ? DEFAULT_AUDIO_BITS : desired_bits;
    desired_sample_rate = ( desired_sample_rate == 0 ) ? DEFAULT_AUDIO_SAMPLE_RATE: desired_sample_rate;
    
    if ( device == 0 || device_name == 0 || kp_sp == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    device->device_fnc_destroy = destroy_device_data_oss;
    
    ossd = ( SAL_OSSData * ) device->device_callbacks.alloc( sizeof( *ossd ) );

    memset( ossd, 0, sizeof( *ossd ) );

    ossd->oss_buffer_length_ms = ( kp_sp->sp_buffer_length_ms == 0 ) ? DEFAULT_BUFFER_DURATION : kp_sp->sp_buffer_length_ms;

    /* open appropriate device */
    if ( ( ossd->oss_fd = open( device_name, O_WRONLY, 0 ) ) == -1 )
    {
        _SAL_warning( device, "Could not open %s\n", device_name );
        return SALERR_SYSTEMFAILURE;
    }

    /* set format */
        {
            int format = ( int ) ( ( desired_bits == 8 ) ? AFMT_U8 : AFMT_S16_NE );
            int desired_oss_format = format;
            int channels = ( int ) desired_channels;
            int speed = ( int ) desired_sample_rate;
    
            if ( ( ioctl( ossd->oss_fd, SNDCTL_DSP_CHANNELS, &channels ) == -1 ) || ( channels != desired_channels ) )
            {
                _SAL_warning( device, "Could not set desired number of channels (%d)\n", channels );
                close( ossd->oss_fd );
                device->device_callbacks.free( ossd );
                return SALERR_SYSTEMFAILURE;
            }

            if ( ( ioctl( ossd->oss_fd, SNDCTL_DSP_SETFMT, &format) == -1 ) || ( desired_oss_format != format ) )
            {
                _SAL_warning( device, "Could not set desired sound format (%d-bits)\n", desired_bits );
                close( ossd->oss_fd );
                device->device_callbacks.free( ossd );
                return SALERR_SYSTEMFAILURE;
            }

            if ( ( ioctl( ossd->oss_fd, SNDCTL_DSP_SPEED, &speed ) == -1) || ( speed != desired_sample_rate ) )
            {
                _SAL_warning( device, "Could not set desired sampling speed (%d)\n", speed );
                close( ossd->oss_fd );
                device->device_callbacks.free( ossd );
                return SALERR_SYSTEMFAILURE;
            }
        }

    /* create mixing buffer */
    ossd->oss_mix_buffer_size = ( desired_bits / 8 ) * desired_sample_rate * desired_channels * ossd->oss_buffer_length_ms / 1000;

    /* forcibly align on 32-bit boundary */
    ossd->oss_mix_buffer_size &= ~(( desired_bits * desired_channels / 8 )-1);
    ossd->oss_mix_buffer = (sal_byte_t *) device->device_callbacks.alloc( ossd->oss_mix_buffer_size );

    /* save out parameters */
    device->device_info.di_size        = sizeof( device->device_info );
    device->device_info.di_channels    = desired_channels;
    device->device_info.di_bits        = desired_bits;
    device->device_info.di_sample_rate = desired_sample_rate;
    device->device_data = ossd;
    strncpy( device->device_info.di_name, "OSS", sizeof( device->device_info.di_name ) );

    /* kick off audio thread */
    _SAL_create_thread( device, s_oss_audio_thread, device );

    return SALERR_OK;
}

static
void
destroy_device_data_oss( SAL_Device *device )
{
    SAL_OSSData *ossd = 0;

    if ( device == 0 || device->device_data == 0 )
        return;

    _SAL_lock_device( device );

    ossd = ( SAL_OSSData * ) ( device->device_data );

    ossd->oss_kill_audio_thread = 1;

    _SAL_unlock_device( device );

    SAL_sleep( device, 1000 );

    /* close the file descriptor */
    close( ossd->oss_fd );

    /* free memory */
    if ( ossd->oss_mix_buffer )
    {
        device->device_callbacks.free( ossd->oss_mix_buffer );
    }
    device->device_callbacks.free( ossd );
    device->device_data = 0;
}

#endif /* POSH_OS_LINUX */
