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
/** @file salx_wave.c
    @author Brian Hook
    @date    2004
    @brief WAVE sample support for the Simple Audio Library
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "../sal.h"

#include <string.h>

/** @internal
    @brief WAV file header structure */
typedef struct
{
    char         wh_riff[ 4 ];          /**< riff tag, should be 'riff' */
    sal_u32_t    wh_size;               /**< size of the header */
    char         wh_wave[ 4 ];          /**< wave tag, should be 'wave' */
    char         wh_fmt[ 4 ];           /**< fmt tag, should be 'fmt ' */
    sal_u32_t    wh_chunk_header_size;  /**< chunk header size */
} _SAL_WaveHeader;

/** @internal
    @brief RIFF chunk structure */
typedef struct
{
    sal_i16_t       wc_tag;               /**< 1 = uncompressed */
    sal_i16_t       wc_num_channels;      /**< number of channels in the wave file */
    sal_i32_t       wc_sample_rate;       /**< sample rate */
    sal_i32_t       wc_bytes_per_second;  /**< bytes per second*/
    sal_i16_t       wc_alignment;         /**< alignment */
    sal_i16_t       wc_bits_per_sample;   /**< bits per sample */
    char            wc_data[ 4 ];         /**< data chunk tag, should be 'data' */
    sal_i32_t       wc_data_size;         /**< size of the data in this chunk */
} _SAL_WaveChunk;

static 
void 
transform_identity_mono_8( const sal_byte_t *kp_src,
                           sal_byte_t *p_dst )
{
    *p_dst = *kp_src;
}

static 
void 
transform_mono_8_to_stereo_8( const sal_byte_t *kp_src,
                              sal_byte_t *p_dst )
{
    p_dst[ 1 ] = p_dst[ 0 ] = *kp_src;
}

static 
void 
transform_mono_16_to_stereo_16( const sal_byte_t *kp_src,
                                sal_byte_t *p_dst )
{
	sal_i16_t *dst_16 = ( sal_i16_t * ) p_dst;
	
	dst_16[ 1 ] = dst_16[ 0 ] = POSH_ReadI16FromLittle( kp_src );
}

static 
void 
transform_identity_stereo_8( const sal_byte_t *kp_src,
                             sal_byte_t *p_dst )
{
    p_dst[ 0 ] = kp_src[ 0 ];
    p_dst[ 1 ] = kp_src[ 1 ];
}

static 
void 
transform_identity_mono_16( const sal_byte_t *kp_src,
                            sal_byte_t *p_dst )
{
    * ( sal_i16_t * ) p_dst = POSH_ReadI16FromLittle( kp_src );
}

static 
void 
transform_identity_stereo_16( const sal_byte_t *kp_src,
                              sal_byte_t *p_dst )
{
    * ( sal_i16_t * ) p_dst  = POSH_ReadI16FromLittle( kp_src );
    p_dst += 2; kp_src += 2;
    * ( sal_i16_t * ) p_dst  = POSH_ReadI16FromLittle( kp_src );
}

static
void
transform_mono_16_to_mono_8( const sal_byte_t *kp_src,
                             sal_byte_t *p_dst )
{
    *p_dst = I16_TO_U8( POSH_ReadI16FromLittle( kp_src ) );
}

static
void
transform_mono_8_to_mono_16( const sal_byte_t *kp_src,
                             sal_byte_t *p_dst )
{
    * ( sal_i16_t * ) p_dst = U8_TO_I16( *kp_src );
}

static
void
transform_stereo_8_to_stereo_16( const sal_byte_t *kp_src,
                                 sal_byte_t *p_dst )
{
    * ( sal_i16_t * ) ( p_dst + 0 ) = U8_TO_I16( kp_src[ 0 ] );
    * ( sal_i16_t * ) ( p_dst + 2 ) = U8_TO_I16( kp_src[ 1 ] );
}

static
void
transform_stereo_16_to_stereo_8( const sal_byte_t *kp_src,
                                 sal_byte_t *p_dst )
{
    p_dst[ 0 ] = I16_TO_U8( POSH_ReadI16FromLittle( kp_src ) );
    p_dst[ 1 ] = I16_TO_U8( POSH_ReadI16FromLittle( kp_src + 2 ) );
}

static
void
transform_stereo_16_to_mono_8( const sal_byte_t *kp_src,
                               sal_byte_t *p_dst )
{
    sal_i32_t s16 = ( POSH_ReadI16FromLittle( kp_src ) + POSH_ReadI16FromLittle( kp_src + 2 ) ) / 2;
    *p_dst = I16_TO_U8( s16 );
}

static
void
transform_stereo_16_to_mono_16( const sal_byte_t *kp_src,
                                sal_byte_t *p_dst )
{
    sal_i32_t s16 = ( POSH_ReadI16FromLittle( kp_src ) + POSH_ReadI16FromLittle( kp_src + 2 ) ) / 2;
    *( sal_i16_t * ) p_dst = s16;
}

static
void
transform_stereo_8_to_mono_16( const sal_byte_t *kp_src,
                               sal_byte_t *p_dst )
{
    sal_i16_t l = kp_src[ 0 ];
    sal_i16_t r = kp_src[ 1 ];
    sal_i16_t u = ( l + r ) / 2;

    * ( sal_i16_t * ) p_dst = U8_TO_I16( u );
}

static
void
transform_stereo_8_to_mono_8( const sal_byte_t *kp_src,
                              sal_byte_t *p_dst )
{
    sal_i16_t l = kp_src[ 0 ];
    sal_i16_t r = kp_src[ 1 ];
    sal_i16_t u = ( l + r ) /2;

    *p_dst = ( sal_byte_t ) u;
}

static
void
transform_mono_16_to_stereo_8( const sal_byte_t *kp_src,
                               sal_byte_t *p_dst )
{
    sal_byte_t s8 = I16_TO_U8( POSH_ReadI16FromLittle( kp_src ) );
    p_dst[ 0 ] = s8;
    p_dst[ 1 ] = s8;
}

static
void
transform_mono_8_to_stereo_16( const sal_byte_t *kp_src,
                               sal_byte_t *p_dst )
{
    sal_i16_t s16 = U8_TO_I16( *kp_src );
    * ( sal_i16_t * ) ( p_dst + 0 ) = s16;
    * ( sal_i16_t * ) ( p_dst + 2 ) = s16;
}

/** Decodes a WAV file and creates a sample from it.
    @ingroup extras
    @param [in] device pointer to output device
    @param [out] pp_sample address of a pointer to a sample to store the new sample
    @param [in] kp_src source array of bytes
    @param [in] src_size number of bytes in kp_src
    @returns SALERR_OK on success, @ref sal_error_e on failure
    This function takes a raw stream of bytes and tries to decode it as a 
    WAV file.  It is not particularly robust, handling only very straightforward
    WAV files with simple, uncompressed chunk formats, but it illustrates the
    basics of different sample formats.
*/
sal_error_e
SALx_create_sample_from_wave( SAL_Device *device,
                             SAL_Sample **pp_sample,
                             const void *kp_src,
                             int src_size )
{
    int i;
    int src_frame_size;
    int num_samples;
    SAL_DeviceInfo dinfo;
    _SAL_WaveHeader wh;
    _SAL_WaveChunk  wc;
    const sal_byte_t *kp_bytes = ( const sal_byte_t * ) kp_src;
    sal_byte_t *p_dst = 0;
    void (*transform)( const sal_byte_t *, sal_byte_t * ) = 0;

    if ( device == 0 || pp_sample == 0 || kp_src == 0 || src_size < sizeof( _SAL_WaveHeader ) )
    {
        return SALERR_INVALIDPARAM;
    }

    *pp_sample = 0;

    /* read out wave header */
    memcpy( wh.wh_riff, kp_bytes, 4 );
    kp_bytes += 4;
    wh.wh_size = POSH_ReadU32FromLittle( kp_bytes );
    kp_bytes += 4;
    memcpy( wh.wh_wave, kp_bytes, 4 );
    kp_bytes += 4;
    memcpy( wh.wh_fmt, kp_bytes, 4 );
    kp_bytes += 4;
    wh.wh_chunk_header_size = POSH_ReadU32FromLittle( kp_bytes );
    kp_bytes += 4;

    /* verify that this is a legit WAV file */
    if ( strncmp( wh.wh_riff, "RIFF", 4 ) ||
         strncmp( wh.wh_wave, "WAVE", 4 ) ||
         wh.wh_chunk_header_size != 16 )
    {
        return SALERR_INVALIDPARAM;
    }

    /* read in the chunk */
    wc.wc_tag                = POSH_ReadI16FromLittle( kp_bytes ); kp_bytes += 2;
    wc.wc_num_channels       = POSH_ReadI16FromLittle( kp_bytes ); kp_bytes += 2;
    wc.wc_sample_rate        = POSH_ReadI32FromLittle( kp_bytes ); kp_bytes += 4;
    wc.wc_bytes_per_second   = POSH_ReadI32FromLittle( kp_bytes ); kp_bytes += 4;
    wc.wc_alignment          = POSH_ReadI16FromLittle( kp_bytes ); kp_bytes += 2;
    wc.wc_bits_per_sample    = POSH_ReadI16FromLittle( kp_bytes ); kp_bytes += 2;
    memcpy( wc.wc_data, kp_bytes, 4 ); kp_bytes += 4;
    wc.wc_data_size          = POSH_ReadI32FromLittle( kp_bytes ); kp_bytes += 4;

    src_frame_size  = ( wc.wc_bits_per_sample / 8 ) * wc.wc_num_channels;

    if ( strncmp( wc.wc_data, "data", 4 ) )
    {
        return SALERR_INVALIDPARAM;
    }

    /* get device info */
    memset( &dinfo, 0, sizeof( dinfo ) );
    dinfo.di_size = sizeof( dinfo );
    SAL_get_device_info( device, &dinfo );

    /* We could offer resampling here, but good resampling is beyond the scope of
       this tool */
    if ( dinfo.di_sample_rate != wc.wc_sample_rate )
    {
        _SAL_warning( device, "Sample frequency of %d does not match device's frequency of %d\n", wc.wc_sample_rate, dinfo.di_sample_rate );
        return SALERR_INVALIDFORMAT;
    }

    /* unity transform */
    if ( dinfo.di_channels == wc.wc_num_channels &&
         dinfo.di_bits == wc.wc_bits_per_sample )
    {
        if ( dinfo.di_channels == 1 && dinfo.di_bits == 8 )
        {
            transform = transform_identity_mono_8;
        }
        else if ( dinfo.di_channels == 2 && dinfo.di_bits == 8 )
        {
            transform = transform_identity_stereo_8;
        }
        else if ( dinfo.di_channels == 1 && dinfo.di_bits == 16 )
        {
            transform = transform_identity_mono_16;
        }
        else
        {
            transform = transform_identity_stereo_16;
        }
    }
    /* non-unity transform */
    else
    {
        /* same number of channels */
        if ( dinfo.di_channels == wc.wc_num_channels )
        {
            if ( dinfo.di_channels == 1 )
            {
                if ( dinfo.di_bits == 8 && wc.wc_bits_per_sample == 16 )
                {
                    transform = transform_mono_16_to_mono_8;
                }
                else if ( dinfo.di_bits == 16 && wc.wc_bits_per_sample == 8 )
                {
                    transform = transform_mono_8_to_mono_16;
                }
            }
            else if ( dinfo.di_channels == 2 )
            {
                if ( dinfo.di_bits == 8 && wc.wc_bits_per_sample == 16 )
                {
                    transform = transform_stereo_16_to_stereo_8;
                }
                else if ( dinfo.di_bits == 16 && wc.wc_bits_per_sample == 8 )
                {
                    transform = transform_stereo_8_to_stereo_16;
                }
            }
        }
        /* different number of channels */
        else
        {
            /* stereo to mono conversion */
            if ( dinfo.di_channels == 1 )
            {
                /* with bit conversion */
                if ( dinfo.di_bits != wc.wc_bits_per_sample )
                {
                    if ( dinfo.di_bits == 8 )
                    {
                        transform = transform_stereo_16_to_mono_8;
                    }
                    else if ( dinfo.di_bits == 16 )
                    {
                        transform = transform_stereo_8_to_mono_16;
                    }
                }
                /* without bit conversion */
                else
                {
                    if ( dinfo.di_bits == 8 )
                    {
                        transform = transform_stereo_8_to_mono_8;
                    }
                    else if ( dinfo.di_bits == 16 )
                    {
                        transform = transform_stereo_16_to_mono_16;
                    }
                }
            }
            /* mono to stereo conversion */
            else if ( dinfo.di_channels == 2 )
            {
                /* with bit conversion */
                if ( dinfo.di_bits != wc.wc_bits_per_sample )
                {
                    if ( dinfo.di_bits == 8 )
                    {
                        transform = transform_mono_16_to_stereo_8;
                    }
                    else if ( dinfo.di_bits == 16 )
                    {
                        transform = transform_mono_8_to_stereo_16;
                    }
                }
                /* without bit conversion */
                else
                {
                    if ( dinfo.di_bits == 8 )
                    {
                        transform = transform_mono_8_to_stereo_8;
                    }
                    else if ( dinfo.di_bits == 16 )
                    {
                        transform = transform_mono_16_to_stereo_16;
                    }
                }
            }
        }
    }

    if ( transform == 0 )
    {
        return SALERR_INVALIDFORMAT;
    }

    /* # samples = num_frames * num_channels */
    num_samples = ( wc.wc_data_size / ( wc.wc_bits_per_sample / 8 )) * dinfo.di_channels / wc.wc_num_channels;

    /* allocate new sample and zero it out */
    SAL_create_sample( device, pp_sample, num_samples, _SAL_generic_decode_sample, _SAL_generic_destroy_sample, NULL );

    /* iterate over data, starting at kp_bytes, and transform into the data
       buffer we allocated for this sample, a frame at a time */
    for ( i = 0, p_dst = (*pp_sample)->sample_data; i < wc.wc_data_size; i += src_frame_size, p_dst += dinfo.di_bytes_per_frame )
    {
        transform( &kp_bytes[ i ], p_dst );
    }

    return SALERR_OK;
}
