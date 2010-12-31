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
/** @file sal_coreaudio.c
    @brief CoreAudio implementation
*/
#ifndef SAL_DOXYGEN
#  define SAL_BUILDING_LIB 1
#endif
#include "../sal.h"

#if defined POSH_OS_OSX || defined SAL_DOXYGEN

#include <unistd.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>

static
OSStatus	
s_audio_renderer( void *inRefCon, 
                  AudioUnitRenderActionFlags 	*ioActionFlags, 
                  const AudioTimeStamp 		*inTimeStamp, 
                  UInt32 						inBusNumber, 
                  UInt32 						inNumberFrames, 
                  AudioBufferList 			*ioData)
{
    SAL_Device *device = ( SAL_Device * ) inRefCon;
    int channel;

    for ( channel = 0; channel < ioData->mNumberBuffers; channel++)
    {
        /* mix into the first buffer */
        _SAL_mix_chunk( device, 
                        ioData->mBuffers[ channel ].mData,
                        ioData->mBuffers[ channel ].mDataByteSize );
    }
	
    return noErr;
}

static void destroy_device_data_coreaudio( SAL_Device *device );

/** @internal
    @ingroup osx
    @brief private device data for the CoreAudio subsystem */
typedef struct SAL_CoreAudioData_s
{
    AudioUnit		cad_audio_unit;     /**< audio unit */
} SAL_CoreAudioData;

/** @internal
    @ingroup osx
    @brief CoreAudio device creation function
*/
sal_error_e
_SAL_create_device_data_coreaudio( SAL_Device *device,
                                   const SAL_SystemParameters *kp_sp,
                                   sal_u32_t desired_channels,
                                   sal_u32_t desired_bits,
                                   sal_u32_t desired_sample_rate )
{
    SAL_CoreAudioData *cad = 0;
    OSStatus err = noErr;
    ComponentDescription desc;
    Component comp;
    ComponentResult cresult;
    UInt32 sz;
    AURenderCallbackStruct input;
    AudioStreamBasicDescription desired_format, device_format;

    device->device_fnc_destroy = destroy_device_data_coreaudio;
	
    /* allocate memory for the coreaudio data */
    cad = device->device_callbacks.alloc( sizeof( *cad ) );
    memset( cad, 0, sizeof( *cad ) );

    /* Open the default output unit */
    memset( &desc, 0, sizeof( desc ) );
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
	
    if ( ( comp = FindNextComponent(NULL, &desc) ) == NULL )
    {
        device->device_callbacks.free( cad );
        _SAL_warning( device, "FindNextComponent failed\n" );
        return SALERR_SYSTEMFAILURE;
    }
	
    if ( ( err = OpenAComponent(comp, &cad->cad_audio_unit ) ) != noErr )
    {
        device->device_callbacks.free( cad );
        _SAL_warning( device, "OpenAComponent failed\n" );
        return SALERR_SYSTEMFAILURE;
    }

    /* get the device format */
    sz = sizeof( device_format );
    cresult = AudioUnitGetProperty( cad->cad_audio_unit,
                                    kAudioUnitProperty_StreamFormat,
                                    kAudioUnitScope_Input,
                                    0,
                                    &device_format,
                                    &sz );

    /* set the desired format */
    desired_format.mSampleRate = desired_sample_rate;
    desired_format.mFormatID =           kAudioFormatLinearPCM;
    if ( desired_bits == 16 )
    {
        desired_format.mFormatFlags =  kLinearPCMFormatFlagIsSignedInteger |
            kLinearPCMFormatFlagIsPacked |
            kLinearPCMFormatFlagIsBigEndian;
    }
    else
    {
        desired_format.mFormatFlags = kLinearPCMFormatFlagIsPacked;
    }
    desired_format.mChannelsPerFrame =    desired_channels;
    desired_format.mBitsPerChannel =      desired_bits;
    desired_format.mBytesPerPacket =      (desired_bits/8) * desired_channels;
    desired_format.mFramesPerPacket =     1;
    desired_format.mBytesPerFrame =       desired_format.mFramesPerPacket * desired_format.mBytesPerPacket;
	
    err = AudioUnitSetProperty( cad->cad_audio_unit,
                                kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input,
                                0,
                                &desired_format,
                                sizeof( desired_format ) );

    /* set up our callback function */
    memset( &input, 0, sizeof( input ) );
    input.inputProc = s_audio_renderer;
    input.inputProcRefCon = device;

    err = AudioUnitSetProperty( cad->cad_audio_unit,
                                kAudioUnitProperty_SetRenderCallback, 
                                kAudioUnitScope_Input,
                                0, 
                                &input, 
                                sizeof(input) );
	
    /* get our formats again to store into device info */
    sz = sizeof( device_format );
    cresult = AudioUnitGetProperty( cad->cad_audio_unit,
                                    kAudioUnitProperty_StreamFormat,
                                    kAudioUnitScope_Input,
                                    0,
                                    &device_format,
                                    &sz );

    device->device_data = cad;
    device->device_info.di_size        = sizeof( device->device_info );
    device->device_info.di_bits        = device_format.mBitsPerChannel;
    device->device_info.di_channels    = device_format.mChannelsPerFrame;
    device->device_info.di_sample_rate = (int)device_format.mSampleRate;
    strncpy( device->device_info.di_name, "CoreAudio", sizeof( device->device_info.di_name ) );

    /* initialize the audio unit */
    err = AudioUnitInitialize( cad->cad_audio_unit );
    err = AudioOutputUnitStart( cad->cad_audio_unit );
	
    return SALERR_OK;
}
								  
static 
void
destroy_device_data_coreaudio( SAL_Device *device )
{
    SAL_CoreAudioData *cad;
	
    if ( device == 0 || device->device_data == 0 )
    {
        return;
    }
	
    _SAL_lock_device( device );
	
    cad = ( SAL_CoreAudioData * ) device->device_data;
	
    AudioOutputUnitStop( cad->cad_audio_unit );
    AudioUnitUninitialize( cad->cad_audio_unit );
	
    device->device_callbacks.free( device->device_data );
    device->device_data = 0;
	
    _SAL_unlock_device( device );
}

#endif /* POSH_OS_OSX */
