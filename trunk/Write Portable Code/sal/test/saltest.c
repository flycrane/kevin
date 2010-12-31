#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/sal.h"

#define SALTEST_SUPPORT_OGG 1
#define SALTEST_SUPPORT_WAVE 1

#ifdef SALTEST_SUPPORT_OGG
#include "../src/extras/salx_ogg.h"
#endif

#ifdef SALTEST_SUPPORT_WAVE
#include "../src/extras/salx_wave.h"
#endif

#if defined POSH_OS_WIN32
#  include <windows.h>
#endif

#ifdef POSH_OS_WINCE
static FILE *s_fopen( const char *fname )
{
    char buffer[ 256 ];

    sprintf( buffer, "\\Windows\\Start Menu\\%s", fname );
    return fopen( buffer, "rb" );
}
#else
static FILE *s_fopen( const char *fname )
{
    return fopen( fname, "rb" );
}
#endif

#if defined POSH_OS_WIN32 && !defined POSH_OS_WINCE
#include <windows.h>
#include <time.h>
#include <fcntl.h>

static HWND g_hWnd;
static void saltest_sys_startup( SAL_SystemParameters *p_sp )
{
    char oldtitle[ 1024 ];
    char newtitle[ 1024 ];
    
    GetConsoleTitle( oldtitle, sizeof( oldtitle ) );
    
    sprintf( newtitle, "SAL Test - %d:%d", GetCurrentProcessId(), GetTickCount() );
    
    SetConsoleTitle( newtitle );
    
    Sleep( 100 );
    
    g_hWnd = FindWindow( NULL, newtitle );
    
    if ( g_hWnd == 0 )
    {
        fprintf( stderr, "Could not find window\n" );
        exit( 1 );
    }
    
    p_sp->sp_buffer_length_ms = 100;
    p_sp->sp_hWnd = g_hWnd;
}
void prompt( const char *text )
{
    fprintf( stdout, text );
    fflush( stdout );
}
#elif defined POSH_OS_WINCE
static void saltest_sys_startup( SAL_SystemParameters *p_sp )
{
    p_sp->sp_buffer_length_ms = 100;
    p_sp->sp_flags = SAL_SPF_WAVEOUT;
    p_sp->sp_hWnd = GetDesktopWindow();
}

static void prompt( const char *text )
{
    wchar_t awcString[ 1024 ];
    
    MultiByteToWideChar( CP_ACP,
                         0,
                         text,
                         -1,
                         awcString,
                         sizeof( awcString ) );
    
    MessageBox( GetDesktopWindow(), awcString, _T("PROMPT"), MB_OK );
}

#else
static void saltest_sys_startup( SAL_SystemParameters *p_sp )
{
}
static void prompt( const char *text )
{
    fprintf( stdout, text );
    fflush( stdout );
}
#endif

static long flength( FILE *fp )
{
    fpos_t pos;
    long l;
    
    fgetpos( fp, &pos );
    fseek( fp, 0, SEEK_END );
    l = ftell( fp );
    
    fsetpos( fp, &pos );
    
    return l;
}


void test_sal( const SAL_SystemParameters *kp_sp );

#ifdef POSH_OS_WINCE
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd )
{
    int main(int,const char *argv[] );
    int argc = 1;
    char *argv[256] = { "exe" };

    main( argc, argv );
}
#endif

int 
main( int argc, const char *argv[] )
{
    int i;
    SAL_SystemParameters sp;

    memset( &sp, 0, sizeof( sp ) );
    sp.sp_size = sizeof( sp );
    
    saltest_sys_startup( &sp );
    
    /* check command line arguments */
    for ( i = 1; i < argc; i++ )
    {
        if ( strcmp( argv[ i ], "--alsa" ) == 0 )
        {
            sp.sp_flags |= SAL_SPF_ALSA;
        }
        if ( strcmp( argv[ i ], "--wave" ) == 0 )
        {
            sp.sp_flags |= SAL_SPF_WAVEOUT;
        }
    }
    
    test_sal( &sp );
    
    return 0;
}

typedef struct
{
    int sta_frequency;
} SawToothArgs;

static void 
sawtooth_destructor( SAL_Device *p_device, SAL_Sample *self )
{
    SAL_SampleArgs sargs;
    
    SAL_get_sample_args( p_device, self, &sargs );
    SAL_free( p_device, sargs.sarg_ptr );
}

static int 
sawtooth_decoder( SAL_Device *p_device, sal_voice_t voice, sal_byte_t *p_dst, int bytes_needed )
{
    SAL_Sample *sample = 0;
    sal_byte_t *sample_data = 0;
    SAL_SampleArgs sargs;
    SAL_DeviceInfo dinfo;
    int samples_needed;
    SawToothArgs *stargs;
    
    memset( &dinfo, 0, sizeof( dinfo ) );
    dinfo.di_size = sizeof( dinfo );
    SAL_get_device_info( p_device, &dinfo );
    
    SAL_get_voice_sample( p_device, voice, &sample );
    SAL_get_sample_args( p_device, sample, &sargs );
    SAL_get_sample_data( p_device, sample, &sample_data );
    
    stargs = ( SawToothArgs * ) sargs.sarg_ptr;
    
    samples_needed = bytes_needed / dinfo.di_bytes_per_sample;
    
    /* NOTE: we're assuming constant looping and 16-bit/44.1Khz */
    {
        int cursor;
        int i;
        
        for ( i = 0; i < samples_needed; i += dinfo.di_channels, p_dst += 2 * dinfo.di_channels )
        {
            sal_i16_t *dst16 = ( sal_i16_t * ) p_dst;
            
            SAL_get_voice_cursor( p_device, voice, &cursor );
            
            /* a real implementation would want to use a wavetable */
            if ( cursor < ( dinfo.di_sample_rate / stargs->sta_frequency )  )
            {
                dst16[0] = 4000;
            }
            else
            {
                dst16[0] = -4000;
            }
            
            if ( dinfo.di_channels == 2 )
            {
                dst16[1] = dst16[0];
            }
            
            if ( !_SAL_advance_voice( p_device, voice, 1 ) )
            {
                return 1;
            }
        }
    }
    
    return 0;
}

static
SAL_Sample *
create_sawtooth_sample( SAL_Device *device )
{
    SAL_Sample *s = 0;
    SawToothArgs *args;
    SAL_SampleArgs sargs;
    SAL_DeviceInfo dinfo;
    
    memset( &dinfo, 0, sizeof( dinfo ) );
    dinfo.di_size = sizeof( dinfo );
    SAL_get_device_info( device, &dinfo );
    
    if ( dinfo.di_bits != 16 || dinfo.di_sample_rate != 44100 )
    {
        return NULL;
    }
    
    SAL_alloc( device, (void**)&args, sizeof( *args ) );
    
    sargs.sarg_ptr = args;
    
    args->sta_frequency = 440;
    
    SAL_create_sample( device, &s, 0, sawtooth_decoder, sawtooth_destructor, &sargs );
    
    return s;
}

#ifdef SALTEST_SUPPORT_OGG
static
SAL_Sample *
load_sample_ogg( SAL_Device *device, const char *name )
{
    FILE *fp = 0;
    sal_byte_t *buffer;
    SAL_Sample *s = 0;
    
    if ( ( fp = s_fopen( name ) ) != 0 )
    {
        long l = flength( fp );
        buffer = ( sal_byte_t * ) malloc( l );
        
        fread( buffer, l, 1, fp );

        if ( SALx_create_sample_from_ogg( device, &s, buffer, l ) != SALERR_OK )
        {
            s = 0;
        }
        
        free( buffer );
        
        fclose( fp );
    }
    
    return s;
}
#endif

static
SAL_Sample *
load_sample( SAL_Device *device, const char *name )
{
    FILE *fp = 0;
    sal_byte_t *buffer;
    SAL_Sample *s = 0;
    
    if ( ( fp = s_fopen( name ) ) != 0 )
    {
        long l = flength( fp );
        buffer = ( sal_byte_t * ) malloc( l );
        
        fread( buffer, l, 1, fp );

        if ( SALx_create_sample_from_wave( device, &s, buffer, l ) != SALERR_OK )
        {
            s = 0;
        }
        
        free( buffer );
        
        fclose( fp );
    }
    
    return s;
}

static void
test_panning( SAL_Device  *device, SAL_Sample *s, int loop_end )
{
    sal_voice_t v = SAL_INVALID_SOUND;
    int i;
    
    SAL_play_sample( device, s, &v, 0xFFFF, SAL_PAN_HARD_LEFT, 0, loop_end, SAL_LOOP_ALWAYS );
    
    for ( i = 0; i < 500; i++ )
    {
        SAL_set_voice_pan( device, v, ( sal_i16_t ) ( SAL_PAN_HARD_LEFT + i * 128 ) );
        SAL_sleep( device, 10 );
    }
    
    SAL_stop_voice( device, v );
}

static void
test_stereo_volume( SAL_Device *device, SAL_Sample *s )
{
    sal_voice_t v = SAL_INVALID_SOUND;
    int i;
    
    SAL_play_sample( device, s, &v, 0xFFFF, 0, 0, 0, SAL_LOOP_ALWAYS );
    
    for ( i = 0; i < 500; i++ )
    {
        SAL_set_voice_volume( device, v, ( sal_i16_t ) ( 65535 - i * 128 ) );
        SAL_sleep( device, 10 );
    }
    
    SAL_stop_voice( device, v );
}

typedef struct
{
    int channels, bits, sample_rate;
} device_format;

static device_format formats[] =
{
    { 2, 16, 44100 },
    { 1, 16, 44100 },
    { 1, 8, 22050 },
    { 2, 8, 22050 },
    { 1, 16, 22050 },
    { 2, 16, 22050 },
    { 1, 16, 11025 },
    { 2, 16, 11025 },
    { 1, 8, 11025 },
    { 2, 8, 11025 },
    { 1, 8, 44100 },
    { 2, 8, 44100 }
};

void
test_sal( const SAL_SystemParameters *kp_sp )
{
    sal_error_e err;
    SAL_Sample *s = 0;
    int i;

    if ( SAL_get_version() != SAL_VERSION )
    {
        fprintf( stderr, "Wrong version, found %x was expecting %x\n", SAL_get_version(), SAL_VERSION );
        exit( 1 );
    }
    
    printf( "SAL version: 0x%08x\n", SAL_get_version() );
    
    for ( i = 0; i < sizeof( formats ) / sizeof( formats[ 0 ] ); i++ )
    {
        char buf[ 256 ];
        SAL_Device *device;
        SAL_DeviceInfo dinfo;
        
        sprintf( buf, "Creating device (%d,%d,%d): ", formats[ i ].channels, formats[ i ].bits, formats[ i ].sample_rate );
        prompt( buf );
        
        /* create device */
        if ( ( err = SAL_create_device( &device,                /* pointer to device */
                                        NULL,                   /* callbacks */
                                        kp_sp,                  /* system parameter information */
                                        formats[i].channels,    /* desired channels */
                                        formats[i].bits,        /* desired bits-per-sample */
                                        formats[i].sample_rate, /* desired sample rate */
                                        8 ) ) != SALERR_OK )    /* number of voices */
        {
            sprintf( buf, "failed (error = %d)\n", err );
            prompt( buf );
            continue;
        }
        prompt( "ok\n" );
        
        memset( &dinfo, 0, sizeof( dinfo ) );
        dinfo.di_size = sizeof( dinfo );
        SAL_get_device_info( device, &dinfo );
        sprintf( buf, "device name = %s\n", dinfo.di_name );
        prompt( buf );

#ifdef SALTEST_SUPPORT_OGG
        /* ogg vorbis test */
        prompt( "   Ogg Vorbis test: " );
        if ( ( s = load_sample_ogg( device, "stereotest.ogg" ) ) != 0 )
        {
            test_panning( device, s, 0 );

            SAL_destroy_sample( device, s );
            printf( "done\n" );
            SAL_sleep( device, 2000 );
        }
        else
        {
            prompt( "FAILED\n" );
        }
#endif

        /* panning test */
        prompt( "   Panning test (16-bit mono source): " );
        
        if ( ( s = load_sample( device, "pantest.wav" ) ) != 0 )
        {
            test_panning( device, s, 0 );
            
            SAL_destroy_sample( device, s );
            printf( "done\n" );
            SAL_sleep( device, 2000 );
        }
        else
        {
            prompt( "FAILED (could not load sample)\n" );
        }

        /* sawtooth test */
        prompt( "   Sawtooth test (16-bit mono source, panning): " );
        
        if ( ( s = create_sawtooth_sample( device ) ) != 0 )
        {
            test_panning( device, s,  dinfo.di_sample_rate * 2 / 440 );
            
            SAL_destroy_sample( device, s );
            printf( "done\n" );
            SAL_sleep( device, 2000 );
        }
        else
        {
            printf( "failed (most likely due to mismatched formats)\n" );
        }
        
        /* stereo + volume test, 16-bit, 22K */
        prompt( "   Stereo + volume test (16-bit stereo 22Khz source): " );

        if ( ( s = load_sample( device, "stereotest16_22K.wav" ) ) != 0 )
        {
            test_stereo_volume( device, s );
            
            SAL_destroy_sample( device, s );
            printf( "done\n" );
            SAL_sleep( device, 2000 );
        }
        else
        {
            printf( "FAILED (could not load sample)\n" );
        }

        /* panning test, 8-bit */
        prompt( "   Panning test (8-bit mono source): " );

        if ( ( s = load_sample( device, "pantest8.wav" ) ) != 0 )
        {
            test_panning( device, s, 0 );
            
            SAL_destroy_sample( device, s );
            prompt( "done\n" );
            SAL_sleep( device, 2000 );
        }
        else
        {
            printf( "FAILED (could not load sample)\n" );
        }
        
        /* stereo + volume test, 8-bit */
        prompt( "   Stereo + volume test (8-bit stereo source): " );
        
        if ( ( s = load_sample( device, "stereotest8.wav" ) ) != 0 )
        {
            test_stereo_volume( device, s );
            
            SAL_destroy_sample( device, s );
            printf( "done\n" );
            SAL_sleep( device, 2000 );
        }
        else
        {
            printf( "FAILED (could not load sample)\n" );
        }
        
        /* stereo + volume test */
        prompt( "   Stereo + volume test (16-bit stereo source): " );

        if ( ( s = load_sample( device, "stereotest.wav" ) ) != 0 )
        {
            test_stereo_volume( device, s );
            
            SAL_destroy_sample( device, s );
            printf( "done\n" );
            SAL_sleep( device, 2000 );
        }
        else
        {
            printf( "FAILED (could not load sample)\n" );
        }

        /* destroy device */
        prompt( "   Destroying device\n" );
        SAL_destroy_device( device );
    }
}
