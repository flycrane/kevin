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
/** @file sal_alsa.c
    @brief Linux/ALSA SAL routines
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "../sal.h"

/** @define SAL_SUPPORT_ALSA
    If defined, support for the ALSA sound API under Linux is compiled in
*/
#if ( defined POSH_OS_LINUX && defined SAL_SUPPORT_ALSA ) || defined SAL_DOXYGEN

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

/** ALSA subsystem device specific data 
 */
typedef struct SAL_ALSAData
{
    snd_pcm_t           *alsad_playback_handle;           /**< PCM playback handle */
    sal_byte_t          *alsad_mix_buffer;                /**< mixing buffer*/
    int                  alsad_mix_buffer_size_bytes;     /**< mixing buffer size in bytes */
    int                  alsad_mix_buffer_length_ms;      /**< approximate duration of the buffer in milliseconds */
    int                  alsad_kill_audio_thread;         /**< set to 1 when the audio thread should be killed */
} SAL_ALSAData;

static void destroy_device_data_alsa( SAL_Device *device );

static 
void 
s_alsa_audio_thread( void *args )
{
    SAL_Device *device = ( SAL_Device * ) args;
    SAL_ALSAData *alsad = ( SAL_ALSAData * ) device->device_data;
    int frames_to_deliver, bytes_to_fill;
    int sleep_time = alsad->alsad_mix_buffer_length_ms / 2;
    
    while ( 1 )
    {
        /* lock the device */
        _SAL_lock_device( device );
        
        /* time to quit? */
        if ( alsad->alsad_kill_audio_thread )
        {
            alsad->alsad_kill_audio_thread = 0;
            _SAL_unlock_device( device );
            return;
        }
        
        frames_to_deliver = snd_pcm_avail_update( alsad->alsad_playback_handle );
        
        bytes_to_fill = frames_to_deliver * device->device_info.di_bytes_per_frame;
        bytes_to_fill = bytes_to_fill > alsad->alsad_mix_buffer_size_bytes ? alsad->alsad_mix_buffer_size_bytes : bytes_to_fill;
        
        if ( bytes_to_fill > 0 )
        {
            _SAL_mix_chunk( device, alsad->alsad_mix_buffer, bytes_to_fill );
            snd_pcm_writei( alsad->alsad_playback_handle, alsad->alsad_mix_buffer, frames_to_deliver );
        }
        
        _SAL_unlock_device( device );
        
        SAL_sleep( device, sleep_time );
    }
}

/** @internal
    @brief ALSA specific device creation function
    @param[in] device pointer to output device
    @param[in] kp_sp pointer to system parameters structure, may NOT be NULL
    @param[in] desired_channels  number of desired output channels, 
    may be 0 if you want it to use default preferences
    @param[in] desired_bits number of bits per sample, specify 0 for system default
    @param[in] desired_sample_rate desired sample rate, in samples/second, specify 0 for system default
*/
sal_error_e
_SAL_create_device_data_alsa( SAL_Device *device, 
                              const SAL_SystemParameters *kp_sp,
                              sal_u32_t desired_channels, 
                              sal_u32_t desired_bits, 
                              sal_u32_t desired_sample_rate )
{
    SAL_ALSAData *alsad = 0;
    snd_pcm_hw_params_t *hw_params;
    const char *device_name = "plughw:0,0";
    snd_pcm_format_t format;

    desired_channels    = ( desired_channels == 0 ) ? DEFAULT_AUDIO_CHANNELS : desired_channels;
    desired_bits        = ( desired_bits == 0 ) ? DEFAULT_AUDIO_BITS : desired_bits;
    desired_sample_rate = ( desired_sample_rate == 0 ) ? DEFAULT_AUDIO_SAMPLE_RATE: desired_sample_rate;
    
    if ( device == 0 || device_name == 0 || kp_sp == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    format = ( desired_bits == 8 ) ? SND_PCM_FORMAT_U8 : SND_PCM_FORMAT_S16;

    device->device_fnc_destroy = destroy_device_data_alsa;
    
    alsad = ( SAL_ALSAData * ) device->device_callbacks.alloc( sizeof( *alsad ) );

    memset( alsad, 0, sizeof( *alsad ) );

    /* allocate playback handle */
    if ( snd_pcm_open( &alsad->alsad_playback_handle, device_name, SND_PCM_STREAM_PLAYBACK, 0) < 0 )
    {
        device->device_callbacks.free( alsad );
        _SAL_warning( device, "Could not open audio device\n" );
        return SALERR_SYSTEMFAILURE;
    }

    /* allocate hw params */
    if ( snd_pcm_hw_params_malloc( &hw_params) < 0 )
    {
        device->device_callbacks.free( alsad );
        _SAL_warning( device, "Could not allocate hw_params" );
        return SALERR_SYSTEMFAILURE;
    }

    /* any parameters */
    if ( snd_pcm_hw_params_any( alsad->alsad_playback_handle, hw_params ) < 0 )
    {
        snd_pcm_hw_params_free( hw_params );
        device->device_callbacks.free( alsad );
        _SAL_warning( device, "Failed call to snd_pcm_hw_params_any" );
        return SALERR_SYSTEMFAILURE;
    }

    /* set interleaved format */
    if ( snd_pcm_hw_params_set_access( alsad->alsad_playback_handle, hw_params,SND_PCM_ACCESS_RW_INTERLEAVED ) < 0 )
    {
        snd_pcm_hw_params_free( hw_params );
        device->device_callbacks.free( alsad );
        _SAL_warning( device, "Failed to set interleaved format\n" );
        return SALERR_SYSTEMFAILURE;
    }

    /* set the bit-depth format */
    if ( snd_pcm_hw_params_set_format( alsad->alsad_playback_handle, hw_params, format ) < 0 )
    {
        snd_pcm_hw_params_free( hw_params );
        device->device_callbacks.free( alsad );
        _SAL_warning( device, "Could not set format" );
        return SALERR_SYSTEMFAILURE;
    }

    /* set number of channels */
    if ( snd_pcm_hw_params_set_channels( alsad->alsad_playback_handle, hw_params, desired_channels ) < 0 )
    {
        snd_pcm_hw_params_free( hw_params );
        device->device_callbacks.free( alsad );
        _SAL_warning( device, "Could not set number of channels" );
        return SALERR_SYSTEMFAILURE;
    }

    /* set the rate */
        {
            if ( snd_pcm_hw_params_set_rate( alsad->alsad_playback_handle, hw_params, desired_sample_rate, 0 ) < 0 )
            {
                snd_pcm_hw_params_free( hw_params );
                device->device_callbacks.free( alsad );
                _SAL_warning( device, "Could not set number of channels" );
                return SALERR_SYSTEMFAILURE;
            }
        }

    /* set the params */
    if ( snd_pcm_hw_params( alsad->alsad_playback_handle, hw_params ) < 0 )
    {
        snd_pcm_hw_params_free( hw_params );
        device->device_callbacks.free( alsad );
        _SAL_warning( device, "Could not set hardware parameters" );
        return SALERR_SYSTEMFAILURE;
    }

    /* do a quick query about the buffer */
        {
            unsigned period_time, periods, buffer_time;
            snd_pcm_uframes_t buffer_frames;
            int dir = 0;

            snd_pcm_hw_params_get_period_time( hw_params, &period_time, &dir );
            snd_pcm_hw_params_get_periods( hw_params, &periods, &dir );
            snd_pcm_hw_params_get_buffer_size( hw_params, &buffer_frames );
            snd_pcm_hw_params_get_buffer_time( hw_params, &buffer_time, &dir );

            alsad->alsad_mix_buffer_length_ms = buffer_time / 1000;
            alsad->alsad_mix_buffer_size_bytes = buffer_frames * desired_channels * desired_bits / 8;
            alsad->alsad_mix_buffer = (sal_byte_t*) device->device_callbacks.alloc( alsad->alsad_mix_buffer_size_bytes );
            memset( alsad->alsad_mix_buffer, 0, alsad->alsad_mix_buffer_size_bytes );
        }

    /* free hw params */
    snd_pcm_hw_params_free( hw_params );

    /* prepare handle */
    if ( snd_pcm_prepare( alsad->alsad_playback_handle ) < 0 )
    {
        device->device_callbacks.free( alsad );
        _SAL_warning( device, "Could not prepare playback handle" );
        return SALERR_SYSTEMFAILURE;
    }

    /* store parameters */
    device->device_info.di_size        = sizeof( device->device_info );
    device->device_info.di_channels    = desired_channels;
    device->device_info.di_bits        = desired_bits;
    device->device_info.di_sample_rate = desired_sample_rate;
    device->device_data = alsad;
    strncpy( device->device_info.di_name, "ALSA", sizeof( device->device_info.di_name ) );

    /* kick off audio thread */
    _SAL_create_thread( device, s_alsa_audio_thread, device );

    return SALERR_OK;
}

static 
void 
destroy_device_data_alsa( SAL_Device *device )
{
    SAL_ALSAData *alsad = 0;

    if ( device == 0 || device->device_data == 0 )
        return;

    _SAL_lock_device( device );

    alsad = ( SAL_ALSAData * ) ( device->device_data );

    alsad->alsad_kill_audio_thread = 1;

    _SAL_unlock_device( device );

    SAL_sleep( device, 1000 );

    /* free the handle */
    snd_pcm_close( alsad->alsad_playback_handle );

    /* free memory */
    device->device_callbacks.free( device->device_data );
    device->device_data = 0;
}

#endif /* POSH_OS_LINUX */
