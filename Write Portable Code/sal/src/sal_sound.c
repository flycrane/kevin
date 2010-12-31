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
/** @file sal_sound.c
    @brief Simple Audio File sound functions.
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "sal.h"
#include <string.h>

/** @defgroup VoiceManagement Voice Management
    @{
*/

/** @brief play a sample, returning a voice handle for further management
    @param[in] device pointer to SAL_Device
    @param[in] p_sample pointer to sample to use for this sound
    @param[in] p_sid pointer to sal_voice_t to store the active voice's id
    @param[in] volume volume of the sound (0 to SAL_VOLUME_MAX)
    @param[in] pan pan position of the sound (SAL_PAN_HARD_LEFT to SAL_PAN_HARD_RIGHT)
    @param[in] loop_start loop start position
    @param[in] loop_end loop end position, can set to 0 if you want to just use the sample's end position
    @param[in] num_repetitions number of times to play the sound, use SAL_LOOP_ALWAYS for infinite repeats 
    This function starts playback of a previously loaded sample, returning an identifier for the voice in
    the p_sid parameter.  Using that identifier an application can adjust the voice's parameters such as
    pan and volume, along with stopping the voice.  One potentialy tricky area is that you can start playing
    a sound, have it end, and then later use that "stale" voice handle only to find it bound to another
    voice.  One way to fix this is to force the application to "free" a voice handle, but that seemed
    far too cumbersome for a rare error.
    @sa SAL_stop_voice, SAL_set_voice_volume, SAL_set_voice_pan, SAL_get_voice_status
*/
sal_error_e
SAL_play_sample( SAL_Device *device,
                 SAL_Sample *p_sample, 
                 sal_voice_t *p_sid,
                 sal_volume_t volume,
                 sal_pan_t pan,
                 sal_u32_t loop_start,
                 sal_u32_t loop_end,
                 sal_i32_t num_repetitions )
{
    int i;
    SAL_Voice *p_voice;

    if ( device == 0 || p_sample == 0 || p_sid == 0 || ( loop_start > loop_end ) )
    {
        return SALERR_INVALIDPARAM;
    }

    _SAL_lock_device( device );

    /* find a free voice */
    p_voice = device->device_voices;

    for ( i = 0; i < device->device_max_voices; i++, p_voice++ )
    {
        if ( p_voice->voice_num_repetitions == 0 )
        {
            break;
        }
    }

    if ( i == device->device_max_voices )
    {
        _SAL_unlock_device( device );
        return SALERR_OUTOFVOICES;
    }

    /* set the default end of loop to the end of the sample */
    if ( loop_end == 0 )
    {
        loop_end = p_sample->sample_num_samples;
    }

    /* set up this voice */
    p_voice->voice_sample          = p_sample;
    p_voice->voice_cursor          = 0;
    p_voice->voice_volume          = volume;
    p_voice->voice_pan             = pan;
    p_voice->voice_loop_start      = loop_start;
    p_voice->voice_loop_end        = loop_end;
    p_voice->voice_num_repetitions = num_repetitions;

    /* increment sample's ref count */
    p_sample->sample_ref_count++;

    /* store voice identifier */
    *p_sid = i;

    /* unlock device */
    _SAL_unlock_device( device );

    return SALERR_OK;
}

/** @brief stops a currently playing voice
    @param[in] p_device pointer to output device
    @param[in] sid handle to the currently playing voice
    @returns SALERR_OK on success, @ref sal_error_e on failure
    Stops a currently playing voice
*/
sal_error_e 
SAL_stop_voice( SAL_Device *p_device, 
                sal_voice_t sid )
{
    if ( p_device == 0 || sid < 0 || sid >= p_device->device_max_voices )
    {
        return SALERR_INVALIDPARAM;
    }

    /* lock the device */
    _SAL_lock_device( p_device );

    if ( p_device->device_voices[ sid ].voice_num_repetitions != 0 )
    {
        /* decrement the ref count */
        if ( --p_device->device_voices[ sid ].voice_sample->sample_ref_count <= 0 )
        {
            _SAL_destroy_sample_raw( p_device, p_device->device_voices[ sid ].voice_sample );
        }
    }

    /* clear the voice */
    memset( &p_device->device_voices[ sid ], 0, sizeof( p_device->device_voices[ 0 ] ) );

    /* unlock the device */
    _SAL_unlock_device( p_device );

    return SALERR_OK;
}

/** @brief returns the status of a playing voice
    @param[in] p_device pointer to output device
    @param[in] sid voice id
    @param[in] p_status address of a sal_voice_status_e variable
    @returns SALERR_OK, @ref sal_error_e otherwise
    This function queries the status of an active voice.  It will be one of the
    constants defined by the sal_voice_status_e enumerant.
*/
sal_error_e 
SAL_get_voice_status( SAL_Device *p_device, 
                      sal_voice_t sid,
                      sal_voice_status_e *p_status )
{
    if ( p_device == 0 || p_status == 0 || sid < 0 || sid >= p_device->device_max_voices )
    {
        if ( p_status && p_device != 0 )
        {
            *p_status = SALVS_INVALIDSOUND;
        }

        return SALERR_INVALIDPARAM;
    }

    /* we need to lock the device before we start inspecting things */
    _SAL_lock_device( p_device );

    if ( p_device->device_voices[ sid ].voice_num_repetitions == 0 )
    {
        *p_status = SALVS_IDLE;
    }
    else
    {
        *p_status = SALVS_PLAYING;
    }

    /* unlock the device */
    _SAL_unlock_device( p_device );

    return SALERR_OK;
}

/** @brief Sets the volume of a sound.
    @param[in] p_device pointer to output device
    @param[in] sid id of the voice we're adjusting the volume of
    @param[in] volume volume we're setting the voice to (0 to 65535)
    @returns SALERR_OK, @ref sal_error_e otherwise
*/
sal_error_e 
SAL_set_voice_volume( SAL_Device *p_device, sal_voice_t sid, sal_volume_t volume )
{
    if ( p_device == 0 || sid < 0 || sid >= p_device->device_max_voices )
    {
        return SALERR_INVALIDPARAM;
    }

    _SAL_lock_device( p_device );

    p_device->device_voices[ sid ].voice_volume = volume;

    _SAL_unlock_device( p_device );

    return SALERR_OK;
}

/** @brief Sets the pan position of a sound.
    @param[in] p_device pointer to output device
    @param[in] sid id of the voice we're adjusting the volume of
5    @param[in] pan pan we're setting the voice to (SAL_PAN_HARD_LEFT to SAL_PAN_HARD_RIGHT)
    @returns SALERR_OK, @ref sal_error_e otherwise
*/
sal_error_e 
SAL_set_voice_pan( SAL_Device *p_device, sal_voice_t sid, sal_pan_t pan )
{
    if ( p_device == 0 || sid < 0 || sid >= p_device->device_max_voices )
    {
        return SALERR_INVALIDPARAM;
    }

    _SAL_lock_device( p_device );

    p_device->device_voices[ sid ].voice_pan = pan;

    _SAL_unlock_device( p_device );

    return SALERR_OK;
}

/** @brief Return the sample associated with a playing sound
    @param[in] p_device pointer to output device
    @param[in] sid sound id
    @param[out] pp_sample address of pointer to sample to store the result
    @returns SALERR_OK on success, @ref sal_error_e otherwise
*/
sal_error_e 
SAL_get_voice_sample( SAL_Device *p_device, 
                      sal_voice_t sid,
                      SAL_Sample **pp_sample )
{
    if ( p_device == 0 || sid < 0 || sid >= p_device->device_max_voices || pp_sample == 0 ) 
    {
        return SALERR_INVALIDPARAM;
    }

    _SAL_lock_device( p_device );

    *pp_sample = p_device->device_voices[ sid ].voice_sample;

    _SAL_unlock_device( p_device );

    return SALERR_OK;
}

/** @brief Returns the current cursor (in sample space) for the sound
    @param[in] p_device pointer to output device
    @param[in] sid sound id
    @param[out] p_cursor address of integer in which to store sample
    @returns SALERR_OK on success, @ref sal_error_e otherwise
*/
sal_error_e 
SAL_get_voice_cursor( SAL_Device *p_device,
                      sal_voice_t sid,
                      int *p_cursor )
{
    if ( p_device == 0 || sid < 0 || sid >= p_device->device_max_voices || p_cursor == 0 ) 
    {
        return SALERR_INVALIDPARAM;
    }

    _SAL_lock_device( p_device );

    *p_cursor = p_device->device_voices[ sid ].voice_cursor;

    _SAL_unlock_device( p_device );

    return SALERR_OK;
}
/**@brief advances a voice's cursor, checking for loops and repetitions
   @internal
   This is an internal function that advances the given voice ahead by one
   frame.  If it reaches the loop end it decrements the 
   number of repetitions and returns 0.  This is exposed in the public API
   since it can be used by a user-defined sample type.

   @param[in] p_device pointer to output device
   @param[in] sid sound id
   @param[in] num_frames number of frames to advance the voice
   @returns 0 if the voice has ended playing
   @returns 1 if the voice is still playing
*/
int
_SAL_advance_voice( SAL_Device *p_device, sal_voice_t sid, int num_frames )
{
    /* parameter validation should have happened upstream */
    SAL_Voice *voice = &p_device->device_voices[ sid ];

    voice->voice_cursor += num_frames;

    /* voice has a loop end marker */
    if ( voice->voice_loop_end )
    {
        if ( voice->voice_cursor >= voice->voice_loop_end )
        {
            voice->voice_cursor = voice->voice_loop_start;

            if ( voice->voice_num_repetitions != SAL_LOOP_ALWAYS )
            {
                if ( --voice->voice_num_repetitions <= 0 )
                {
                    /* voice should be freed upstream */
                    return 0;
                }
            }
        }
    }
    /* no loop end marker */
    else
    {
        /* don't do anything */
    }


    return 1;
}

/** @} */
