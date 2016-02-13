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

inline uint32
SafeTruncateInt64(uint64 Value)
{
	// TODO (peter) : Define max values uint32 etc.
		Assert(Value <= 0xFFFFFFFF);	// Never read files bigger then 4 gigabytes!
	uint32 Result = (uint32)Value;
	return(Result);
}

/*
Services that the platform layer provides to the game
*/
#if INTERNAL
struct debug_file_result 
{
	uint32 ContentSize;
	void *Content;
};
internal debug_file_result DEBUGPlatformReadEntireFile(char *Filename);
internal void DEBUGPlatformFreeFileMemory(void *Memory);
internal bool32 DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory);
#endif

/*
	Services that the game provides to the platform layer
*/
struct game_offscreen_buffer
{
	void *Memory;
	int Width;
	int Height;
	int Pitch;
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
	// TODO (Peter) : Add timing here
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
};

struct game_clock
{
	real32 SecondsElapsed;
};

// Takes input, bitmap buffer to use, sound buffer to use, timing information
internal void GameUpdateAndRender(game_memory *Memory, game_offscreen_buffer *Buffer, 
								game_sound_output_buffer *SoundBuffer, game_input *Input);

struct game_state
{
	int ToneHz;
	int xOffset;
	int yOffset;
};