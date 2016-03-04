	/**************************************************************************** 
	* File:			game.h
	* Version:		0.0.1a
	* Date:			27-12-2015
	* Creator:		Peter Verzijl
	* Description:	Game Layer Header File
	* Notice:		(c) Copyright 2015 by Peter Verzijl. All Rights Reserved.
	***************************************************************************/
/*
	NOTE:
	INTERNAL
	0 - Public release code only
	1 - Developer code

	SLOW
	0 - No slow code, for performance checking.
	1 - Additional debug stuff that might hit performance
*/

// TODO(Peter): Implement math functions ourselves
#include <math.h>

	//
// NOTE(Peter): Types
//
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

#define PI32 3.1415926535897932384626433832795028841971693993751058209749445923078164062f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint32_t bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef size_t memory_index;

typedef float real32;
typedef double real64;

// TODO (peter) : Macro's, swap, min, max etc..
#define Kilobytes(value) ((value) * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) (Megabytes(value) * 1024)
#define Terabytes(value) (Gigabytes(value) * 1024)

#if SLOW
#define Assert(Expression) if (!(Expression)) { *(int *)0 = 0; }
#else
#define Assert(Expression)
#endif

#define ArrayCount(Array)  (sizeof(Array) / sizeof((Array)[0]))

#include "intrinsics.h"

inline uint32 SafeTruncateInt64(uint64 Value)
{
	// TODO (peter) : Define max values uint32 etc.
		Assert(Value <= 0xFFFFFFFF);	// Never read files bigger then 4 gigabytes!
	uint32 Result = (uint32)Value;
	return(Result);
}

struct thread_context
{
	int Placeholder;
};

/*
	Services that the platform layer provides to the game
*/
#if INTERNAL
// IMPORTANT(Peter): These are not for shipping code! 
// They are blocking and do not protect against lost data.
struct debug_read_file_result 
{
	uint32 ContentSize;
	void *Content;
};
// NOTE(Peter): These get passed to the game via the game_memory struct.
#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context *Thread, void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context *Thread, char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(thread_context *Thread,  char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif

/*
	NOTE(Peter): Services that the game provides to the platform layer
*/
struct game_offscreen_buffer
{
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

struct game_sound_output_buffer
{
	int SamplesPerSecond;
	uint32 SampleCount;
	int16 *Samples;
};

struct game_button_state
{
	int HalfTransitionCount;
	bool32 EndedDown;
};

// TODO(Peter): Change the keyboard input to also act as analogue
// Or, let the analog act as buttons.
struct game_controller_input
{
	bool32 IsConnected;
	bool32 IsAnalog;
	real32 StickAverageX;
	real32 StickAverageY;

	union 
	{
		game_button_state Buttons[16];
		struct 
		{
			game_button_state StickUp;
			game_button_state StickDown;
			game_button_state StickLeft;
			game_button_state StickRight;

			game_button_state DPadUp;
			game_button_state DPadDown;
			game_button_state DPadLeft;
			game_button_state DPadRight;

			game_button_state LeftShoulder;
			game_button_state RightShoulder;

			game_button_state Back;
			game_button_state Start;

			game_button_state AButton;
			game_button_state BButton;
			game_button_state XButton;
			game_button_state YButton;
		};
	};
};

struct game_input
{
	game_button_state MouseButtons[5];
	int32 MouseX, MouseY, MouseZ;

	real32 DeltaTime;		// Amount of seconds to advance the game by.

	game_controller_input Controllers[5];	// Four controllers and a keyboard on input 0
};
inline game_controller_input *GetController(game_input *Input, int ControllerIndex) 
{
	Assert(ControllerIndex < ArrayCount(Input->Controllers));
	game_controller_input *Result = &Input->Controllers[ControllerIndex];
	return (Result);
}

struct game_memory
{
	bool32 IsInitialized;

	uint64 PermanentStorageSize;
	void *PermanentStorage;

	uint64 TransientStorageSize;
	void *TransientStorage;

	debug_platform_free_file_memory * DEBUGPlatformFreeFileMemory;
	debug_platform_read_entire_file * DEBUGPlatformReadEntireFile;
	debug_platform_write_entire_file * DEBUGPlatformWriteEntireFile;
};

struct game_clock
{
	real32 SecondsElapsed;
};

#define GAME_UPDATE_AND_RENDER(name) void name(thread_context *Thread, game_memory *Memory, game_offscreen_buffer *Buffer, game_input *Input)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

// NOTE(Peter): This function induces audio latency if it takes longer than 1ms
// TODO(Peter): Measure the time it takes to do this function.
#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context *Thread, game_memory *Memory, game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

struct tile_map
{
	int32 CountX;
	int32 CountY;

	real32 UpperLeftX;
	real32 UpperLeftY;
	real32 TileSize;
	
	uint32 *Tiles;
};

struct loaded_bitmap
{
	int32 Width;
	int32 Height;
	uint32 *Pixels;
};

struct world
{
	// TODO(Peter): Beginners sparseness.
	int32 TileMapCountX;
	int32 TileMapCountY;

	tile_map *TileMaps;
};

struct game_state
{
	real32 PlayerX;
	real32 PlayerY;

	real32 tSine;
	loaded_bitmap TestImage;
};