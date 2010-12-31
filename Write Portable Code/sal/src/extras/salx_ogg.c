/*
LICENSE:

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
/** @file salx_ogg.c
    @brief SAL extra Ogg Vorbis support
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "../sal.h"
#include "salx_ogg.h"

#include <vorbis/vorbisfile.h>
#include <string.h>

/** @internal
*/
typedef struct
{
    OggVorbis_File    oa_file;       /**< ogg vorbis file structure */
    sal_byte_t       *oa_buffer;     /**< pointer to compressed data */
    sal_i64_t         oa_cursor;     /**< current cursor in the compressed data */
    int               oa_size;       /**< size of the buffer */
    int               oa_num_channels; /**< number of channels */
    int               oa_sample_rate;  /**< sample rate */
    int               oa_section;      /**< current section */
} SALx_OggArgs;;

static
size_t
s_ogg_read( void *ptr, size_t size, size_t nmemb, void *datasource)
{
    SALx_OggArgs *ogg_args = ( SALx_OggArgs * ) datasource;
    sal_i64_t bytes_left = ogg_args->oa_size - ogg_args->oa_cursor;
    sal_i64_t bytes_to_read = size * nmemb;

    bytes_to_read = bytes_to_read > bytes_left ? bytes_left : bytes_to_read;

    memcpy( ptr, &ogg_args->oa_buffer[ ogg_args->oa_cursor ], (unsigned)bytes_to_read );

    ogg_args->oa_cursor += bytes_to_read;

    if ( ogg_args->oa_cursor > ogg_args->oa_size )
    {
        ogg_args->oa_cursor = ogg_args->oa_size;
    }

    return (unsigned)(bytes_to_read / size);
}

static
int
s_ogg_close( void *ptr )
{
    ptr = ptr;
    return 0;
}

static
int     
s_ogg_seek( void *datasource, ogg_int64_t offset, int whence )
{
    SALx_OggArgs *ogg_args = ( SALx_OggArgs * ) datasource;

    switch( whence )
    {
    case SEEK_CUR:
        {
            ogg_args->oa_cursor += offset;
            if ( ogg_args->oa_cursor > ogg_args->oa_size )
            {
                ogg_args->oa_cursor = ogg_args->oa_size;
                return -1;
            }
        }
        break;

    case SEEK_SET:
        {
            ogg_args->oa_cursor = offset;

            if ( ogg_args->oa_cursor > ogg_args->oa_size )
            {
                ogg_args->oa_cursor = ogg_args->oa_size;
                return -1;
            }
        }
        break;

    case SEEK_END:
        {
            ogg_args->oa_cursor = ogg_args->oa_size - offset;

            if ( ogg_args->oa_cursor < 0 )
            {
                ogg_args->oa_cursor = 0;
                return -1;
            }
        }
        break;
    }

    return 0;
}

static
long
s_ogg_tell( void *datasource )
{
    SALx_OggArgs *ogg_args = ( SALx_OggArgs * ) datasource;

    return (long)(ogg_args->oa_cursor);
}

static
int
s_ogg_decoder( SAL_Device *p_device, 
               sal_voice_t voice,
               sal_byte_t *p_dst, 
               int bytes_needed )
{
    int bytes_read = 0;
    SAL_Sample *sample = 0;
    SALx_OggArgs *ogg_args = 0;
    int big_endian = 0;
    int cursor = 0;

#if POSH_BIG_ENDIAN
    big_endian = 1;
#endif

    SAL_get_voice_sample( p_device, voice, &sample );
    SAL_get_voice_cursor( p_device, voice, &cursor );

    ogg_args = ( SALx_OggArgs * ) sample->sample_args.sarg_ptr;

    /* first seek to the voice's sample */
    ov_pcm_seek( &ogg_args->oa_file, cursor );

    /* then read stuff out */
    while ( bytes_read < bytes_needed )
    {
        long lret;

        lret = ov_read( &ogg_args->oa_file,         /* vorbis file */
                        p_dst + bytes_read,         /* destination buffer */
                        bytes_needed - bytes_read,  /* size of the destination buffer*/
                        big_endian,                 /* endianess, 0 == little, 1 == big */
                        2,                          /* bytes per sample */
                        1,                          /* 0 == unsigned, 1 == signed */
                        &ogg_args->oa_section );    /* pointer to number of current logical bitstream */

        if ( lret == 0 )
        {
            /* rewind the stream */
            ov_pcm_seek( &ogg_args->oa_file, 0 );
        }
        else if ( lret < 0 )
        {
            /* error */
            break;
        } 
        else 
        {
            bytes_read += lret;

             /** @todo update to handle different size samples */
            if ( !_SAL_advance_voice( p_device, voice, lret / ( ogg_args->oa_num_channels * 2 ) ) )
            {
                return 1;
            }
        }
    }

    return 0;
}

static
void
s_ogg_destructor( SAL_Device *p_device, 
                  SAL_Sample *self )
{
    SALx_OggArgs *ogg_args = ( SALx_OggArgs * ) self->sample_args.sarg_ptr;

    ov_clear( &ogg_args->oa_file );
}

/** Creates a sample that decodes from an in-memory Ogg image.
    @ingroup extras
    @param [in] device pointer to output device
    @param [out] pp_sample address of a pointer to a sample to store the new sample
    @param [in] kp_src source array of bytes.  This data is copied.
    @param [in] src_size number of bytes in kp_src
    @returns SALERR_OK on success, @ref sal_error_e on failure
    This function takes a raw stream of bytes and decodes it on the fly as an
    Ogg stream (it does not decompress all at once).  Currently
    the Ogg stream must match formats with the device to work.
*/
sal_error_e 
SALx_create_sample_from_ogg( SAL_Device *device,
                             SAL_Sample **pp_sample,
                             const void *kp_src,
                             int src_size )
{
    ov_callbacks ogg_callbacks = 
    {
        s_ogg_read,
        s_ogg_seek,
        s_ogg_close,
        s_ogg_tell
    };

    vorbis_info *vi = 0;
    SAL_Sample *p_sample = 0;
    SAL_SampleArgs args;
    sal_error_e err;
    SALx_OggArgs *p_ogg_args = 0;
    SAL_DeviceInfo dinfo;

    if ( ( err = SAL_alloc( device, &p_ogg_args, sizeof( *p_ogg_args ) ) ) != SALERR_OK )
    {
        return err;
    }

    memset( &dinfo, 0, sizeof( dinfo ) );
    dinfo.di_size = sizeof( dinfo );
    SAL_get_device_info( device, &dinfo );

    memset( p_ogg_args, 0, sizeof( *p_ogg_args ) );

    args.sarg_ptr = p_ogg_args;

    if ( ( err = SAL_alloc( device, &p_ogg_args->oa_buffer, src_size ) ) != SALERR_OK )
    {
        SAL_free( device, p_ogg_args );
        return err;
    }
    p_ogg_args->oa_size   = src_size;

    memcpy( p_ogg_args->oa_buffer, kp_src, src_size );

    
    if ( ( err = SAL_create_sample( device, &p_sample, 0, s_ogg_decoder, s_ogg_destructor, &args ) ) != SALERR_OK )
    {
        SAL_free( device, p_ogg_args );
        return err;
    }

    if ( ov_open_callbacks( p_ogg_args, &p_ogg_args->oa_file, NULL, 0, ogg_callbacks ) < 0 )
    {
        SAL_free( device, p_ogg_args );
        return SALERR_SYSTEMFAILURE;
    }
    
    vi = ov_info( &p_ogg_args->oa_file, -1 );

    p_ogg_args->oa_num_channels = vi->channels;
    p_ogg_args->oa_sample_rate  = vi->rate;

    if ( p_ogg_args->oa_sample_rate != dinfo.di_sample_rate ||
         p_ogg_args->oa_num_channels != dinfo.di_channels )
    {
        ov_clear( &p_ogg_args->oa_file );
        SAL_free( device, p_ogg_args );
        return SALERR_INVALIDFORMAT;
    }

    *pp_sample = p_sample;

    return SALERR_OK;
}
