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
/** @file sal_dsound.c
    @brief DirectSound implementation (Windows only)
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "../sal.h"

/** @define SAL_SUPPORT_DIRECTSOUND
    If defined, DirectSound support is compiled
*/
#if ( defined POSH_OS_WIN32 && defined SAL_SUPPORT_DIRECTSOUND ) || defined SAL_DOXYGEN

#define DEFAULT_BUFFER_DURATION   50        /**< default buffer length, in milliseconds, if unspecified */

#include <windows.h>
#include <dsound.h>
#include <process.h>
#include <string.h>
#include <assert.h>

/** @internal
    @brief DirectSound private device data
    @ingroup Win32
*/
typedef struct
{
    LPDIRECTSOUND       dsd_lpDS;                /**< pointer to directsound object */
    LPDIRECTSOUNDBUFFER dsd_lpPrimaryBuffer;     /**< pointer to primary buffer */

    LPDIRECTSOUNDBUFFER dsd_lpSecondaryBuffer;   /**< pointer to secondary buffer */
    DWORD               dsd_dwWriteLocation;     /**< write location into secondary buffer */
    DWORD               dsd_dwBufferSize;        /**< size of the secondary buffer */

    int                 dsd_kill_audio_thread;   /**< set to 1 to kill audio thread */
    int                 dsd_buffer_length_ms;    /**< length of the buffer in milliseconds */
} SAL_DirectSoundData;

static void destroy_device_data_dsound( SAL_Device *device );

static
void
s_audio_thread( void *args )
{
    SAL_Device *device = ( SAL_Device * ) args;
    SAL_DirectSoundData *dsd = ( SAL_DirectSoundData * ) device->device_data;
    DWORD dwStatus;
    DWORD dwCurrentPlayCursor, dwCurrentWriteCursor;
    HRESULT hr;
    int sleep_duration;

    sleep_duration = dsd->dsd_buffer_length_ms/4;

    while ( 1 )
    {
        /* grab lock */
        _SAL_lock_device( device );

        /* time to quit? */
        if ( dsd->dsd_kill_audio_thread )
        {
            dsd->dsd_kill_audio_thread = 0;
            _SAL_unlock_device( device );
            return;
        }

        /* get status (for debugging) */
        dsd->dsd_lpSecondaryBuffer->lpVtbl->GetStatus( dsd->dsd_lpSecondaryBuffer, &dwStatus );

        /* retrieve the play cursor so that we know how much data we need to write into
           the buffer */
        hr = dsd->dsd_lpSecondaryBuffer->lpVtbl->GetCurrentPosition( dsd->dsd_lpSecondaryBuffer, 
                                                                     &dwCurrentPlayCursor, 
                                                                     &dwCurrentWriteCursor );

        /* mix our samples into the buffer */
        if ( hr == DS_OK )
        {
            LPVOID lpMixBuffer0, lpMixBuffer1;
            DWORD  dwLength0, dwLength1;

            int bytes_to_write = ( ( int ) dwCurrentPlayCursor - dsd->dsd_dwWriteLocation );

            while ( bytes_to_write < 0 )
            {
                bytes_to_write += dsd->dsd_dwBufferSize;
            }

            /* go ahead and fill the buffer */
            if ( bytes_to_write > 0 )
            {
                if ( dsd->dsd_lpSecondaryBuffer->lpVtbl->Lock( dsd->dsd_lpSecondaryBuffer,
                                                               dsd->dsd_dwWriteLocation,      /* write cursor */
                                                               bytes_to_write,                /* size of lock */
                                                               &lpMixBuffer0,                 /* first write pointer */
                                                               &dwLength0,                    /* length of first write buffer */
                                                               &lpMixBuffer1,                 /* second write pointer */
                                                               &dwLength1,                    /* length of second write buffer */
                                                               0 ) == DS_OK )                 /* flags */
                {
                    /* mix into the first buffer */
                    _SAL_mix_chunk( device, 
                                    lpMixBuffer0, 
                                    dwLength0 );

                    /* mix into the second buffer if necessary */
                    if ( dwLength1 > 0 )
                    {
                        _SAL_mix_chunk( device, 
                                        lpMixBuffer1, 
                                        dwLength1 );
                    }

                    /* unlock the buffer */
                    dsd->dsd_lpSecondaryBuffer->lpVtbl->Unlock( dsd->dsd_lpSecondaryBuffer, 
                                                                lpMixBuffer0, 
                                                                dwLength0, 
                                                                lpMixBuffer1, 
                                                                dwLength1 );
                }
                else
                {
                    _SAL_warning( device, "Could not Lock secondary buffer, error = %d\n", hr & 0xFFFF );
                }

                dsd->dsd_dwWriteLocation = ( dsd->dsd_dwWriteLocation + bytes_to_write ) % dsd->dsd_dwBufferSize;
            }
        }

        /* unlock access */
        _SAL_unlock_device( device );

        /* sleep and hang out */
        SAL_sleep( device, sleep_duration );
    }
}

/** @internal
    @ingroup Win32
    @brief DirectSound specific device creation function
    @param[in] device pointer to output device
    @param[in] kp_sp pointer to system parameters structure, may NOT be NULL
    @param[in] desired_channels  number of desired output channels, 
    may be 0 if you want it to use default preferences
    @param[in] desired_bits number of bits per sample, specify 0 for system default
    @param[in] desired_sample_rate desired sample rate, in samples/second, specify 0 for system default
*/
sal_error_e
_SAL_create_device_data_dsound( SAL_Device *device, 
                                const SAL_SystemParameters *kp_sp, 
                                sal_u32_t desired_channels, 
                                sal_u32_t desired_bits, 
                                sal_u32_t desired_sample_rate )
{
    SAL_DirectSoundData *dsd = 0;
    HRESULT hr;
    DSBUFFERDESC dsbdesc;
    WAVEFORMATEX wfx;
    DSCAPS dsCaps;
    WAVEFORMATEX wfex;
    int buffer_length_ms = 0;

    if ( device == 0 || kp_sp == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    buffer_length_ms = ( kp_sp->sp_buffer_length_ms == 0 ) ? DEFAULT_BUFFER_DURATION : kp_sp->sp_buffer_length_ms;

    device->device_fnc_destroy = destroy_device_data_dsound;
    
    dsd = ( SAL_DirectSoundData * ) device->device_callbacks.alloc( sizeof( SAL_DirectSoundData ) );

    memset( dsd, 0, sizeof( *dsd ) );

    dsd->dsd_buffer_length_ms = buffer_length_ms;

    /* create directsound object */
    if ( ( hr = DirectSoundCreate( NULL,           /* device GUID */
                                   &dsd->dsd_lpDS, /* pointer to LPDIRECTSOUND */
                                   NULL ) ) != DS_OK )
    {
        device->device_fnc_destroy( device );

        return SALERR_SYSTEMFAILURE;
    }

    /* set cooperative level */
    if ( ( hr = dsd->dsd_lpDS->lpVtbl->SetCooperativeLevel( dsd->dsd_lpDS, 
                                                            kp_sp->sp_hWnd, 
                                                            DSSCL_PRIORITY ) ) != DS_OK )
    {
        _SAL_warning( device, "SetCooperativeLevel failed with error '%d'\n", hr & 0xFFFF );

        device->device_fnc_destroy( device );

        return SALERR_SYSTEMFAILURE;
    }

    /* obtain primary buffer so we can set its format */
    memset( &dsbdesc, 0, sizeof( dsbdesc ) );
    dsbdesc.dwSize = sizeof( dsbdesc );
    dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

    if ( ( hr = dsd->dsd_lpDS->lpVtbl->CreateSoundBuffer( dsd->dsd_lpDS,
                                                          &dsbdesc, 
                                                          &dsd->dsd_lpPrimaryBuffer,
                                                          NULL ) ) != DS_OK )
    {
        _SAL_warning( device, "Could not create primary buffer\n" );
        device->device_fnc_destroy( device );

        return SALERR_SYSTEMFAILURE;
    }

    /* set the format */
    memset( &wfx, 0, sizeof( wfx ) );

    desired_channels    = ( desired_channels == 0 ) ? DEFAULT_AUDIO_CHANNELS : desired_channels;
    desired_bits        = ( desired_bits     == 0 ) ? DEFAULT_AUDIO_BITS : desired_bits;
    desired_sample_rate = ( desired_sample_rate == 0 ) ? DEFAULT_AUDIO_SAMPLE_RATE  : desired_sample_rate;

    wfx.wFormatTag      = WAVE_FORMAT_PCM; 
    wfx.nChannels       = desired_channels;
    wfx.nSamplesPerSec  = desired_sample_rate;
    wfx.wBitsPerSample  = desired_bits;
    wfx.nBlockAlign     = wfx.wBitsPerSample / 8 * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if ( ( hr = dsd->dsd_lpPrimaryBuffer->lpVtbl->SetFormat( dsd->dsd_lpPrimaryBuffer, &wfx ) ) != DS_OK )
    {
        _SAL_warning( device, "Could not set format\n" );
        device->device_fnc_destroy( device );

        return SALERR_SYSTEMFAILURE;
    }

    /* Get caps (for debugging) */
    dsCaps.dwSize = sizeof( dsCaps );

    if ( ( hr = dsd->dsd_lpDS->lpVtbl->GetCaps( dsd->dsd_lpDS, &dsCaps ) ) != DS_OK )
    {
        _SAL_warning( device, "Could not GetCaps\n" );
        device->device_fnc_destroy( device );

        return SALERR_SYSTEMFAILURE;
    }

    /* create the secondary buffer that we'll be streaming into */
        {
            LPDIRECTSOUNDBUFFER lpBuffer = 0;
            PCMWAVEFORMAT pcmwf;
            DSBUFFERDESC dsbdesc;
            void *pWritePtr = 0, *pWritePtr2 = 0;
            DWORD dwLength = 0, dwLength2 = 0;

            /* compute buffer size */
            int buffer_size = ( desired_bits / 8 ) * desired_sample_rate * desired_channels * buffer_length_ms / 1000;

            /* forcibly align on 32-bit boundary */
            buffer_size &= ~(( desired_bits * desired_channels / 8 )-1);

            /* set up the wave format structure */
            memset( &pcmwf, 0, sizeof( pcmwf ) );
            pcmwf.wf.wFormatTag      = WAVE_FORMAT_PCM;
            pcmwf.wf.nChannels       = desired_channels;
            pcmwf.wf.nSamplesPerSec  = desired_sample_rate;
            pcmwf.wf.nBlockAlign     = ( desired_bits / 8 ) * ( desired_channels );
            pcmwf.wf.nAvgBytesPerSec = pcmwf.wf.nSamplesPerSec * pcmwf.wf.nBlockAlign;
            pcmwf.wBitsPerSample     = desired_bits;

            /* set up DSBUFFERDESC structure */
            memset( &dsbdesc, 0, sizeof( DSBUFFERDESC ) );
            dsbdesc.dwSize = sizeof( dsbdesc );
            dsbdesc.dwFlags = DSBCAPS_CTRLPAN | 
                DSBCAPS_CTRLVOLUME | 
                DSBCAPS_STATIC | 
                DSBCAPS_GETCURRENTPOSITION2 |
                DSBCAPS_CTRLPOSITIONNOTIFY |
                DSBCAPS_LOCSOFTWARE;
        
            dsbdesc.dwBufferBytes = buffer_size;
            dsbdesc.lpwfxFormat   = (LPWAVEFORMATEX) &pcmwf; 

            /* create the buffer */
            if ( ( hr = dsd->dsd_lpDS->lpVtbl->CreateSoundBuffer( dsd->dsd_lpDS,
                                                                  &dsbdesc, 
                                                                  &lpBuffer, 
                                                                  NULL ) ) != DS_OK )
            {
                _SAL_warning( device, "Failed to CreateSoundBuffer for secondary sound buffer\n" );
                device->device_fnc_destroy( device );

                return SALERR_SYSTEMFAILURE;
            }

            /* lock and zero the buffer */
            hr = lpBuffer->lpVtbl->Lock( lpBuffer,
                                         0,                      //Offset at which to start lock
                                         buffer_size,            //Size of lock; ignored because of flag
                                         &pWritePtr,             //Pointer to start of lock
                                         &dwLength,              //Size of first part of lock
                                         &pWritePtr2,            //Pointer to second part of lock
                                         &dwLength2,             //Size of second part of lock
                                         0 );

            if ( hr != DS_OK )
            {
                _SAL_warning( device, "Could not lock secondary buffer\n" );

                device->device_fnc_destroy( device );

                lpBuffer->lpVtbl->Release( lpBuffer );

                return SALERR_SYSTEMFAILURE;
            }

            if ( dsbdesc.lpwfxFormat->wBitsPerSample == 16 )
            {
                memset( pWritePtr, 0, dsbdesc.dwBufferBytes );
            }
            else
            {
                memset( pWritePtr, 0x80, dsbdesc.dwBufferBytes );
            }

            lpBuffer->lpVtbl->Unlock( lpBuffer, pWritePtr, dwLength, NULL, 0 );

            /* get the buffer playing in the background */
            lpBuffer->lpVtbl->Play( lpBuffer, 0, 0, DSBPLAY_LOOPING );

            dsd->dsd_lpSecondaryBuffer = lpBuffer;
            dsd->dsd_dwWriteLocation    = 0;
            dsd->dsd_dwBufferSize       = dsbdesc.dwBufferBytes;
        }

    dsd->dsd_kill_audio_thread = 0;

    /* save device info */
    memset( &wfex, 0, sizeof( wfex ) );
    wfex.cbSize = sizeof( wfex );

    dsd->dsd_lpSecondaryBuffer->lpVtbl->GetFormat( dsd->dsd_lpSecondaryBuffer, &wfex, sizeof( wfex ), 0 );
    
    device->device_info.di_size        = sizeof( device->device_info );
    device->device_info.di_bits        = wfex.wBitsPerSample;
    device->device_info.di_channels    = wfex.nChannels;
    device->device_info.di_sample_rate = wfex.nSamplesPerSec;
    strncpy( device->device_info.di_name, "DirectSound", sizeof( device->device_info.di_name ) );

    /* store device data */
    device->device_data = dsd;

    /* kick off the audio filler thread */
    _SAL_create_thread( device, s_audio_thread, device );

    return SALERR_OK;
}

static
void
destroy_device_data_dsound( SAL_Device *device )
{
    SAL_DirectSoundData *dsd;

    if ( device == 0 || device->device_data == 0 )
    {
        return;
    }
    
    /* lock device */
    _SAL_lock_device( device );

    dsd = ( SAL_DirectSoundData * ) device->device_data;

    /* stop and release all buffers/directsound objects */
    if ( dsd )
    {
        if ( dsd->dsd_lpSecondaryBuffer )
        {
            dsd->dsd_lpSecondaryBuffer->lpVtbl->Stop( dsd->dsd_lpSecondaryBuffer );
            dsd->dsd_lpSecondaryBuffer->lpVtbl->Release( dsd->dsd_lpSecondaryBuffer );
        }
        if ( dsd->dsd_lpPrimaryBuffer )
        {
            dsd->dsd_lpPrimaryBuffer->lpVtbl->Stop( dsd->dsd_lpPrimaryBuffer );
            dsd->dsd_lpPrimaryBuffer->lpVtbl->Release( dsd->dsd_lpPrimaryBuffer );
        }
        if ( dsd->dsd_lpDS )
        {
            dsd->dsd_lpDS->lpVtbl->Release( dsd->dsd_lpDS );
        }
    }

    /* tell the thread that it's time to quit */
    dsd->dsd_kill_audio_thread = 1;

    /* unlock */
    _SAL_unlock_device( device );

    /* sleep for some period of time, waiting for the thread to exit.  Yes,
       I should probably do something else like SignalAndWaitForObject, but
       that's a lot of overhead for a one time thing like this */
    SAL_sleep( device, dsd->dsd_buffer_length_ms * 4 );

    assert( dsd->dsd_kill_audio_thread == 0 );

    /* free memory */
    device->device_callbacks.free( device->device_data );
    device->device_data = 0;
}

#endif /* POSH_OS_WIN32 */
