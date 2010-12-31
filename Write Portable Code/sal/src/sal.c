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
/** @file sal.c
    @brief Simple Audio File core functions
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "sal.h"

#include <stdarg.h>

#ifdef _MSC_VER
#  define vsnprintf _vsnprintf
#endif

/** @brief Perform vararg collation and then call the user-defined warning callback
    @internal
    @ingroup Utility
    Function used to display warning text to the console.  Dispatches
    to the print callback registered by the user during SAL_create_device,
    defaulting to fprintf(stder,...) if no callback was registered
    @param[in] device pointer to output device
    @param[in] kp_fmt printf-style format string
*/
sal_error_e
_SAL_warning( SAL_Device *device, const char *kp_fmt, ... )
{
    char buf[ 256 ];
    va_list args;
    
    va_start( args, kp_fmt );

    vsnprintf( buf, sizeof( buf ), kp_fmt, args );

    device->device_callbacks.warning( buf );

    return SALERR_OK;
}

/** @brief perform vararg collation then call the user-specified error callback
    @internal
    @ingroup Utility
    Function used to display error text and also to generate a fatal error.
    Dispatches to whatever error handler was registered in the callbacks structure
    during the call to SAL_create_device().
    @param[in] device pointer to output device
    @param[in] kp_fmt printf-style format string
*/
sal_error_e
_SAL_error( SAL_Device *device, const char *kp_fmt, ... )
{
    char buf[ 256 ];
    va_list args;
    
#ifdef _MSC_VER
    va_start( kp_fmt, args );
#else
	va_start( args, kp_fmt );
#endif

    vsnprintf( buf, sizeof( buf ), kp_fmt, args );

    device->device_callbacks.error( buf );

    return SALERR_OK;
}

/** @brief Returns the compiled version of SAL.
    @ingroup Utility
    @returns version number in the format 0xMMMMmmpp where MMMM is the major version, mm is the
    minor version, and pp is the patch level.
*/
sal_u32_t
SAL_get_version( void )
{
    return SAL_VERSION;
}

/** @brief Wrapper around the memory allocation callback provided by the user.
    @ingroup Utility
    @param[in] device pointer to output device
    @param[out] pp address of pointer where the location of allocated memory should be stored
    @param[in] sz number of bytes to be allocated
    @retval SALERR_OK on success
    @retval SALERR_OUTOFMEMORY if memory is exhausted
    @retval SALERR_INVALIDPARAM if device or pp are NULL, or if sz is < 1
    @remarks This function allows user code (e.g. custom samples) to use the 
    global memory allocation routines without requiring specific knowledge of
    the device callback format.
    @sa SAL_free
*/
sal_error_e 
SAL_alloc( SAL_Device *device, void **pp, size_t sz )
{
    if ( device == 0 || pp == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    if ( ( *pp = device->device_callbacks.alloc( sz ) ) == 0 )
    {
        return SALERR_OUTOFMEMORY;
    }

    return SALERR_OK;
}

/** @brief Wrapper around the memory free callback provided by the user.
    @ingroup Utility
    @param[in] device pointer to output device
    @param[in] p pointer to free
    @remarks This function allows user code (e.g. custom samples) to use the 
    global memory free routines without requiring specific knowledge of
    the device callback format.
    @sa SAL_alloc
*/
sal_error_e 
SAL_free( SAL_Device *device, void *p )
{
    if ( device == 0 || p == 0 )
    {
        return SALERR_INVALIDPARAM;
    }

    device->device_callbacks.free( p );

    return SALERR_OK;
}

/** @internal
    @ingroup Multithreading
    @brief Creates a mutex used for thread synchronization 
    @param[in] device pointer to output device
    @param[out] p_mtx pointer to sal_mutex_t structure
    @returns SALERR_OK on success, @ref sal_error_e on failure
*/
sal_error_e
_SAL_create_mutex( SAL_Device *device, sal_mutex_t *p_mtx )
{
    if ( device == 0 || p_mtx == 0 )
    {
        return SALERR_INVALIDPARAM;
    }
    return device->device_fnc_create_mutex( device, p_mtx );
}

/** @internal
    @ingroup Multithreading
    @brief Destroys a mutex used for thread synchronization 
    @param[in] device pointer to output device
    @param[in] mtx mutex to destroy
    @returns SALERR_OK on success, @ref sal_error_e on failure
*/
sal_error_e
_SAL_destroy_mutex( SAL_Device *device, sal_mutex_t mtx )
{
    if ( device == 0 || mtx == 0 )
    {
        return SALERR_INVALIDPARAM;
    }
    return device->device_fnc_destroy_mutex( device, mtx );
}

/** @internal
    @ingroup Multithreading
    @brief Locks a mutex.
    @param[in] device pointer to output device
    @param[in] mtx mutex to lock
    @returns SALERR_OK on success, @ref sal_error_e on failure
*/
sal_error_e
_SAL_lock_mutex( SAL_Device *device, sal_mutex_t mtx )
{
    if ( device == 0 || mtx == 0 )
    {
        return SALERR_INVALIDPARAM;
    }
    return device->device_fnc_lock_mutex( device, mtx );
}

/** @internal
    @ingroup Multithreading
    @brief Unlocks a mutex.
    @param[in] device pointer to output device
    @param[in] mtx mutex to lock
    @returns SALERR_OK on success, @ref sal_error_e on failure
*/
sal_error_e
_SAL_unlock_mutex( SAL_Device *device, sal_mutex_t mtx )
{
    if ( device == 0 || mtx == 0 )
    {
        return SALERR_INVALIDPARAM;
    }
    return device->device_fnc_unlock_mutex( device, mtx );
}

/** @internal
    @ingroup Multithreading
    @brief Creates and starts execution of a new thread.
    @param[in] device pointer to output device
    @param[in] fnc pointer to thread start function
    @param[in] targs arguments passed to the thread start function
    @returns SALERR_OK on success, @ref sal_error_e on failure
*/
sal_error_e 
_SAL_create_thread( SAL_Device *device, SAL_THREAD_FUNC fnc, void *targs )
{
    if ( device == 0 || fnc == 0 )
    {
        return SALERR_INVALIDPARAM;
    }
    return device->device_fnc_create_thread( device, fnc, targs );
}

/** @ingroup Utility
    @brief A cross-platform sleep function that sleeps the processor for duration_ms milliseconds
    @param[in] device pointer to output device
    @param[in] duration duration, in milliseconds, to sleep
    @returns SALERR_OK on success, @ref sal_error_e on failure
*/
sal_error_e
SAL_sleep( SAL_Device *device, sal_u32_t duration )
{
    if ( device == 0 )
    {
        return SALERR_INVALIDPARAM;
    }
    return device->device_fnc_sleep( device, duration );
}

/** @def SAL_BUILDING_LIB
    Defined if we're building the library, synonym for POSH_BUILDING_LIB and used
    to control POSH_PUBLIC_API/SAL_PUBLIC_API when building as a dynamic library.
*/
