#if !defined(WIN32_LAYER_H)
/**************************************************************************** 
* File:			game.cpp
* Version:		
* Date:			13-02-2016
* Creator:		Peter Verzijl
* Description:	Game Layer Main File
* Notice:		(c) Copyright 2015 by Peter Verzijl. All Rights Reserved.
***************************************************************************/
struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

struct win32_window_dimension
{
	int Width;
	int Height;
};

struct win32_sound_output
{
	int SamplesPerSecond;
	uint32 RunningSampleIndex;
	int BytesPerSample;
	DWORD SecondaryBufferSize;
	DWORD SafetyBytes;
	int LatencySampleCount;
};

struct win32_debug_time_marker 
{
	DWORD OutputPlayCursor;
	DWORD OutputWriteCursor;
	DWORD OutputLocation;
	DWORD OutputByteCount;
	DWORD ExpectedFlipPlayCursor;

	DWORD FlipPlayCursor;
	DWORD FlipWriteCursor;
};

struct win32_game_code 
{
	HMODULE GameCodeDLL;
	FILETIME DLLLastWriteTime;
	
	// IMPORTANT(Peter): These must be checked for 0 befor use!
	game_update_and_render *UpdateAndRender;
	game_get_sound_samples *GetSoundSamples;

	bool32 IsValid;
};

struct win32_replay_buffer
{
	HANDLE FileHandle;
	HANDLE MemoryMap;
	char ReplayFilename[MAX_PATH];
	void *MemoryBlock;
};

struct win32_state
{
	uint64 TotalSize;
	void *GameMemoryBlock;
	win32_replay_buffer ReplayBuffers[4];

	HANDLE RecordingHandle;
	int  InputRecordingIndex;

	HANDLE PlaybackHandle;
	int InputPlayingIndex;


	char EXEFilename[MAX_PATH];
	char *OnePastLastEXEFilenameSlash; 	
}; 

#define WIN32_LAYER_H
#endif