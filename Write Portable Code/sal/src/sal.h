/*
Copyright (c) 2004, Brian Hook
All rights reserved.

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

/** @file sal.h
    @author Brian Hook, http://www.bookofhook.com
    @brief Simple Audio Library header file
*/
#ifndef SAL_H
#define SAL_H

/*
** SAL is dependent on the Portable Open Source Harness, posh.h, which
** handles a lot of platform configuration funkiness for us.  In order
** for posh.h to work, we need to make sure that we're correctly
** specifying POSH_DLL and POSH_BUILDING_LIB correctly.  The application
** build tools should be specifying the former, and the latter is
** handled indirectly by looking at SAL_BUILDING_LIB.
*/
#ifdef SAL_DLL
#  define POSH_DLL
#else
#  undef POSH_DLL
#endif

#ifdef SAL_BUILDING_LIB
#  define POSH_BUILDING_LIB
#else
#  undef POSH_BUILDING_LIB
#endif

#include <stddef.h>
#include <stdio.h>
#include "posh.h"

#ifdef SAL_DOXYGEN
/**     Defined if we're building the library, synonym for POSH_BUILDING_LIB and used
        to control POSH_PUBLIC_API/SAL_PUBLIC_API when building as a dynamic library. */
#  define SAL_BUILDING_LIB 1 
#  define SAL_PUBLIC_API(x) x 
#  else
#  define SAL_PUBLIC_API POSH_PUBLIC_API  
#endif

/** @def SAL_PUBLIC_API
    A synonym for POSH_PUBLIC_API which allows us to wrap our function
	prototypes so that they properly export for dynamic lib builds
*/

#if defined __cplusplus
extern "C" {
#endif

/*
** ----------------------------------------------------------------------------
** type definitions
** ----------------------------------------------------------------------------
*/

/** Standard error codes returned by most SAL functions */
typedef enum
{
    SALERR_OK              = 0x0000,       /**< everything is ok */

    /* 0x0001 - 0x00FF are generic errors */
    SALERR_INVALIDPARAM    = 0x0001,       /**< one of the parameters was invalid */
    SALERR_WRONGVERSION    = 0x0002,       /**< incompatible versions being used */

    SALERR_OUTOFMEMORY     = 0x0003,       /**< an allocation failed */
    SALERR_SYSTEMFAILURE   = 0x0004,       /**< a call to the underlying OS API failed */
    SALERR_ALREADYLOCKED   = 0x0005,       /**< tried to lock an already locked mutex */
    SALERR_INUSE           = 0x0006,       /**< attempted to destroy object that is currently in use */
    SALERR_INVALIDFORMAT   = 0x0007,       /**< mismatched sample formats */

    /* 0x0100 - 0x01FF = transient errors/warnings */
    SALERR_OUTOFVOICES     = 0x0101,       /**< out of voices */

    /* 0x1000+ are internal errors */
    SALERR_UNIMPLEMENTED   = 0x1000,       /**< feature unimplemented */

    SALERR_UNKNOWN         = 0xFFFF        /**< unknown error */
} sal_error_e;

/** voice status constant as returned by SAL_get_voice_status */
typedef enum
{
    SALVS_IDLE,                 /**< voice is done playing */
    SALVS_PLAYING,              /**< voice is currently playing */
    SALVS_INVALIDSOUND          /**< illegal voice handle */
} sal_voice_status_e;

typedef posh_byte_t sal_byte_t;   /**< unsigned 8-bit type */
typedef posh_i16_t  sal_i16_t;    /**< signed 16-bit type */
typedef posh_u16_t  sal_u16_t;    /**< unsigned 16-bit type */
typedef posh_i32_t  sal_i32_t;    /**< signed 32-bit type */
typedef posh_u32_t  sal_u32_t;    /**< unsigned 32-bit type */
typedef posh_i64_t  sal_i64_t;    /**< signed 32-bit integer */
typedef posh_u64_t  sal_u64_t;    /**< unsigned 32-bit integer */

typedef sal_i32_t   sal_voice_t;  /**< handle used for a playing sound */
typedef sal_u16_t   sal_volume_t; /**< volume parameter, from 0 to 65535 */
typedef sal_i16_t   sal_pan_t;    /**< pan parameter, from -32768 (left) to 32767 (right) */

/*
** ----------------------------------------------------------------------------
** constants
** ----------------------------------------------------------------------------
*/
#define SAL_INVALID_SOUND  -1       /**< default value for a sal_voice_t to indicate it's not valid */
#define SAL_LOOP_ALWAYS    -1       /**< passed as num_repetitions parameter to SAL_play_voice() to loop always */

#define SAL_PAN_HARD_LEFT  -32767   /**< constant for panning to the hard left.  We could use SHRT_MIN instead */
#define SAL_PAN_HARD_RIGHT  32767   /**< constant for panning to the hard right. */
#define SAL_VOLUME_MIN      0       /**< constant for minimum volume */
#define SAL_VOLUME_MAX      65535   /**< constant for maximum volume */

#define SAL_VERSION 0x00010000      /**< version of this library, in format 0xMMMMmmpp where MMMM = major version,
                                       mm = minor version, and pp = patch level.  Check against SAL_get_version */

/*
** ----------------------------------------------------------------------------
** structs 
** ----------------------------------------------------------------------------
*/

/** @brief Optional callback structure passed to SAL_create_device that defines
    user defined callback procedures for memory allocation, free, warning
    output, and error output/handling.
*/
typedef struct SAL_Callbacks
{
    sal_i32_t cb_size;                    /**< size of callback structure */

    void * (POSH_CDECL *alloc)( sal_u32_t sz );       /**< memory allocation function, must allocate sz bytes */
    void   (POSH_CDECL *free)( void *p );             /**< memory free function */

    void   (POSH_CDECL *warning)( const char *msg );   /**< warning/general output function */
    void   (POSH_CDECL *error)( const char *msg );     /**< error handling/output funciton */
} SAL_Callbacks;

#define SAL_DEVICEINFO_MAX_NAME 256  /**< Maximum length of a device's name */

/** @brief Device information structure, retrieved by calling SAL_get_device_info */
typedef struct SAL_DeviceInfo_s
{
    sal_i32_t   di_size;                       /**< size of the device info structure */
    sal_i32_t   di_channels;                   /**< number of output channels supported by this device */
    sal_i32_t   di_bits;                       /**< bits per sample */
    sal_i32_t   di_sample_rate;                /**< in frames/second */
    sal_i32_t   di_bytes_per_sample;           /**< a sample is a single sample on one channel */
    sal_i32_t   di_bytes_per_frame;            /**< a frame consists of samples on all channels for one time slice */
    char        di_name[ SAL_DEVICEINFO_MAX_NAME ]; /**< name of the device */
} SAL_DeviceInfo;

/* The system parameter flags are divided into four groups of eight bits
   each:

   bits  0 - 15:  general configuration
   bits 16 - 31:  OS specific configuration
*/
/** @defgroup SPF System Parameter Flags
    @ingroup DeviceManagement
    @{
*/
#define SAL_SPF_WAVEOUT   0x00010000         /**< Windows only (default is DSOUND) */
#define SAL_SPF_ALSA      0x00010000         /**< Linux only (default is OSS) */
/** @} */

/** 
    @brief The Win32 system parameters structure.
*/
struct SAL_SystemParametersWin32
{
    sal_i32_t   sp_size; /**< size of the system parameters structure */
    sal_u32_t   sp_flags; /**< miscellaneous flags, as defined at @ref SPF*/
    sal_i32_t   sp_buffer_length_ms; /**< length of the buffer, in milliseconds */

    void       *sp_hWnd; /**< pointer to HWND */
};

/** 
    @brief The default system parameters structure, used by Linux and OS X .
*/
struct SAL_SystemParametersDefault
{
    sal_i32_t   sp_size; /**< size of the system parameters structure */
    sal_u32_t   sp_flags; /**< miscellaneous flags, as defined at @ref SPF*/
    sal_i32_t   sp_buffer_length_ms; /**< length of the buffer, in milliseconds -- used by OSS */
};

#ifdef POSH_OS_WIN32 
typedef struct SAL_SystemParametersWin32 SAL_SystemParameters;
#else
typedef struct SAL_SystemParametersDefault SAL_SystemParameters;
#endif

/** @typedef struct SAL_SystemParametersDefault SAL_SystemParameters
    @brief Creates a synonym for the SAL_SystemParameters and the proper underlying system
    parameter structure for a particular platform.
*/

/** @brief Arguments bound to SAL_Sample during SAL_create_sample
 */
typedef union SAL_SampleArgs_u
{
    void       *sarg_ptr;         /**< pointer to user data */
    sal_i32_t   sarg_int32;       /**< arbitrary 32-bit integer */
    sal_i64_t   sarg_int64;       /**< arbitrary 64-bit integer */
} SAL_SampleArgs;

/*
** This chunk of code is used to hide the underlying implementation
** of the device and sample structures.  When building an application,
** an opaque stand in is used to define the structures, but when
** building the library we include "sal_private.h" which defines them
** explicitly.
*/
#if !defined SAL_BUILDING_LIB

struct SAL_opaque 
{ 
    int dummy; 
};

typedef struct SAL_opaque SAL_Device;
typedef struct SAL_opaque SAL_Sample;

typedef void (POSH_CDECL * sal_sample_destroy_fnc_t)( SAL_Device *p_device, SAL_Sample *self );
typedef int  (POSH_CDECL * sal_sample_decode_fnc_t)( SAL_Device *p_device, sal_voice_t voice, sal_byte_t *p_dst, int bytes_needed );

#endif

#ifdef SAL_BUILDING_LIB

#include "sal_private.h"

#endif

/*
** ----------------------------------------------------------------------------
** function prototypes
** ----------------------------------------------------------------------------
*/

/* Device management */
SAL_PUBLIC_API( sal_error_e )  SAL_create_device( SAL_Device **pp_device, 
                                                  const SAL_Callbacks *kp_cb, 
                                                  const SAL_SystemParameters *kp_sp, 
                                                  sal_u32_t desired_channels,
                                                  sal_u32_t desired_bits,
                                                  sal_u32_t desired_sample_rate,
                                                  sal_u32_t num_voices );
SAL_PUBLIC_API( sal_error_e )  SAL_destroy_device( SAL_Device *p_device );
SAL_PUBLIC_API( sal_error_e )  SAL_get_device_info( SAL_Device *p_device,
                                                    SAL_DeviceInfo *p_info );

/* Sample management */ 
SAL_PUBLIC_API( sal_error_e )  SAL_create_sample( SAL_Device *p_device, 
                                                  SAL_Sample **pp_sample, 
                                                  size_t num_samples, 
                                                  sal_sample_decode_fnc_t decoder, 
                                                  sal_sample_destroy_fnc_t destroyer,
                                                  SAL_SampleArgs *p_sample_args );
SAL_PUBLIC_API( sal_error_e )  SAL_destroy_sample( SAL_Device *p_device, SAL_Sample *p_sample );
SAL_PUBLIC_API( sal_error_e )  SAL_get_sample_ref_count( SAL_Device *p_device, const SAL_Sample *p_sample, sal_i32_t *p_count );
SAL_PUBLIC_API( sal_error_e )  SAL_get_sample_data( SAL_Device *p_device, SAL_Sample *p_sample, sal_byte_t **pp_bytes );
SAL_PUBLIC_API( sal_error_e )  SAL_get_sample_args( SAL_Device *p_device, const SAL_Sample *p_sample, SAL_SampleArgs *args );

SAL_PUBLIC_API( int )          _SAL_advance_voice( SAL_Device *device, sal_voice_t voice, int num_frames );
SAL_PUBLIC_API( int )          _SAL_generic_decode_sample( SAL_Device *p_device, 
                                                           sal_voice_t voice,
                                                           sal_byte_t *p_dst, 
                                                           int bytes_needed );
SAL_PUBLIC_API( void )         _SAL_generic_destroy_sample( SAL_Device *p_device, SAL_Sample *self );

/* Sound management */
SAL_PUBLIC_API( sal_error_e )  SAL_play_sample( SAL_Device *p_device, 
                                                SAL_Sample *p_sample, 
                                                sal_voice_t *p_sid,
                                                sal_volume_t volume,
                                                sal_pan_t pan,
                                                sal_u32_t loop_start,
                                                sal_u32_t loop_end,
                                                sal_i32_t num_repetitions );
SAL_PUBLIC_API( sal_error_e )  SAL_stop_voice( SAL_Device *p_device, 
                                               sal_voice_t sid );
SAL_PUBLIC_API( sal_error_e )  SAL_set_voice_volume( SAL_Device *p_device,
                                                     sal_voice_t p_vid,
                                                     sal_volume_t volume );
SAL_PUBLIC_API( sal_error_e )  SAL_set_voice_pan( SAL_Device *p_device,
                                                  sal_voice_t p_vid,
                                                  sal_pan_t   pan );
SAL_PUBLIC_API( sal_error_e )  SAL_get_voice_status( SAL_Device *p_device, 
                                                     sal_voice_t sid,
                                                     sal_voice_status_e *p_status );
SAL_PUBLIC_API( sal_error_e )  SAL_get_voice_sample( SAL_Device *p_device,
                                                     sal_voice_t sid,
                                                     SAL_Sample **pp_sample );
SAL_PUBLIC_API( sal_error_e )  SAL_get_voice_cursor( SAL_Device *p_device,
                                                     sal_voice_t sid,
                                                     int *p_cursor );

/* Utility */
SAL_PUBLIC_API( sal_u32_t )   SAL_get_version( void );
SAL_PUBLIC_API( sal_error_e ) SAL_sleep( SAL_Device *device, sal_u32_t duration );
SAL_PUBLIC_API( sal_error_e ) SAL_alloc( SAL_Device *device, void **pp, size_t sz );
SAL_PUBLIC_API( sal_error_e ) SAL_free( SAL_Device *device, void *p );

#ifdef __cplusplus
}
#endif

#endif /* SAL_H */
