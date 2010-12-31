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
/** @file sal_sample.c
    @brief Simple Audio File sample functions
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "sal.h"
#include <string.h>

/** @brief Returns the ref count of a sound.
    @param[in] p_device pointer to output device
    @param[in] p_sample sample from which to retrieve the ref count
    @param[out] p_count pointer to integer to store the ref count
    @ingroup SampleManagement
    @returns SALERR_OK on success, @ref sal_error_e otherwise
    Samples are ref-counted.  When created their initial ref count is 1, and
    when destroyed the ref count is decremented.  Every time a sample is played
    its ref count increases by one, and every time a voice has completed, it
    decrements the ref count by one.  Samples are never destroyed unless all
    voices referencing the sample have stopped and 
*/
sal_error_e 
SAL_get_sample_ref_count( SAL_Device *p_device, const SAL_Sample *p_sample, sal_i32_t *p_count )
{
    if ( p_device == 0 || p_sample == 0 || p_count == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    /* lock the device */
    _SAL_lock_device( p_device );

    /* get the sample's ref count */
    *p_count = p_sample->sample_ref_count;

    /* unlock the device */
    _SAL_unlock_device( p_device );

    return SALERR_OK;

}

/** @internal
    @brief Frees memory/destroys a sample, assuming that the ref count is already 0
    @param[in] p_device pointer to output device
    @param[in] p_sample pointer to sample to destroy
    @remarks This assumes the device is already locked.
*/
void
_SAL_destroy_sample_raw( SAL_Device *p_device,
                         SAL_Sample *p_sample )
{
    p_sample->sample_fnc_destroy( p_device, p_sample );
    p_device->device_callbacks.free( p_sample );
}

/** @brief Destroys a sample, assuming its ref count is 0.
    @ingroup SampleManagement
    @param[in] p_device
    @param[in] p_sample pointer to sample to destroy
    @retval SALERR_OK on success
    @retval SALERR_INUSE if the sample is still being used
    @retval @ref sal_error_e on failure
*/
sal_error_e
SAL_destroy_sample( SAL_Device *p_device,
                    SAL_Sample *p_sample )
{
    if ( p_device == 0 || p_sample == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    /* lock access to the device */
    _SAL_lock_device( p_device );

    /* decrement ref count */
    p_sample->sample_ref_count--;

    /* destroy the sample if possible */
    if ( p_sample->sample_ref_count <= 0 )
    {
        _SAL_destroy_sample_raw( p_device, p_sample );

        /* unlock the device */
		_SAL_unlock_device( p_device );
        return SALERR_OK;
    }

    /* unlock the device */
    _SAL_unlock_device( p_device );

    return SALERR_INUSE;
}

/**@internal
   @brief Generic sound decoding function that does a direct copy from a source
   buffer to a destination buffer (e.g. for PCM data).
   @ingroup SampleManagement
   @param[in] p_device pointer to output device
   @param[in] voice   voice we are decoding from
   @param[out] p_dst   destination buffer we're decoding to
   @param[in] bytes_needed the number of bytes we're requested to fill into p_dst
   @returns 1 if the voice has ended (i.e. sample has played out), 0 if not
   @remarks This should never be called directly by an application.
   This is a generic PCM sample decoder that copies the requested number 
   of bytes directly into the target.  It is assumed that all formats
   match.   This function is intended to be a callback function for
   the SAL_Sample structure.
*/
int 
_SAL_generic_decode_sample( SAL_Device *p_device, 
                            sal_voice_t voice,
                            sal_byte_t *p_dst, 
                            int bytes_needed )
{
    int samples_needed;
    int i;
    sal_byte_t *sample_data;
    SAL_Sample *sample;
    int cursor;

    /* NOTE: we don't need to lock the device since this should be called from the mixer,
       which locks the device for us */
    samples_needed = bytes_needed / p_device->device_info.di_bytes_per_sample;

    SAL_get_voice_sample( p_device, voice, &sample );
    SAL_get_sample_data( p_device, sample, &sample_data );

    /* This code assumes that the source sample format matches the output 
       device's format exactly */
    if ( p_device->device_info.di_bits == 8 )
    {
        for ( i = 0; i < samples_needed; i++ )
        {
            SAL_get_voice_cursor( p_device, voice, &cursor );
            p_dst[ i ] = sample_data[ cursor ];
            if ( !_SAL_advance_voice( p_device, voice, 1 ) )
            {
                return 1;
            }
        }
    }
    else if ( p_device->device_info.di_bits == 16 )
    {
        const sal_i16_t *kp_src = ( const sal_i16_t * ) sample_data;
		sal_i16_t *dst16 = ( sal_i16_t * ) p_dst;
		
        for ( i = 0; i < samples_needed; i++)
        {
            SAL_get_voice_cursor( p_device, voice, &cursor );
            dst16[i] = kp_src[ cursor ];

            if ( !_SAL_advance_voice( p_device, voice, 1 ) )
            {
                return 1;
            }
        }
    }

    return 0;
}

/** @internal
    @ingroup SampleManagement
    @brief Generic sample destruction callback
    @param[in] p_device pointer to output device
    @param[in] self pointer to sample
    This routine is used as the default sample destruction callback
    function.  It should never be called directly by an application.
*/
void
_SAL_generic_destroy_sample( SAL_Device *p_device, SAL_Sample *self )
{
    if ( self->sample_data )
    {
        p_device->device_callbacks.free( self->sample_data );
        self->sample_data = 0;
    }
}

/** @brief Return the raw sample data associated with this sample.
    @ingroup SampleManagement
    @param[in] p_device pointer to output device
    @param[in] p_sample pointer to sample we wish to retrieve sample data from
    @param[out] pp_bytes address of pointer for the sample data
    @returns SALERR_OK on success, @ref sal_error_e on failure
*/
sal_error_e 
SAL_get_sample_data( SAL_Device *p_device, SAL_Sample *p_sample, sal_byte_t **pp_bytes )
{
    if ( p_device == 0 || p_sample == 0 || pp_bytes == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    *pp_bytes = p_sample->sample_data;

    return SALERR_OK;
}

/** @brief Creates a "raw" sample suitable for filling in by the application.
    @ingroup SampleManagement
    @param[in] p_device pointer to output device
    @param[out] pp_sample address of pointer to sample to store output in
    @param[in] num_samples number of samples that we should be looking at
    @param[in] decoder decode function to use when the mixer needs new data
    @param[in] destroy destruction function to use when sample is destroyed
    @param[in] args pointer to a SAL_SampleArgs structure to associate with this 
    sample.  The contents are copied, so the pointer does not need to be persistent
    with the sample.  This parameter may be NULL.
    @remarks Use SAL_get_sample_data() to modify the PCM data directly
*/
sal_error_e 
SAL_create_sample( SAL_Device *p_device, 
                   SAL_Sample **pp_sample, 
                   size_t num_samples,
                   sal_sample_decode_fnc_t decoder,
                   sal_sample_destroy_fnc_t destroy,
 				   SAL_SampleArgs *args )
{
    SAL_Sample *p_sample;
    SAL_DeviceInfo dinfo;

    if ( p_device == 0 || pp_sample == 0 || decoder == 0 || destroy == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    /* Get device's information */
    memset( &dinfo, 0, sizeof( dinfo ) );
    dinfo.di_size = sizeof( dinfo );
    SAL_get_device_info( p_device, &dinfo );

    p_sample = ( SAL_Sample * ) p_device->device_callbacks.alloc( sizeof( *p_sample ) );
    memset( p_sample, 0, sizeof( *p_sample ) );
    p_sample->sample_num_samples = num_samples;
    p_sample->sample_fnc_decoder = decoder;
    p_sample->sample_fnc_destroy = destroy;
    p_sample->sample_ref_count = 1;

    if ( num_samples > 0 )
    {
        p_sample->sample_data = ( sal_byte_t * ) p_device->device_callbacks.alloc( num_samples * dinfo.di_bytes_per_sample );
    }

    if ( args )
    {
        p_sample->sample_args = *args;
    }

    *pp_sample = p_sample;

    return SALERR_OK;
}

/** @brief Returns the SAL_SampleArgs structure associated with the sample
    @ingroup SampleManagement
    @param[in] p_device pointer to output device
    @param[in] p_sample pointer to sample from which to retrieve args
    @param[out] args pointer to a SAL_SampleArgs structure in which to store the args
    @sa SAL_create_sample
    @returns SALERR_OK on success, @ref sal_error_e on failure
*/
sal_error_e 
SAL_get_sample_args( SAL_Device *p_device, 
                     const SAL_Sample *p_sample, 
                     SAL_SampleArgs *args )
{
    if ( p_device == 0 || p_sample == 0 || args == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    *args = p_sample->sample_args;

    return SALERR_OK;
}
