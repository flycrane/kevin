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
/** @file sal_mixer.c
    @brief Simple Audio File mixer functions
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "sal.h"
#include <string.h>

/** @internal
    @brief Mixes a source buffer onto a cumulative mix buffer
    @param[in] device pointer to output device
    @param[in,out] pointer to the destination buffer for mixing

    @param[in] kp_src pointer to the source buffer
    @param[in] src_bytes number of bytes in the buffer pointed to by kp_src
    @param[in] voice_volume volume of the voice

    This splats one buffer onto another one, doing volume and pan
    adjustment at the same time.  Panning is obviously ignored with
    monoaural output devices, and it's implemented naively by simply
    linearly adjusting volume based on the pan position.  A better panning
    system would take into account perceptual response to and adjust the
    values appropriately.

    Note that the device does not need to be locked here, since it is
    assumed that the caller will have locked the device before calling
    this routine.
*/
static
void
submix_buffer( SAL_Device *device,
               sal_byte_t *p_dst, 
               const sal_byte_t *kp_src, 
               sal_i32_t src_bytes,
               sal_u16_t voice_volume,
               sal_i16_t voice_pan )
{
    int samples_to_mix;
    int i;

    samples_to_mix = src_bytes / device->device_info.di_bytes_per_sample;

    if ( device->device_info.di_bits == 8 )
    {
        if ( device->device_info.di_channels == 1 )
        {
            for ( i = 0; i < samples_to_mix; i++, p_dst += 1 )
            {
                sal_i16_t a = *p_dst - 128;
                sal_i16_t b = ( kp_src[ i ] - 128 );

                a = a + ( ( b * voice_volume ) >> 16 ) + 128;

                if ( a > 255 ) a = 255;
                if ( a < 0 ) a = 0;

                *p_dst = ( sal_byte_t ) ( a );
            }
        }
        else
        {
            for ( i = 0; i < samples_to_mix; i++, p_dst += 1 )
            {
                sal_u32_t volume = voice_volume;
                sal_i16_t a = *p_dst - 128;
                sal_i16_t b = ( kp_src[ i ] - 128 );

                /* right side */
                if ( i & 1 )
                {
                    volume += voice_pan*2;
                }
                /* left side */
                else
                {
                    volume -= voice_pan*2;
                }

                if ( volume < 0 ) volume = 0;
                if ( volume > 65535 ) volume = 65535;

                a = a + ( ( b * volume ) >> 16 ) + 128;

                if ( a > 255 ) a = 255;
                if ( a < 0 ) a = 0;

                *p_dst = ( sal_byte_t ) ( a );
            }
        }
    }
    else if ( device->device_info.di_bits == 16 )
    {
        const sal_i16_t *kp_src16 = ( const sal_i16_t * ) kp_src;

        /* stereo devices need to take panning into consideration */
		if ( device->device_info.di_channels == 2 )
		{
            for ( i = 0; i < samples_to_mix; i++, p_dst += 2 )
            {
                sal_u32_t volume = voice_volume;

                /* right side */
                if ( i & 1 )
                {
                    volume += voice_pan*2;
                }
                /* left side */
                else
                {
                    volume -= voice_pan*2;
                }

                if ( volume < 0 ) volume = 0;
                if ( volume > 65535 ) volume = 65535;

                * ( sal_i16_t * ) p_dst += ( sal_i16_t ) ( ( kp_src16[ i ] * volume ) >> 16 );
            }
        }
        else
        {
            for ( i = 0; i < samples_to_mix; i++, p_dst += 2 )
            {
                * ( sal_i16_t * ) p_dst += ( sal_i16_t ) ( ( kp_src16[ i ] * voice_volume ) >> 16 );
            }
        }
    }
}

/** @internal
    @brief This is the core SAL chunk of code, responsible for iterating over all
    available voices and mixing them into the destination buffer.
    @param[in] device pointer to output device
    @param[out] p_dst pointer to destination buffer
    @param[in] bytes_to_mix number of bytes we need to mix
    @returns SALERR_OK on success, @ref sal_error_e otherwise

    This function mixes stuff.
*/
sal_error_e
_SAL_mix_chunk( SAL_Device *device, 
                sal_byte_t *p_dst, 
                sal_u32_t   bytes_to_mix )
{
    int i;
    sal_byte_t clear_value;
    sal_byte_t decode_buffer[ 512 ];

    /* lock the device */
    _SAL_lock_device( device );
	
    /* clear out the chunk to zero or 0x80 first depending on bit depth */
    clear_value = ( device->device_info.di_bits == 8 ) ? 0x80 : 0;
    memset( p_dst, clear_value, bytes_to_mix );
	
    /*
    ** iterate over all the voices and decode/submix their sample data
    ** into our larger buffer
    */
    for ( i = 0; i < device->device_max_voices; i++ )
    {
        SAL_Voice *p_voice;

      	p_voice = &device->device_voices[ i ];

        if ( p_voice->voice_num_repetitions == 0 )
        {
            continue;
        }
        else
        {
            int bytes_left = bytes_to_mix;
            int voice_ended = 0;

            /* decode up to sizeof( decode_buffer ) bytes at a time */
            while ( bytes_left > 0 )
            {
                int bytes_to_decode = ( bytes_left > sizeof( decode_buffer ) ) ? sizeof( decode_buffer ) : bytes_left;

                /* call the sample's specific decoding function, which returns 1 if the voice has
                   played out (i.e. reached end of the sample and there are no more loop repetitions
                   left */
                voice_ended = p_voice->voice_sample->sample_fnc_decoder( device, i, decode_buffer, bytes_to_decode );

                /* submix the decoded buffer into the destination */
                submix_buffer( device,                                  /* device */
                               p_dst + ( bytes_to_mix - bytes_left ),   /* destination for the mixdown */
                               decode_buffer,                           /* src buffer (should match dest buffer's format */
                               bytes_to_decode,                         /* number of bytes to decode */
                               p_voice->voice_volume,                   /* voice volume */
                               p_voice->voice_pan );                    /* voice pan */

                bytes_left -= bytes_to_decode;

                /* if the voice has ended, adjust the ref count and clear out the voice. 
                   This has to be done _after_ we do the submix and not inside the
                   decoder itself.  */
                if ( voice_ended )
                {
                    /* decrease the ref count on our source sample */
                    --p_voice->voice_sample->sample_ref_count;

                    /* clear voice entry */
                    memset( p_voice, 0, sizeof( *p_voice ) );
                    break;
                }
            }
        }
    }
	
    /* unlock the device */
	_SAL_unlock_device( device );

    return SALERR_OK;
}

