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
/** @file sal_private.h
    @brief Private header file for Simple Audio Library
*/
#ifndef SAL_PRIVATE_H
#define SAL_PRIVATE_H

/*
** ----------------------------------------------------------------------------
** Helper macros and constants
** ----------------------------------------------------------------------------
*/
/** @internal
    Converts unsigned 8-bit value to signed 16-bit integer */
#define U8_TO_I16( b ) (sal_i16_t)  ( ( ( ( (sal_u16_t) (b) ) <<8 ) | (b) )-32768)
/** @internal
    Converts signed 16-bit integer to unsigned 8-bit value */
#define I16_TO_U8( i ) (sal_byte_t) ( ( ( (sal_i32_t)(i)) >> 8 ) + 0x80 )

#ifdef _MSC_VER
#define snprintf _snprintf
#define vsnprint _vsnprintf
#endif

#define DEFAULT_AUDIO_BITS        16         /**< default bits per sample */
#define DEFAULT_AUDIO_CHANNELS    2          /**< default number of channels */
#define DEFAULT_AUDIO_SAMPLE_RATE 44100      /**< default sample rate */
#define DEFAULT_BUFFER_DURATION   50         /**< default buffer length in milliseconds */

/*
** ----------------------------------------------------------------------------
** Internal types
** ----------------------------------------------------------------------------
*/
/** @internal
 */
typedef void *sal_mutex_t; /**< mutex used for interthread synchronization */

/** @internal 
    @brief Internal data structure used to keep track of a playing voice's state
*/
typedef struct SAL_Voice_s
{
    struct SAL_Sample_s *voice_sample;       /**< pointer to voice sample */
    sal_u32_t    voice_cursor;               /**< cursor into sample data, in samples (NOT in bytes) */
    sal_volume_t voice_volume;               /**< voice volume, from 0 to 65535 */
    sal_pan_t    voice_pan;                  /**< voice pan, from -32768 (far left) to +32767 (far right) */
    sal_u32_t    voice_loop_start;           /**< loop start position, default is 0 */
    sal_u32_t    voice_loop_end;             /**< loop end position, default is 0 which indicates end of sample */
    sal_i32_t    voice_num_repetitions;      /**< number of times to repeat.  A value of @ref SAL_LOOP_ALWAYS means indefinite */
} SAL_Voice;

typedef void ( POSH_CDECL *SAL_THREAD_FUNC)( void *args ); /**< function pointer type passed to _SAL_create_thread() */

/** @internal 
    @brief Internal data structure used to keep track of a sound device's state */
typedef struct SAL_Device_s
{
    SAL_Callbacks   device_callbacks;          /**< callbacks registered with the device at creation time */
    sal_mutex_t     device_mutex;              /**< used to control access to the device */
    void           *device_data;               /**< system specific data */
    SAL_DeviceInfo  device_info;               /**< information about the device */

    struct SAL_Voice_s  *device_voices;        /**< array of voice entries */
    int                  device_max_voices;    /**< maximum number of simultaneous voices playing */

    /** @defgroup ImplementationCallbacks Implementation Callbacks
        @ingroup Implementations
        @brief Function pointers that provide the raw platform specific implementations
        @{
    */
    sal_error_e   (*device_fnc_create_mutex)( struct SAL_Device_s *device, sal_mutex_t *p_mtx ); /**< creates a mutex */
    sal_error_e   (*device_fnc_destroy_mutex)( struct SAL_Device_s *device, sal_mutex_t mtx );   /**< destroys a mutex */
    sal_error_e   (*device_fnc_lock_mutex)( struct SAL_Device_s *device, sal_mutex_t mtx );      /**< locks a mutex */
    sal_error_e   (*device_fnc_unlock_mutex)( struct SAL_Device_s *device, sal_mutex_t mtx );    /**< unlocks a mutex */
    sal_error_e   (*device_fnc_create_thread)( struct SAL_Device_s *device, SAL_THREAD_FUNC fnc, void *targs ); /**< creates a thread */
    sal_error_e   (*device_fnc_sleep)( struct SAL_Device_s *device, sal_u32_t duration );        /**< sleeps for the specified duration in milliseconds */
    /** @} */

    void          (*device_fnc_destroy)( struct SAL_Device_s *d ); /**< pointer to device destruction function */
} SAL_Device;

/** Sample destruction callback function registered with SAL_create_sample() 
    @param p_device[in] pointer to output device 
    @param self[in] pointer to sample to destroy
*/
typedef void (*sal_sample_destroy_fnc_t)( SAL_Device *p_device, struct SAL_Sample_s *self );
/** Sample decode callback function registered with SAL_create_sample() 
    @param p_device[in] pointer to output device
    @param voice[in] voice that is being decoded 
    @param p_dst[out] destination buffer for decoding 
    @param dst_stride_bytes[in] stride of destination between samples 
    @param bytes_needed[in] number of bytes we need to decode
*/
typedef int (*sal_sample_decode_fnc_t)( SAL_Device *p_device, sal_voice_t voice, sal_byte_t *p_dst, int bytes_needed );

/** @internal
    @brief Internal data structure used to keep track of a sample's state */
typedef struct SAL_Sample_s
{
    sal_i32_t   sample_ref_count;      /**< ref count, when it drops to 0 it may be destroyed */
    sal_byte_t *sample_data;           /**< raw sample data */
    sal_i32_t   sample_num_samples;    /**< number of samples in sample_data */

    sal_sample_destroy_fnc_t sample_fnc_destroy;  /**< function used to destroy the sample */
    sal_sample_decode_fnc_t  sample_fnc_decoder;  /**< function used to decode a chunk from the sample */

	SAL_SampleArgs           sample_args;         /**< arguments specified during SAL_create_sample */
} SAL_Sample;

/*
** ----------------------------------------------------------------------------
** Internal APIs
** ----------------------------------------------------------------------------
*/
sal_error_e _SAL_mix_chunk( SAL_Device *device, sal_byte_t *p_dst, sal_u32_t u_bytes_to_mix );
void        _SAL_destroy_sample_raw( SAL_Device *p_device, SAL_Sample *p_sample );

/*
** ----------------------------------------------------------------------------
** Internal APIs for multithreading
** ----------------------------------------------------------------------------
*/
sal_error_e _SAL_create_thread( SAL_Device *device, SAL_THREAD_FUNC fnc, void *targs );
sal_error_e _SAL_create_mutex( SAL_Device *device, sal_mutex_t *p_mutex );
sal_error_e _SAL_destroy_mutex( SAL_Device *device, sal_mutex_t mutex );
sal_error_e _SAL_lock_mutex( SAL_Device *device, sal_mutex_t mutex );
sal_error_e _SAL_unlock_mutex( SAL_Device *device, sal_mutex_t mutex );

/*
** ----------------------------------------------------------------------------
** Internal APIs for other system specific stuff
** ----------------------------------------------------------------------------
*/
/** @internal
    @brief System-dependent implementation that creates a device's underlying implementation data
    @param[in] device output device being created
    @param[in] kp_sp pointer to system parameters
    @param[in] desired_channels    number of desired channels, may use 0 to have the subsystem choose
    @param[in] desired_bits        number of desired bits per sample, may use 0 to have the subsystem choose
    @param[in] desired_sample_rate desired sample rate, in samples/second, may use 0 to have the subsystem choose
    @returns SALERR_OK on success, @ref sal_error_e on failure
    Each operating system implementation implements this function, which is responsible for creating all the
    underlying OS data.  Sometimes this just dispatches to another function depending on options (e.g. on Linux
    the actual implementation will call either the ALSA or OSS implementations).
*/
sal_error_e _SAL_create_device_data( SAL_Device *device, 
                                     const SAL_SystemParameters *kp_sp, 
                                     sal_u32_t desired_channels, 
                                     sal_u32_t desired_bits, 
                                     sal_u32_t desired_sample_rate );

sal_error_e _SAL_lock_device( SAL_Device *device );
sal_error_e _SAL_unlock_device( SAL_Device *device );

/*
** ----------------------------------------------------------------------------
** Error handling
** ----------------------------------------------------------------------------
*/
sal_error_e _SAL_warning( SAL_Device *device, const char *kp_fmt, ... );
sal_error_e _SAL_error( SAL_Device *device, const char *kp_fmt, ... );

#endif /* SAL_PRIVATE_H */
