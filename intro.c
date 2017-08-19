#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>
#include <stdio.h>

// relevant glext.h fragment
#define APIENTRYP __stdcall *
typedef char GLchar;
#define GL_FRAGMENT_SHADER                0x8B30
typedef void (APIENTRYP PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRYP PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef GLuint (APIENTRYP PFNGLCREATEPROGRAMPROC) (void);
typedef GLuint (APIENTRYP PFNGLCREATESHADERPROC) (GLenum type);
typedef void (APIENTRYP PFNGLLINKPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (APIENTRYP PFNGLUSEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP PFNGLUNIFORM1FPROC) (GLint location, GLfloat v0);
// end of glext.h fragment

#define GL_EXT_FUNCTION_COUNT 8

static const char *glExtFunctionNames[] = {
    "glAttachShader",
    "glCompileShader",
    "glCreateProgram",
    "glCreateShader",
    "glLinkProgram",
    "glShaderSource",
    "glUseProgram",
    "glUniform1f"
};

static void *glExtFunctions[GL_EXT_FUNCTION_COUNT] = { 0 };

#define glAttachShader ((PFNGLATTACHSHADERPROC)glExtFunctions[0])
#define glCompileShader ((PFNGLCOMPILESHADERPROC)glExtFunctions[1])
#define glCreateProgram ((PFNGLCREATEPROGRAMPROC)glExtFunctions[2])
#define glCreateShader ((PFNGLCREATESHADERPROC)glExtFunctions[3])
#define glLinkProgram ((PFNGLLINKPROGRAMPROC)glExtFunctions[4])
#define glShaderSource ((PFNGLSHADERSOURCEPROC)glExtFunctions[5])
#define glUseProgram ((PFNGLUSEPROGRAMPROC)glExtFunctions[6])
#define glUniform1f ((PFNGLUNIFORM1FPROC)glExtFunctions[7])

static PIXELFORMATDESCRIPTOR pfd = {
    sizeof(PIXELFORMATDESCRIPTOR),
    1,
    PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    PFD_TYPE_RGBA,
    32,
    0,
    0,
    0,
    0,
    0,
    0,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    32,
    0,
    0,
    PFD_MAIN_PLANE,
    0,
    0,
    0,
    0
};

//#define CAPTURE_FRAMES

#define UNIFORM_COUNT 6

#include "shader.h"

#define TRACKER_PERIOD 4725 // 140 bpm (44100 * 60 / 140 / 4)
#define TRACKER_PATTERN_LENGTH 16 // 16 periods (16th) per pattern
#define TRACKER_SONG_LENGTH 40 // in patterns
#define AUDIO_SAMPLES (TRACKER_PERIOD * TRACKER_PATTERN_LENGTH * TRACKER_SONG_LENGTH * 2)

#define AUDIO_DEBUG

static const unsigned int riffHeader[11] = {
    0x46464952, /* RIFF */
    AUDIO_SAMPLES * 2 + 36,
    0x45564157, /* WAVE */
    
    /* fmt chunk */
    0x20746D66,
    16,
    0x00020001,
    44100,
    176400,
    0x00100004,
    
    /* data chunk */
    0x61746164,
    AUDIO_SAMPLES * 2
};

#define DELAY_LEFT (TRACKER_PERIOD * 2 * 2)
#define DELAY_RIGHT (TRACKER_PERIOD * 3 * 2)

short audioBuffer[AUDIO_SAMPLES + 22];
short auxBuffer[AUDIO_SAMPLES + DELAY_LEFT + DELAY_RIGHT];

float TAU = 2.0f * 3.14159265f;

void __declspec(naked) _CIpow()
{
	_asm
	{
		fxch st(1)
		fyl2x
		fld st(0)
		frndint
		fsubr st(1),st(0)
		fxch st(1)
		fchs
		f2xm1
		fld1
		faddp st(1),st(0)
		fscale
		fstp st(1)
		ret
	}
}

static float expf(float value)
{
	return pow(2.71828f, value);
}

static float iexpf(float value)
{
	return 1.0f / pow(2.71828f, 1.0f / value);
}

// returns fractional part but from -0.5f to 0.5f
static float sfract(float f)
{
	return f - (float)(int)f;
}

static float rand(float f)
{
	f = sin(f * 12.9898) * 43758.5453;
	return sfract(f);
}

typedef float (*Instrument)(float t, float phase);

static float silence(float t, float phase)
{
    return 0.0f;
}

#define SAW_VOLUME_DIVIDER 5.0f
static float saw(float t, float phase)
{
	return sfract(phase / TAU) * 2.0f / SAW_VOLUME_DIVIDER;
}

#define SAW2_VOLUME_DIVIDER 10.0f
static float saw2(float t, float phase)
{
	float adsr = expf(-(float)t * 0.001f);
	return adsr * sfract(phase / TAU) * 2.0f / SAW2_VOLUME_DIVIDER;
}

static float square(float t, float phase)
{
	float adsr = expf(-(float)t * 0.0004f);
	float out = sfract(phase / TAU) >= 0.0f;
	
	return adsr * out * 0.1f;
}

static float kick(float t, float phase)
{
	float out = expf(-t * 0.001f) * sin(phase * expf(-t * 0.0002f));
	
    return out;
}

#define SINE_VOLUME_DIVIDER 6.0f
static float sine(float t, float phase)
{
	float adsr = expf(-(float)t * 0.001f);
	float out = adsr * sin(phase);// + 0.2f * sin(phase * 1.1f);
    return out / SINE_VOLUME_DIVIDER;
}

#define REESE_VOLUME_DIVIDER 4.0f
static float reese(float t, float phase)
{
	float detune = 1.01f;
	float out = sin(phase) + 0.5f * sin(phase * 2.0) + 0.33f * sin(phase * 3.0)
	          + sin(phase * detune) + 0.5f * sin(phase * 2.0 * detune) + 0.33f * sin(phase * 3.0 * detune);
	
    return 0.25f * out / REESE_VOLUME_DIVIDER;
}

#define NOISE_VOLUME_DIVIDER 32.0f
static float noise(float t, float phase)
{
	float adsr = expf(-phase * 0.01f);
	float out = adsr * rand(phase) * 2.0;
	
    return out / NOISE_VOLUME_DIVIDER;
}

static float noise2(float t, float phase)
{
	float out = iexpf(phase * 0.001f) * rand(phase) * 2.0;

	return out / NOISE_VOLUME_DIVIDER;
}

static float mangatome(float t, float phase)
{
	/*float detune = 1.01f;
	float out = sin(phase) + 0.5f * sin(phase * 2.0) + 0.33f * sin(phase * 3.0)
	          + sin(phase * detune) + 0.5f * sin(phase * 2.0 * detune) + 0.33f * sin(phase * 3.0 * detune);*/
	
	//float out = sin(phase * sin(t / 0.0000001f));
	//float out = sin(phase + sin(phase * 1.12f) * t * 0.00001f);
	
	float detune = 1.01f - t * 0.0000002f;
	float out = sin(phase + sin(phase * 1.12f) * t * 0.000001f) + sin(phase * detune + sin(phase * detune * 0.87f) * t * 0.000003f);
	
	//float out = saw(t, phase) + saw(t, phase * 1.02f);
	
    return out * 0.15f;
}

Instrument instruments[] = {
    /* 0 */ silence,
    /* 1 */ saw,
    /* 2 */ saw2,
	/* 3 */ square,
	/* 4 */ kick,
	/* 5 */ sine,
	/* 6 */ reese,
	/* 7 */ noise,
	/* 8 */ mangatome,
	/* 9 */ noise2
};

#define CHANNELS 8

typedef struct
{
    unsigned int frame;
    unsigned int period;
    unsigned char instrument;
} ChannelState;

ChannelState channels[CHANNELS];

// effect bitfield

// 0x8: left output
// 0x4: right output
// 0x2: stereo delay
// 0x1: pitch fall

unsigned short patterns[][TRACKER_PATTERN_LENGTH * 2] = {
    // note, effects (4 bits) + instrument (4 bits)
    {
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 1 - Mangatome
	{
        NOTE(37), 0xc8,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 2 - bass
    {
        NOTE(13), 0xc6,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(17), 0xc6,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(20), 0xc6,
        0, 0
    },
	
	// 3 - solo
    {
        NOTE(37), 0xe2,
        0, 0,
        NOTE(44), 0xe2,
        0, 0,
        NOTE(41), 0xe2,
        0, 0,
        NOTE(44), 0xe2,
        0, 0,
        NOTE(37), 0xe2,
        0, 0,
        NOTE(44), 0xe2,
        0, 0,
        NOTE(41), 0xe2,
        0, 0,
        NOTE(44), 0xe2,
        0, 0
    },
	
	// 4 - noise
	{
        NOTE(30), 0xe7,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 5 - note off
	{
        NOTE(0), 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 6 - chords
	{
        NOTE(32), 0xe3,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(34), 0xe3,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 7 - chords2
	{
        NOTE(37), 0xe3,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(41), 0xe3,
        0, 0,
        0, 0,
        0, 0
    },

	// 8 - noise2
	{
		NOTE(30), 0xe9,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0
	},

	// 9 - peek in
	{
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		NOTE(52), 0xd3,
		0, 0,
		NOTE(54), 0xd3,
		0, 0
	},

	// 10 - peek out
	{
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		0, 0,
		NOTE(42), 0xd3,
		0, 0
	}
};

unsigned char song[TRACKER_SONG_LENGTH][CHANNELS] = {
	// 0 - intro (32)
    { 1, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
	
	//// 32 - bass
    //{ 0, 2, 0, 0, 0, 0, 0, 0 },
    //{ 0, 2, 0, 0, 0, 0, 0, 0 },
    //{ 0, 2, 0, 0, 0, 0, 0, 0 },
    //{ 0, 2, 0, 0, 0, 0, 0, 0 },
	//{ 0, 2, 3, 0, 0, 0, 0, 0 },
	//{ 0, 2, 3, 0, 0, 0, 0, 0 },
	{ 0, 2, 3, 0, 0, 0, 0, 0 },
	{ 0, 2, 3, 0, 0, 0, 0, 0 },
    { 0, 2, 3, 0, 0, 0, 0, 0 },
    { 0, 2, 3, 0, 0, 0, 0, 0 },
    { 0, 2, 3, 0, 0, 0, 0, 0 },
    { 0, 2, 3, 4, 0, 0, 0, 0 },
	
	// 32 - happy
    { 5, 2, 3, 6, 7, 0, 0, 0 },
	{ 0, 2, 3, 6, 7, 0, 0, 0 },
    { 0, 2, 3, 6, 7, 0, 0, 0 },
    { 0, 2, 3, 6, 7, 0, 0, 0 },
    { 0, 2, 3, 6, 7, 0, 0, 0 },
    { 0, 2, 3, 6, 7, 0, 0, 0 },
    { 0, 2, 3, 6, 7, 0, 0, 0 },
    { 0, 2, 3, 6, 7, 0, 0, 0 },

	//// final - white noise
	//{ 0, 2, 3, 6, 7, 8, 0, 0 },
	//{ 0, 2, 3, 6, 7, 0, 8, 0 },
	//{ 0, 2, 3, 6, 7, 0, 0, 8 },
	///*{ 8, 2, 8, 6, 8, 0, 0, 0 },
	//{ 0, 8, 0, 9, 0, 0, 0, 0 },*/
	//{ 0, 0, 0, 0, 0, 0, 0, 0 },
	//{ 0, 0, 0, 0, 9, 0, 0, 0 },

	//// final - hats away
	//{ 0, 2, 3, 6, 7, 0, 0, 0 },
	//{ 0, 2, 3, 6, 7, 4, 0, 0 },
	//{ 0, 2, 0, 6, 7, 4, 4, 0 },
	//{ 4, 2, 0, 6, 0, 4, 4, 4 },
	//{ 4, 0, 4, 0, 0, 4, 4, 4 },

	// final - hats away + noise
	{ 0, 2, 3, 6, 7, 0, 0, 0 },
	{ 0, 2, 3, 6, 7, 4, 0, 0 },
	{ 0, 2, 8, 6, 7, 4, 4, 0 },
	{ 4, 2, 0, 6, 8, 4, 4, 4 },
	//{ 4, 4, 5, 5, 0, 4, 4, 4 },
	{ 4, 4, 5, 5, 5, 5, 5, 5 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },

	//// final - peek in
	//{ 5, 5, 5, 5, 5, 5, 5, 5 },
	//{ 0, 0, 0, 0, 0, 0, 0, 9 }

	// final - peek in & out
	//{ 5, 5, 5, 5, 5, 5, 5, 5 },
	{ 9, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 10, 0, 0, 0, 0, 0, 0, 0 }

	//=================================
	
	// 4 - black & white start (32)
    /*{ 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 9, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 5, 7, 8, 0, 0, 0, 0 },
	
	// 36 - blue! (32)
    { 4, 6, 7, 8, 2, 0, 0, 20 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 9, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 5, 7, 8, 2, 0, 0, 0 },
	
	// 68 - balls (32)
    { 4, 6, 7, 8, 2, 10, 0, 21 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 9, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 5, 7, 8, 2, 10, 0, 22 },
	
	// 100 - more balls! (32)
    { 4, 6, 7, 8, 2, 10, 0, 21 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 9, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 21 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 11, 7, 8, 2, 10, 0, 22 },
	
	// 132 - fire from hell (32)
    { 17, 12, 15, 18, 13, 0, 0, 20 },
    { 17, 12, 16, 18, 14, 0, 0, 0 },
    { 17, 12, 15, 18, 13, 0, 0, 0 },
    { 17, 12, 16, 18, 14, 19, 0, 0 },
    { 17, 12, 15, 18, 13, 0, 0, 0 },
    { 17, 12, 16, 18, 14, 0, 0, 21 },
    { 17, 12, 15, 18, 13, 0, 0, 0 },
    { 17, 12, 16, 18, 14, 19, 0, 0 },
	
	// 164 - more fire (32)
    { 17, 12, 15, 18, 13, 00, 25, 20 },
    { 17, 12, 16, 18, 14, 00, 0, 00 },
    { 17, 12, 15, 18, 13, 00, 0, 21 },
    { 17, 12, 16, 18, 14, 19, 0, 00 },
    { 17, 12, 15, 18, 13, 00, 25, 20 },
    { 17, 12, 16, 18, 14, 00, 0, 21 },
    { 17, 12, 15, 18, 13, 00, 0, 00 },
    { 1, 12, 16, 18, 14, 19, 20, 22 },
	
	// 196 - craziness (32)
    { 1, 23, 7, 8, 2, 10, 22, 21 },
    { 3, 24, 7, 8, 2, 10, 22, 0 },
    { 4, 6, 7, 8, 2, 10, 22, 0 },
    { 1, 6, 7, 8, 2, 10, 22, 0 },
    { 3, 26, 7, 8, 2, 10, 22, 21 },
    { 1, 27, 7, 8, 2, 10, 22, 0 },
    { 4, 6, 7, 8, 2, 10, 22, 0 },
    { 3, 9, 7, 8, 2, 10, 22, 0 },
	
	// 228 - craziness (32)
    { 3, 23, 7, 8, 2, 10, 2, 20 },
    { 1, 24, 7, 8, 2, 10, 22, 0 },
    { 4, 6, 7, 8, 2, 10, 22, 21 },
    { 3, 6, 7, 8, 2, 10, 22, 19 },
    { 1, 26, 7, 8, 2, 10, 22, 0 },
    { 4, 27, 7, 8, 2, 10, 22, 21 },
    { 3, 6, 7, 8, 2, 10, 22, 0 },
    { 1, 11, 5, 8, 2, 10, 22, 19 },
	
	// 260 - back to the roots, but crazy (32) - greets?
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 9, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 5, 7, 8, 0, 0, 0, 0 },
	
	// 292 - blue again! (32)
    { 4, 6, 7, 8, 2, 0, 0, 20 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 9, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 5, 7, 8, 2, 0, 0, 0 },
	
	// 324 - end
    { 0, 6, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 }*/
};

static __forceinline void renderAudio()
{
    int i, j, k, l, delay;
    short *buffer = audioBuffer + 22;
    short *aux = auxBuffer + DELAY_LEFT + DELAY_RIGHT;
    char *songPosition;
    
    for (songPosition = (char *)song; songPosition != (char *)song + TRACKER_SONG_LENGTH * CHANNELS; songPosition += CHANNELS)
    {
        for (k = 0; k < TRACKER_PATTERN_LENGTH; k++)
        {
            for (j = 0; j < CHANNELS; j++)
            {
                short *periodBuffer = buffer;
                short *auxPeriodBuffer = aux;
                
                unsigned short *pattern = patterns[songPosition[j]];
                unsigned short period = pattern[k * 2 + 0];
                
                if (period)
                {
                    channels[j].frame = 0;
                    channels[j].period = period;
                    channels[j].instrument = pattern[k * 2 + 1];
                }
                
                for (l = 0; l < TRACKER_PERIOD; l++)
                {
					float t = (float)channels[j].frame;
					float phase = TAU * t / (float)channels[j].period;
					float sf = instruments[channels[j].instrument & 0x0f](t, phase) * 0.8f;
                    short s = (short)((int)(sf * 32767.0f)/* * 3 / 4*/);
                    short l = (channels[j].instrument & 0x80) ? s : 0;
                    short r = (channels[j].instrument & 0x40) ? s : 0;
                    
                    *periodBuffer++ += l;
                    *periodBuffer++ += r;
                    
                    *auxPeriodBuffer++ += (channels[j].instrument & 0x20) ? l : 0;
                    *auxPeriodBuffer++ += (channels[j].instrument & 0x20) ? r : 0;
                    
                    if (!(channels[j].frame % 1000) && (channels[j].instrument & 0x10))
                        channels[j].period++;
                    
                    channels[j].frame++;
                }
            }
            
            buffer += TRACKER_PERIOD * 2;
            aux += TRACKER_PERIOD * 2;
        }
    }
    
    delay = DELAY_LEFT;
    for (buffer = audioBuffer + 22, aux = auxBuffer + DELAY_LEFT + DELAY_RIGHT; aux != auxBuffer + AUDIO_SAMPLES + DELAY_LEFT + DELAY_RIGHT;)
    {
        short s = aux[-delay] * 3 / 4;
        *aux += s;
        
        delay ^= DELAY_LEFT;
        delay ^= DELAY_RIGHT;
        
        *buffer++ += *aux++;
    }
}

#ifdef CAPTURE_FRAMES
char *frameFilename(int n)
{
    static char *name = "frame00000.raw";
    char *ptr = name + 9;
    
    while (n > 0)
    {
        *ptr-- = (n - (n / 10) * 10) + '0';
        n /= 10;
    }
    
    return name;
}
#endif

void entry()
{
    HWND hwnd;
    HDC hdc;
    unsigned int i;
    GLint program;
    GLint fragmentShader;
    DWORD startTime;
	DWORD width;
	DWORD height;
    float u[UNIFORM_COUNT];
    
#ifdef AUDIO_DEBUG
    HANDLE audioFile;
    DWORD bytesWritten;
#endif
    
#ifdef CAPTURE_FRAMES
    int frameNumber;
    HANDLE frameFile;
    DWORD frameBytesWritten;
    char *frameBuffer;
#endif

	width = GetSystemMetrics(SM_CXSCREEN);
	height = GetSystemMetrics(SM_CYSCREEN);
	
#ifdef CAPTURE_FRAMES
    frameNumber = 0;
    frameBuffer = HeapAlloc(GetProcessHeap(), 0, width * height * 3 /* RGB8 */);
#endif

    hwnd = CreateWindow("static", NULL, WS_POPUP | WS_VISIBLE, 0, 0, width, height, NULL, NULL, NULL, 0);
    hdc = GetDC(hwnd);
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    wglMakeCurrent(hdc, wglCreateContext(hdc));
    
    for (i = 0; i < GL_EXT_FUNCTION_COUNT; i++)
        glExtFunctions[i] = wglGetProcAddress(glExtFunctionNames[i]);
    
    program = glCreateProgram();
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fs, 0);
    glCompileShader(fragmentShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glUseProgram(program);
    
    renderAudio();
    memcpy(audioBuffer, &riffHeader, sizeof(riffHeader));
    
#ifdef AUDIO_DEBUG
    // debug audio output
    audioFile = CreateFile("audio.wav", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    WriteFile(audioFile, audioBuffer, sizeof(audioBuffer), &bytesWritten, NULL);
    CloseHandle(audioFile);
#endif
    
    sndPlaySound((LPCSTR)audioBuffer, SND_ASYNC | SND_MEMORY);
    
	ShowCursor(FALSE);
	
    startTime = timeGetTime();
    while (!GetAsyncKeyState(VK_ESCAPE))
    {
        float time;
        
		// avoid 'not responding' system messages
		MSG msg;
		PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
		
#ifdef CAPTURE_FRAMES
        // capture at a steady 60fps
        time = (float)frameNumber / 60.0f * 140.0f / 60.0f;
        
        // stop at the end of the music
        if (((float)frameNumber / 60.0f) > (AUDIO_SAMPLES / 2 / 44100.0f))
            break;
#else
		time = (float)(timeGetTime() - startTime) * 0.001f; //* 140.0f / 60.0f;
#endif
        
		//time += 164.0f;
		
        u[0] = time; // time
		u[1] = (float)(time < 4.0f); // black
		u[1] += (time >= 324.0f) ? (time - 324.0f) * 0.25f : 0.0f;
		u[2] = 1.0f - (float)(time >= 68.0f && time < 260.0f); // spheres
		
		u[3] = 0.0f;
		if (time >= 164.0f && time < 168.0f)
			u[3] = 4.0 - (time - 164.0f);
		else if (time >= 180.0f && time < 184.0f)
			u[3] = 4.0 - (time - 180.0f);

        u[4] = (float)width;
        u[5] = (float)height;
        
        // hack - assume that the uniforms u[] will always be linked to locations [0-n]
        // given that they are the only uniforms in the shader, it is likely to work on all drivers
        for (i = 0; i < UNIFORM_COUNT; i++)
            glUniform1f(i, u[i]);
        
        glRects(-1, -1, 1, 1);
        
#ifdef CAPTURE_FRAMES
        // read back pixels
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, frameBuffer);
        
        // write ouput frame (skip existing ones)
        frameFile = CreateFile(frameFilename(frameNumber), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		if (frameFile)
		{
			WriteFile(frameFile, frameBuffer, width * height * 3, &frameBytesWritten, NULL);
			CloseHandle(frameFile);
		}
        
        frameNumber++;
#endif
        
        wglSwapLayerBuffers(hdc, WGL_SWAP_MAIN_PLANE);
    }
    
	ExitProcess(0);
}
