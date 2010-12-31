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
    @file sal_waveout.c
    @author Brian Hook
    @date    2004
    @brief WAVEOUT subsystem (Windows-only)
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "../sal.h"

#if defined SAL_DOXYGEN
#  define SAL_SUPPORT_WAVEOUT /**< if defined, WAVEOUT support is compiled in */
#endif

#if ( defined POSH_OS_WIN32 && defined SAL_SUPPORT_WAVEOUT ) || defined SAL_DOXYGEN

#include <windows.h>
#include <stdio.h>

#define _SAL_WAVEOUT_NUM_BUFFERS 8 /**< number of output buffers */

/** @internal
    @ingroup Win32
    @brief private device data for the WAVEOUT subsystem */
typedef struct
{
    UINT     wod_device_id;              /**< device id */
    HWAVEOUT wod_hWaveOut;               /**< handle to WAVEOUT */
    HANDLE   wod_hEvent;                 /**< buffer done signaling event */

    WAVEHDR     wod_wave_header[ _SAL_WAVEOUT_NUM_BUFFERS ];  /**< WAVEHDR */
    sal_byte_t *wod_buffer[ _SAL_WAVEOUT_NUM_BUFFERS ] ;      /**< mix buffer */

    int wod_buffer_length_ms;    /**< length of individual buffers, in ms */
    int wod_buffer_size;         /**< size of individual buffers, in bytes */
    int wod_next_buffer;         /**< index to next buffer to fill */

    int wod_kill_audio_thread;   /**< set to 1 when the thread should terminate */
} SAL_WaveOutData;

static void destroy_device_waveout( SAL_Device *device );

static
void
s_audio_thread( void *args )
{
    SAL_Device *device = ( SAL_Device * ) args;
    SAL_WaveOutData *wod = ( SAL_WaveOutData * ) device->device_data;
    int sleep_duration;
    DWORD dwResult;

    sleep_duration = wod->wod_buffer_length_ms/2;

    while ( 1 )
    {
        int i;
        int count = 0;

        dwResult = WaitForSingleObject( wod->wod_hEvent, 1000 );

        /* grab lock */
        _SAL_lock_device( device );

        /* time to quit? */
        if ( wod->wod_kill_audio_thread )
        {
            wod->wod_kill_audio_thread = 0;
            _SAL_unlock_device( device );
            return;
        }

        /* fill next buffers */
        i = wod->wod_next_buffer;

        do
        {
            if ( wod->wod_wave_header[ i ].dwFlags & WHDR_DONE )
            {
                count++;

                _SAL_mix_chunk( device, wod->wod_buffer[ i ], wod->wod_buffer_size );
                waveOutWrite( wod->wod_hWaveOut, &wod->wod_wave_header[ i ], sizeof( wod->wod_wave_header[ 0 ] ) );

                i = ( i + 1 ) % _SAL_WAVEOUT_NUM_BUFFERS;
            }
            else
            {
                break;
            }
        } while ( i != wod->wod_next_buffer );

        /* update cursor */
        wod->wod_next_buffer = i;

        /* unlock access */
        _SAL_unlock_device( device );
    }
}

static
DWORD
params_to_format( sal_u32_t desired_channels,
                  sal_u32_t desired_bits,
                  sal_u32_t desired_sample_rate )
{
    if ( desired_sample_rate == 11025 )
    {
        if ( desired_channels == 1 )
        {
            if ( desired_bits == 8 )
            {
                return WAVE_FORMAT_1M08;
            }
            else if ( desired_bits == 16 )
            {
                return WAVE_FORMAT_1M16;
            }
            else
            {
                return 0;
            }
        }
        else if ( desired_channels == 2 )
        {
            if ( desired_bits == 8 )
            {
                return WAVE_FORMAT_1S08;
            }
            else if ( desired_bits == 16 )
            {
                return WAVE_FORMAT_1S16;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            return 0;
        }
    }
    else if ( desired_sample_rate == 22050 )
    {
        if ( desired_channels == 1 )
        {
            if ( desired_bits == 8 )
            {
                return WAVE_FORMAT_2M08;
            }
            else if ( desired_bits == 16 )
            {
                return WAVE_FORMAT_2M16;
            }
            else
            {
                return 0;
            }
        }
        else if ( desired_channels == 2 )
        {
            if ( desired_bits == 8 )
            {
                return WAVE_FORMAT_2S08;
            }
            else if ( desired_bits == 16 )
            {
                return WAVE_FORMAT_2S16;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            return 0;
        }
    }
    else if ( desired_sample_rate == 44100 )
    {
        if ( desired_channels == 1 )
        {
            if ( desired_bits == 8 )
            {
                return WAVE_FORMAT_4M08;
            }
            else if ( desired_bits == 16 )
            {
                return WAVE_FORMAT_4M16;
            }
            else
            {
                return 0;
            }
        }
        else if ( desired_channels == 2 )
        {
            if ( desired_bits == 8 )
            {
                return WAVE_FORMAT_4S08;
            }
            else if ( desired_bits == 16 )
            {
                return WAVE_FORMAT_4S16;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            return 0;
        }
    }
    return 0;
}

/** @internal
    @ingroup Win32
    @brief Creates device specific data for the WAVEOUT subsystem
    @param[in] device pointer to output device
    @param[in] kp_sp pointer to system parameters structure, may NOT be NULL
    @param[in] desired_channels  number of desired output channels, 
    may be 0 if you want it to use default preferences
    @param[in] desired_bits number of bits per sample, specify 0 for system default
    @param[in] desired_sample_rate desired sample rate, in samples/second, specify 0 for system default
*/
sal_error_e
_SAL_create_device_data_waveout( SAL_Device *device, 
                                 const SAL_SystemParameters *kp_sp, 
                                 sal_u32_t desired_channels, 
                                 sal_u32_t desired_bits, 
                                 sal_u32_t desired_sample_rate )
{
    WAVEFORMATEX wfx;
    MMRESULT mmr;
    SAL_WaveOutData *wod = 0;
    int buffer_length_ms;
    UINT num_devices;
    int i;
    sal_error_e err=SALERR_UNKNOWN;

    if ( device == 0 || kp_sp == 0 )
    {
        err = SALERR_UNKNOWN;
        goto fail;
    }

    desired_channels    = ( desired_channels == 0 ) ? DEFAULT_AUDIO_CHANNELS : desired_channels;
    desired_bits        = ( desired_bits     == 0 ) ? DEFAULT_AUDIO_BITS : desired_bits;
    desired_sample_rate = ( desired_sample_rate == 0 ) ? DEFAULT_AUDIO_SAMPLE_RATE  : desired_sample_rate;

    /* compute length of each buffer */
    buffer_length_ms = ( kp_sp->sp_buffer_length_ms == 0 ) ? DEFAULT_BUFFER_DURATION : kp_sp->sp_buffer_length_ms;
    buffer_length_ms /= _SAL_WAVEOUT_NUM_BUFFERS;

    /* assign destructor */
    device->device_fnc_destroy = destroy_device_waveout;

    /* make sure we have at least one device */
    if ( ( num_devices = waveOutGetNumDevs() ) < 1 )
    {
        _SAL_warning( device, "No audio devices found\n" );
        return SALERR_SYSTEMFAILURE;
    }

    /* query caps for the right format */
    while ( 1 )
    {
        DWORD format_mask = params_to_format( desired_channels, desired_bits, desired_sample_rate );
        WAVEOUTCAPS woc;

        format_mask = params_to_format( desired_channels, desired_bits, desired_sample_rate );

        waveOutGetDevCaps( WAVE_MAPPER, &woc, sizeof( woc ) );

        if ( woc.dwFormats & format_mask )
        {
            snprintf( device->device_info.di_name, sizeof( device->device_info.di_name ), "WAVEOUT: %s (support = 0x%x)", woc.szPname, woc.dwSupport );
            break;
        }

        /* try again, this time with our fallback rates */
        if ( desired_channels == DEFAULT_AUDIO_CHANNELS &&
             desired_bits == DEFAULT_AUDIO_BITS &&
             desired_sample_rate == DEFAULT_AUDIO_SAMPLE_RATE )
        {
            _SAL_warning( device, "No valid audio devices found\n" );
            err = SALERR_SYSTEMFAILURE;
            goto fail;
        }

        desired_channels    = DEFAULT_AUDIO_CHANNELS;
        desired_bits        = DEFAULT_AUDIO_BITS;
        desired_sample_rate = DEFAULT_AUDIO_SAMPLE_RATE;
    }

    /* allocate and clear memory for waveout private data */
    wod = ( SAL_WaveOutData * ) device->device_callbacks.alloc( sizeof( *wod ) );
    memset( wod, 0, sizeof( *wod ) );

    /* compute buffer size */
    wod->wod_buffer_length_ms = buffer_length_ms;
    wod->wod_buffer_size = ( desired_bits / 8 ) * desired_sample_rate * desired_channels * buffer_length_ms / 1000;

    /* forcibly align on 32-bit boundary */
    wod->wod_buffer_size &= ~(( desired_bits * desired_channels / 8 )-1);

    /* create event */
    wod->wod_hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

    /* fill in WAVEFORMATEX structure */
    memset( &wfx, 0, sizeof( wfx ) );

    wfx.wFormatTag      = WAVE_FORMAT_PCM; 
    wfx.nChannels       = desired_channels;
    wfx.nSamplesPerSec  = desired_sample_rate;
    wfx.wBitsPerSample  = desired_bits;
    wfx.nBlockAlign     = wfx.wBitsPerSample / 8 * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    
    /* open the waveout device */
    mmr = waveOutOpen( &wod->wod_hWaveOut,        /* pointer to HWAVEOUT */
                       WAVE_MAPPER,               /* device ID */
                       &wfx,                      /* pointer to WAVEFORMATEX */
                       ( DWORD ) wod->wod_hEvent, /* callback */
                       0,                         /* instance */
                       CALLBACK_EVENT );          /* no callback */

    if ( mmr != MMSYSERR_NOERROR )
    {
        device->device_callbacks.free( wod );
        _SAL_warning( device, "Could not open device, err = %d\n", mmr );
        return SALERR_SYSTEMFAILURE;
    }

    /* allocate memory and clear it to silence */
    for ( i = 0; i < _SAL_WAVEOUT_NUM_BUFFERS; i++ )
    {
        if ( ( wod->wod_buffer[ i ] = ( sal_byte_t * ) device->device_callbacks.alloc( wod->wod_buffer_size ) ) == 0 )
        {
            _SAL_warning( device, "Out of memory allocating buffers\n" );
            err = SALERR_OUTOFMEMORY;
            goto fail;
        }

        if ( desired_bits == 16 )
        {
            memset( wod->wod_buffer[ i ], 0, wod->wod_buffer_size );
        }
        else
        {
            memset( wod->wod_buffer[ i ], 0x80, wod->wod_buffer_size );
        }
    }

    /* prepare our headers */
    for ( i = 0; i < _SAL_WAVEOUT_NUM_BUFFERS; i++ )
    {
        memset( &wod->wod_wave_header[ i ], 0, sizeof( wod->wod_wave_header[ i ] ) );

        wod->wod_wave_header[ i ].dwFlags = 0;
        wod->wod_wave_header[ i ].dwBufferLength = wod->wod_buffer_size;
        wod->wod_wave_header[ i ].lpData         = (char*) wod->wod_buffer[ i ];
        wod->wod_wave_header[ i ].dwLoops        = 1;

        mmr = waveOutPrepareHeader( wod->wod_hWaveOut, &wod->wod_wave_header[ i ], sizeof( wod->wod_wave_header[ i ] ) );

        if ( mmr != MMSYSERR_NOERROR )
        {
            _SAL_warning( device, "Could not prepare header, err = %d\n", mmr );
            err = SALERR_SYSTEMFAILURE;
            goto fail;
        }
    }

    /* reset the event before sending audio out */
    ResetEvent( wod->wod_hEvent );

    /* write out all buffers */
    for ( i = 0; i < _SAL_WAVEOUT_NUM_BUFFERS; i++ )
    {
        mmr = waveOutWrite( wod->wod_hWaveOut, &wod->wod_wave_header[ i ], sizeof( wod->wod_wave_header ) );
        if ( mmr != MMSYSERR_NOERROR )
        {
            _SAL_warning( device, "Could not prepare header, err = %d\n", mmr );
            err = SALERR_SYSTEMFAILURE;
            goto fail;
        }
    }

    /* save info */
    wod->wod_device_id                 = WAVE_MAPPER;
    device->device_data                = wod;
    device->device_info.di_size        = sizeof( device->device_info );
    device->device_info.di_bits        = wfx.wBitsPerSample;
    device->device_info.di_channels    = wfx.nChannels;
    device->device_info.di_sample_rate = wfx.nSamplesPerSec;

    /* kick off our thread */
    _SAL_create_thread( device, s_audio_thread, device );

    return SALERR_OK;
fail:
    if ( wod )
    {
        if ( wod->wod_hWaveOut )
        {
            waveOutReset( wod->wod_hWaveOut );
            waveOutClose( wod->wod_hWaveOut );
        }
        for ( i = 0; i < _SAL_WAVEOUT_NUM_BUFFERS; i++ )
        {
            waveOutUnprepareHeader( wod->wod_hWaveOut, &wod->wod_wave_header[ i ], sizeof( wod->wod_wave_header ) );
            if ( wod->wod_buffer[ i ] )
            {
                device->device_callbacks.free( wod->wod_buffer[ i ] );
            }
        }
        device->device_callbacks.free( wod );
    }
    return err;
}

static 
void
destroy_device_waveout( SAL_Device *device )
{
    int i;
    SAL_WaveOutData *wod;

    if ( device == 0 || device->device_data == 0 )
    {
        return;
    }
    
    /* lock device */
    _SAL_lock_device( device );

    wod = ( SAL_WaveOutData * ) device->device_data;

    /* stop playing */
    waveOutReset( wod->wod_hWaveOut );

    /* wait */
    for ( i = 0; i < 100; i++ )
    {
        int j;
        int num_waiting = 0;

        for ( j = 0; j < _SAL_WAVEOUT_NUM_BUFFERS; j++ )
        {
            if ( !( wod->wod_wave_header[ j ].dwFlags & WHDR_DONE ) )
            {
                SAL_sleep( device, 10 );
                num_waiting++;
                break;
            }
            else
            {
                break;
            }
        }

        if ( !num_waiting )
            break;
    }

    /* unprepare headers */
    for ( i = 0; i < _SAL_WAVEOUT_NUM_BUFFERS; i++ )
    {
        waveOutUnprepareHeader( wod->wod_hWaveOut, &wod->wod_wave_header[ i ], sizeof( wod->wod_wave_header[ i ] ) );
    }

    /* close */
    waveOutClose( wod->wod_hWaveOut );

    /* signal the mutex */
    SetEvent( wod->wod_hEvent );

    /* kill the thread */
    wod->wod_kill_audio_thread = 1;

    /* unlock device */
    _SAL_unlock_device( device );

    /* wait for the thread to terminate */
    SAL_sleep( device, wod->wod_buffer_length_ms * _SAL_WAVEOUT_NUM_BUFFERS );

    /* release resource */
    CloseHandle( wod->wod_hEvent );

    for ( i = 0; i < _SAL_WAVEOUT_NUM_BUFFERS; i++ )
    {
        device->device_callbacks.free( wod->wod_buffer[ i ] );
    }
    device->device_callbacks.free( wod );
    device->device_data = 0;
}

#endif /* POSH_OS_WIN32 */
