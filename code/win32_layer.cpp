/**************************************************************************** 
* File:			win32_layer.cpp
* Version:		0.0.6a
* Date:			27-12-2015
* Creator:		Peter Verzijl
* Description:	Windows API Layer
* Notice:		(c) Copyright 2015 by Peter Verzijl. All Rights Reserved.
***************************************************************************/ 

/* Lots of TODO's
	- Saved game locations
	- Getting a handle to our own exe
	- Asset loading path
	- Threading
	- Raw Input (support multiple keyboards and +4 controllers)
	- Sleep / timeing (don't burn battery)
	- Clip cursor
	- Fullscreen
	- Set the cursor visiblity
	- QueryCancelAutoplay
	- Active app (when are we not active)
	- Blit speed improvement (BitBlt)
	- Hardware accereleration (OpenGL Direct3D DirectX)
	- Keyboard layouts
*/

#include "game.h"

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_layer.h"

// TODO(player) : This should maybe not be global?
global_variable bool GlobalRunning;
global_variable bool GlobalPause;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;

// XInput function signatures
// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(xinput_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable xinput_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(xinput_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable xinput_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// DirectSound function signature
// DirectSoundCreate
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);
DIRECT_SOUND_CREATE(DirectSoundCreateStub)
{
	return (0);
}
global_variable direct_sound_create *DirectSoundCreate_ = DirectSoundCreateStub;
#define DirectSoundCreate DirectSoundCreate_

internal int StringLength(char *String)
{
	int Result = 0;
	while(*String++)
	{
		++Result;
	}
	return(Result);
}

internal void ConcatStrings(size_t SourceACount, char *SourceA,
							size_t SourceBCount, char *SourceB,
							size_t DestCount, char *Dest)
{
	// NOTE(Peter): Super slow implementation.
	// TODO(Peter): Dest bounds checking. 
	for (int i = 0; i < SourceACount; ++i)
	{
		*Dest++ = *SourceA++;
	}
	for (int i = 0; i < SourceBCount; ++i)
	{
		*Dest++ = *SourceB++;
	}
	*Dest++ = 0; 
}

internal void Win32BuildEXEFilename(win32_state *Win32State, char *Filename,
									int DestCount, char *Dest)
{
	ConcatStrings(Win32State->OnePastLastEXEFilenameSlash - Win32State->EXEFilename, 
					Win32State->EXEFilename,
					StringLength(Filename), Filename,
					sizeof(DestCount), Dest);
	
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
	if (Memory)
	{
		VirtualFree(Memory,			// The address of the memory to free
					0,				// Must be 0 if MEM_RELEASE (releases the whole page)
					MEM_RELEASE);	// We want to just free it forever
	}
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
	debug_read_file_result Result = {}  ;
	HANDLE FileHandle = CreateFileA(Filename,
									GENERIC_READ,		// What do we want to do
									FILE_SHARE_READ,	// What can others do
									0,					// Security
									OPEN_EXISTING,		// Throw error if the asset doesn't OPEN_EXISTING
									0,					// Read only or hidden etc.
									0);					// Ignored when opening a file
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		if (GetFileSizeEx(FileHandle, &FileSize))
		{
			uint32 FileSize32 = SafeTruncateInt64(FileSize.QuadPart);
			Result.Content = VirtualAlloc(0, FileSize32, 
								MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if (Result.Content)
			{
				DWORD BytesRead;
				if (ReadFile(FileHandle,		 
							Result.Content,			// The pointer of the resulting buffer
							FileSize32,				// The file size
							&BytesRead,				// Amount of bytes read (for file size)
							0)						// If we want to use overlapped IO 
					&& (BytesRead == FileSize32))	// Check if we read the whole file!
				{
					Result.ContentSize = BytesRead;
				}
				else
				{
					// NOTE (peter) :  This defninitely fails if we want to read files < 4GB!!!!
					// If the read fails, free the allocated memory and return zero!
					DEBUGPlatformFreeFileMemory(Thread, Result.Content);
					Result.Content = 0;
				}
			}
			else
			{
				// TODO (peter) : Logging
			}
		}
		else
		{
			// TODO (peter) : Logging
		}
		CloseHandle(FileHandle);
	}
	else
	{
		// TODO (peter) : Logging
	}
	return (Result);
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
	bool32 Result = false;;
	HANDLE FileHandle = CreateFileA(Filename,
									GENERIC_WRITE,		// What do we want to do
									0,					// Lock the file
									0,					// Security
									CREATE_ALWAYS,		// Create if doesn't exist
									0,					// Read only or hidden etc.
									0);					// Ignored when opening a file
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten;
		if (WriteFile(FileHandle,
					Memory,
					MemorySize,
					&BytesWritten,
					0))				// No overlap 
		
		{
			// We got to read the file, yeahy!
			// Error success!
			Result = (BytesWritten == MemorySize);
		}
		else
		{
			// TODO (peter) : Logging
		}
		CloseHandle(FileHandle);
	}
	else
	{
		// TODO (peter) : Logging
		DWORD error = GetLastError();
	}
	return (Result);	
}

inline FILETIME Win32GetLastWriteTime(char *Filename)
{
	FILETIME LastWriteTime = {};

	WIN32_FILE_ATTRIBUTE_DATA FileData;
	if (GetFileAttributesEx(Filename, GetFileExInfoStandard, &FileData))
	{
		LastWriteTime = FileData.ftLastWriteTime;
	}

	return(LastWriteTime);
}

internal win32_game_code Win32LoadGameCode(char *SourceDLLName, char *TempDLLName)
{
	win32_game_code Result = {};
	
	Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);
	
	CopyFile(SourceDLLName, TempDLLName, FALSE);

	Result.GameCodeDLL = LoadLibraryA(TempDLLName);
	if (Result.GameCodeDLL)
	{
		Result.UpdateAndRender = (game_update_and_render *)
			GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
		Result.GetSoundSamples = (game_get_sound_samples *)
			GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");

		Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
	}

	// Set these to stub functions so nothing fails.
	if (!Result.IsValid)
	{
		Result.UpdateAndRender = 0;
		Result.GetSoundSamples = 0;
	}

	return (Result);
}

internal void Win32UnloadGameCode(win32_game_code *GameCode) 
{
	if (GameCode->GameCodeDLL) 
	{
		FreeLibrary(GameCode->GameCodeDLL);
		GameCode->GameCodeDLL = 0;
	}
	GameCode->IsValid = false;
	GameCode->UpdateAndRender = 0;
	GameCode->GetSoundSamples = 0;
}

internal void Win32LoadXInput(void)
{
	// Load XInput 1.4 first
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (!XInputLibrary)
	{
		// TODO (peter) : Diagnostic, which XInput we tried to load
		XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}

	if (XInputLibrary)
	{
		XInputGetState = (xinput_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
		if(!XInputGetState) { XInputGetState = XInputGetStateStub; }
		XInputSetState = (xinput_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
		if(!XInputSetState) { XInputSetState = XInputSetStateStub; }

		// TODO (peter) : Diagnostic Why couldn't we load Xinput?
	}
	else
	{
		// TODO (peter) : Diagnostic Why couldn't we load Xinput?
	}
}

internal void Win32InitDirectSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	// Load library
 	HMODULE DirectSoundLibrary = LoadLibraryA("dsound.dll");

	if (DirectSoundLibrary) 
	{
		DirectSoundCreate = (direct_sound_create*)GetProcAddress(DirectSoundLibrary, "DirectSoundCreate");
		
		// TODO(peter) : Doulbe-check that this works on XP - DirectSound8 or 7?
		LPDIRECTSOUND DirectSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;		// Only supported format
			WaveFormat.nChannels = 2;						// Sterio sound
			WaveFormat.nSamplesPerSec = SamplesPerSecond;	// 
			WaveFormat.wBitsPerSample = 16;					// 16-bit audio
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;							// Extra info, we don't need this

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))		// Set buffer format
			{
				DSBUFFERDESC BufferDescription = {};
				// Maybe play audio in the background even when the game is not in focus: DSBCAPS_GLOBALFOCUS
				BufferDescription.dwSize = sizeof(BufferDescription);		// Size of description
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;			// Flags
				BufferDescription.dwBufferBytes = 0;						// Primary buffer needs to be size zero

				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(
											&BufferDescription,
											&PrimaryBuffer,
											0)))
				{
					HRESULT error = PrimaryBuffer->SetFormat(&WaveFormat);
					if (SUCCEEDED(error))
					{

					}
					else
					{
						// TODO (peter) : Diagnostic, why we couldn't set the wave format 
					}
				}
				else
				{
					// TODO (peter) : We couldn't get the buffers we needed :(
				}
			}
			else
			{
				// TODO (peter) : Diagnostic, why we couldn't set cooperative level
			}

			// NOTE (peter) : Better play cursor accuracy DSBCAPS_GETCURRENTPOSITION2
			DSBUFFERDESC BufferDescription = {};
			// Maybe play audio in the background even when the game is not in focus: DSBCAPS_GLOBALFOCUS
			BufferDescription.dwSize = sizeof(BufferDescription);		// Size of description
			BufferDescription.dwFlags = 0;								// Flags
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;

			HRESULT error = DirectSound->CreateSoundBuffer(
										&BufferDescription,
										&GlobalSecondaryBuffer,
										0);
			if (SUCCEEDED(error))
			{
				
			}
			else
			{
				// TODO (peter) : We couldn't get the buffers we needed :(
			}
		}
	}
	else
	{
		// TODO (peter) : Diagnostic, why we couldn't load direct sound...
	}

	// Get DirectSound object

	// Make primary buffer

}

/**
 * @brief Returns a struct with the window dimensions.
 * @details Returns a new win32_window_dimension struct with
 * the width and height filled in. Takes a window handle. 
 * @param Window The window handle to get the dimensions of.
 * @return The filled in win32_window_dimension.
 */
internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;
	
	return(Result);
}

/**
 * @brief Resizes the DIBSection on windows.
 * @details Resizes the backbuffer we use to draw our graphics into.
 * The backbuffer is saved in a bitmap. It creates a device context,
 * a and destroys the old bitmap handle before creating a new one.
 * @param width The width of the backbuffer.
 * @param height The height of the backbuffer.
 */
internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, 
									int width, int height)
{
	// TODO(peter) : Free memory in a nice way
	// Maybe don't free first, but free after, then free first if that fails

	if (Buffer->Memory)
	{
		// Release memeory pages
		VirtualFree(Buffer->Memory,		// The start address for the memory
					0,					// Memory size 0 since we want to free everything
					MEM_RELEASE);		// Release ipv decommit
	}

	Buffer->Width = width;
	Buffer->Height = height;
	Buffer->BytesPerPixel = 4;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;	// Negative value so we can refer to our pixel rows from top to bottom
	Buffer->Info.bmiHeader.biPlanes = 1;				// 1 color plane, legacy
	Buffer->Info.bmiHeader.biBitCount = 32;				// Bits per color (8 bytes alignment)
	Buffer->Info.bmiHeader.biCompression = BI_RGB;
													// Rest is cleared to 0 due to being static
	
	// NOTE(peter) We don't need to have to get a DC or a BitmapHandle
	// 8 bits red, 8 bits green, 8 bits blue and 8 bits padding (memory aligned accessing) 
	int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;

	// Allocate enough pages to store the bitmap.
	Buffer->Memory = VirtualAlloc(0,				// Starting address, 0 is ok for us
	  							BitmapMemorySize,
	  							MEM_COMMIT,			// Tell windows we are using the memory and not just reserving
	  							PAGE_READWRITE);	// Access codes, we just want to read and write the memory

	Buffer->Pitch = width * Buffer->BytesPerPixel;
	
	// TODO(peter) : Clear to black probably
}

/**
 * @brief Swaps the old bitmap for the backbuffer.
 * @details Gives the new bitmap memory and info to windows so it can draw
 * our backbuffer.
 * @param DeviceContext The window drawing DeviceContext
 * @param x x coordinate of the screen.
 * @param y y coordinate of the screen.
 * @param width Width of the screen.
 * @param height Height of the screen.
 */
internal void Win32SwapBackBuffer(HDC DeviceContext, 
								win32_offscreen_buffer *Buffer,
								int WindowWidth, int WindowHeight)
{
	int OffsetX = 10;
	int OffsetY = 10;

	// Black clear background
	PatBlt(DeviceContext, 0, 0, OffsetX, WindowHeight, BLACKNESS);
	PatBlt(DeviceContext, 0, 0, WindowWidth, OffsetY, BLACKNESS);
	PatBlt(DeviceContext, OffsetX + Buffer->Width, 0, WindowWidth, WindowHeight, BLACKNESS);
	PatBlt(DeviceContext, 0, OffsetY + Buffer->Height, WindowWidth, WindowHeight, BLACKNESS);

	// NOTE(Peter): Scaling for the release, for now, keep the pixels 1-to-1
	StretchDIBits(DeviceContext, 
					OffsetX, OffsetY, Buffer->Width, Buffer->Height,
					0, 0, Buffer->Width, Buffer->Height,
					Buffer->Memory,
					&Buffer->Info,
					DIB_RGB_COLORS,			// What kind of picture (buffer) RGB or Paletized
					SRCCOPY);				// Bitwise operator on the buffers, we copy
}

/**
* @brief The function that windows uses to call us back.
* @details This function is used as a callback that windows can use to
* call us back. It gets send messages for redrawing, pointer events, key presses,
* etc. All those messages will thus be handled in this class.
* @param Window The window handle that is passed so we know which window it is referencing.
* @param Message The message send by windows.
* @param wParam [description]
* @param lParam [description]
* @return The results we want to send back to windows.
*/
LRESULT CALLBACK Win32MainWindowCallback(HWND Window, 
										UINT Message, 
										WPARAM wParam, 
										LPARAM lParam)
{
	LRESULT Result = 0;

	switch(Message) 
	{
		case WM_SIZE:
		{
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
			// Make the game translucent when the window is not into focus,
			// such that we can see our code.
			if (wParam == TRUE)
			{
				SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
			}
			else
			{
				SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 100, LWA_ALPHA);
			}

		} break;
		
		case WM_DESTROY:
		{
			// TODO(peter) : This is not good, maybe restart the game and save if possible.
			GlobalRunning = false;
		} break;
		
		// Input
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			Assert(!"Error: Keyboard input trough non-dispatch message!");
		}break;


		case WM_CLOSE:
		{
			// TODO(peter) : Maybe ask the player if he really wants to close without saving.
			GlobalRunning = false;
		} break;
		
		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int x = Paint.rcPaint.left;
			int y = Paint.rcPaint.top;
			int width = Paint.rcPaint.right - Paint.rcPaint.left;
			int height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			
			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32SwapBackBuffer(DeviceContext, &GlobalBackBuffer, Dimension.Width, Dimension.Height);
			EndPaint(Window, &Paint);
		} break;

		default:
		{
//			OutputDebugStringA("default\n");
			Result = DefWindowProc(Window, Message, wParam, lParam);	// Catch all, default handling messages
		}
	}

	return(Result);
}

internal void Win32ClearBuffer(win32_sound_output *SoundOutput)
{
	// TODO (peter) : Look at the buffer.
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size; 
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0,					// The position we want to write at
											SoundOutput->SecondaryBufferSize,		// Amount of bytes to write to audio buffer
											&Region1, &Region1Size,	// Start of writing region
											&Region2, &Region2Size,	// The wrapping part of the buffer, NULL if it fits
											0)))					// Flags
	{
		// TODO (peter) : Region sizes are valid and multiples of 16
		int8 *DestSample = (int8 *)Region1;
		for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
		{
			*DestSample++ = 0;
		}

		DestSample = (int8 *)Region2;
		for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
		{
			*DestSample++ = 0;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, 
									DWORD ByteToLock, DWORD BytesToWrite, 
									game_sound_output_buffer *SourceBuffer)
{
	// TODO (peter) : Look at the buffer.
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size; 
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock,				// The position we want to write at
											BytesToWrite,				// Amount of bytes to write to audio buffer
											&Region1, &Region1Size,		// Start of writing region
											&Region2, &Region2Size,		// The wrapping part of the buffer, NULL if it fits
											0)))						// Flags
	{
		/* 
		 * int16 int16 int16 int16 int16 
		 * LEFT  RIGHT LEFT  RIGHT LEFT
		 * HIHG LOW HIGH LOW HIGH LOW 
		*/
		// TODO (peter) : Region sizes are valid and multiples of 16
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		int16 *DestSample = (int16 *)Region1;
		int16 *SourceSample = SourceBuffer->Samples;
		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		DestSample = (int16 *)Region2;
		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

internal void Win32ProcessKeyboardMessage(game_button_state *NewState,
											bool32 IsDown)
{
	// TODO(Peter): This happens when switching apps
	if (NewState->EndedDown != IsDown)
	{
		NewState->EndedDown = IsDown;
		++NewState->HalfTransitionCount;
	}
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, 
											game_button_state *OldState,
											game_button_state *NewState,
											DWORD ButtonBit)
{
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
	real32 result = 0;
	if (Value < -DeadZoneThreshold)
	{
		result = (real32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));	// Normalize
	}
	else if (Value > DeadZoneThreshold)
	{
		result = (real32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));	// We need to do this, since negative values are one bigger!
	}	// Inside of deadzone thus 0
	return(result);
}

internal void Win32GetInputFileLocation(win32_state *State, bool32 InputStream, int SlotIndex,
										int DestCount, char *Dest)
{
	char Temp[64];
	wsprintf(Temp, "loop_edit_ %d_%s.gir", SlotIndex, InputStream ? "input" : "state");
	Win32BuildEXEFilename(State, Temp, DestCount, Dest);
}

internal win32_replay_buffer* Win32GetReplayBuffer(win32_state *State, int unsigned ReplayBufferIndex)
{
	Assert(ReplayBufferIndex < ArrayCount(State->ReplayBuffers));
	win32_replay_buffer *Result = &State->ReplayBuffers[ReplayBufferIndex];
	return(Result);
}

internal void Win32BeginRecordingInput(win32_state *State, int InputRecordingIndex)
{
	win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputRecordingIndex);
	if (ReplayBuffer->MemoryBlock)
	{
		State->InputRecordingIndex = InputRecordingIndex;
		
		char Filename[MAX_PATH];
		Win32GetInputFileLocation(State, true, InputRecordingIndex, sizeof(Filename), Filename);
		State->RecordingHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
#if 0
		LARGE_INTEGER FilePosition;
		FilePosition.QuadPart = State->TotalSize;
		SetFilePointerEx(State->RecordingHandle, FilePosition, 0, FILE_BEGIN);
#endif	
		CopyMemory(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
	}
}

internal void Win32EndRecordingInput(win32_state *State)
{
	CloseHandle(State->RecordingHandle);
	State->InputRecordingIndex = 0;
}

internal void Win32BeginInputPlayback(win32_state *State, int InputPlayingIndex)
{
	win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputPlayingIndex);
	if (ReplayBuffer->MemoryBlock)
	{
		State->InputPlayingIndex = InputPlayingIndex;

		char Filename[MAX_PATH];
		Win32GetInputFileLocation(State, true, InputPlayingIndex, sizeof(Filename), Filename);
		State->PlaybackHandle = CreateFileA(Filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
#if 0
		LARGE_INTEGER FilePosition;
		FilePosition.QuadPart = State->TotalSize;
		SetFilePointerEx(State->PlaybackHandle, FilePosition, 0, FILE_BEGIN);
#endif
		CopyMemory(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
	}
}

internal void Win32EndInputPlayback(win32_state *State)
{
	CloseHandle(State->PlaybackHandle);
	State->InputPlayingIndex = 0;
}

internal void Win32RecordInput(win32_state *State, game_input *Input)
{
	// Write to file
	DWORD BytesWritten;
	WriteFile(State->RecordingHandle, Input, sizeof(*Input), &BytesWritten, 0);
}	

internal void Win32PlayBackInput(win32_state *State, game_input *Input) 
{
	// Read from file
	DWORD BytesRead = 0;
	if (ReadFile(State->PlaybackHandle, Input, sizeof(*Input), &BytesRead, 0))
	{
		if (BytesRead == 0) 
		{
			// NOTE(Peter): We've hit the end of the stream, go back to the beginning
			int PlayingIndex = State->InputPlayingIndex;
			Win32EndInputPlayback(State);
			Win32BeginInputPlayback(State, PlayingIndex);
			ReadFile(State->PlaybackHandle, Input, sizeof(*Input), &BytesRead, 0);
		}
	}
}

internal void Win32ProcessPendingMessages(win32_state *State, game_controller_input *KeyboardController)
{
	MSG Message;
	while (PeekMessage(&Message, 		// Pointer to message
						0, 				// The window handle, 0 get all messages
						0, 				// Message filter inner bounds
						0, 				// .. outer bounds
						PM_REMOVE))
	{
		switch(Message.message)
		{
			case WM_QUIT:
			{
				GlobalRunning = false;
			} break;

			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				uint32 VKCode = (uint32)Message.wParam;
				bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);		// Check if the key came up	
				bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);		// Check if the key is up now
				if (WasDown != IsDown)									// Remove key repeat messages
				{
					if (VKCode == 'W')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->StickUp, IsDown);
					}
					else if (VKCode == 'A')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->StickLeft, IsDown);
					}
					else if (VKCode == 'S')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->StickDown, IsDown);
					}
					else if (VKCode == 'D')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->StickRight, IsDown);
					}
					else if (VKCode == 'Q')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
					}
					else if (VKCode == 'E')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
					}
					else if (VKCode == 'X')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->AButton, IsDown);
					}
					else if (VKCode == 'Z')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->BButton, IsDown);
					}
					else if (VKCode == VK_UP)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->DPadUp, IsDown);
					}
					else if (VKCode == VK_DOWN)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->DPadDown, IsDown);
					}
					else if (VKCode == VK_LEFT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->DPadLeft, IsDown);
					}
					else if (VKCode == VK_RIGHT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->DPadRight, IsDown);
					}
					else if (VKCode == VK_ESCAPE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
					}
					else if (VKCode == VK_SPACE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
					}
#if INTERNAL
					else if (VKCode == 'P' && IsDown)
					{
						GlobalPause = !GlobalPause;
					}
					else if (VKCode == 'L') // Loop mode start recording
					{
						if (IsDown)
						{
							if (State->InputPlayingIndex == 0) 
							{
								if (State->InputRecordingIndex == 0)
								{
									Win32BeginRecordingInput(State, 1);
								}
								else
								{
									Win32EndRecordingInput(State);
									Win32BeginInputPlayback(State, 1);
								}
							}
							else 
							{
								Win32EndInputPlayback(State);
							}
						}
					}
#endif
				}
				// Force exit alt-F4
				bool AltKeyWasDown = ((Message.lParam & (1 << 29)) != 0);
				if (VKCode == VK_F4 && AltKeyWasDown)
				{
					GlobalRunning = false;
				}
			} break;

			default:
			{
				TranslateMessage(&Message);	// Takes message and gets ready for keyboard events etc.
				DispatchMessage(&Message);
			}break;
		}
	}
}

inline LARGE_INTEGER Win32GetWallClock(void)
{
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return (Result);
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	real32 Result = ((real32)(End.QuadPart - Start.QuadPart) / 
					(real32)GlobalPerfCountFrequency);
	return (Result);
}

#if 0
internal void Win32DebugDrawVertical(win32_offscreen_buffer *BackBuffer, 
									int x, int Top, int Bottom, uint32 Color) 
{
	if (Top <= 0)
	{
		Top = 0;
	}

	if (Bottom > BackBuffer->Height)
	{
		Bottom = BackBuffer->Height;
	}

	if (x >= 0 && x < BackBuffer->Width)
	{
		uint8 *Pixel = ((uint8*)BackBuffer->Memory + 
						x * BackBuffer->BytesPerPixel + 
						Top * BackBuffer->Pitch);
		for (int y = Top; y < Bottom; ++y)
		{
			*(uint32 *)Pixel = Color;
			Pixel += BackBuffer->Pitch;
		}
	}
}

internal void Win32DrawSoundBufferMarker(win32_offscreen_buffer *BackBuffer, 
										win32_sound_output *SoundOutput,
										real32 c, int PadX, int Top, int Bottom,
										DWORD Value, uint32 Color)
{
	real32 XReal32 = (c * (real32)Value);
	int x = PadX + (int)XReal32;
	Win32DebugDrawVertical(BackBuffer, x, Top, Bottom, Color);
}

internal void Win32DebugSyncDisplay(win32_offscreen_buffer *BackBuffer, 
									int MarkerCount, win32_debug_time_marker *Markers,
									int Current, 
									win32_sound_output *SoundOutput, real32 TargetSecondsPerFrame)
{
	int PadX = 16;
	int PadY = 16;

	int LineHeight = 64;

	real32 c = ((real32)BackBuffer->Width - 2 * PadX) / (real32)SoundOutput->SecondaryBufferSize;	// Get the buffersize ratio in pixels
	for (int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
	{
		win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
		
		Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);

		DWORD PlayColor = 0xFFFFFFFF;
		DWORD WriteColor = 0xFFFF0000;
		DWORD ExpectedFlipColor = 0xFFFFFF00;
		DWORD PlayWindowColor = 0xFFFF00FF;

		int Top = PadY;
		int Bottom = PadY + LineHeight; 
		if (MarkerIndex == Current) 
		{
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;

			int FirstTop = Top;

			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, c, PadX, Top, 
										Bottom, ThisMarker->OutputPlayCursor, PlayColor);
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, c, PadX, Top, 
										Bottom, ThisMarker->OutputWriteCursor, WriteColor);
			
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;
			
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, c, PadX, Top, 
										Bottom, ThisMarker->OutputLocation, PlayColor);
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, c, PadX, Top, 
										Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);

			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;

			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, c, PadX, FirstTop, 
										Bottom, ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
		}
		Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, c, PadX, Top, 
									Bottom, ThisMarker->FlipPlayCursor, PlayColor);
		Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, c, PadX, Top, 
									Bottom, ThisMarker->FlipWriteCursor, WriteColor);
		Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, c, PadX, Top,
									Bottom, ThisMarker->FlipPlayCursor + 
									480 * SoundOutput->BytesPerSample, PlayWindowColor);
	}
}
#endif

internal void Win32GetEXEFilename(win32_state *Win32State)
{
	// NOTE(Peter): Never use MAX_PATH since it is not really the max file path!!!
	DWORD SizeOfFilename = GetModuleFileNameA(0, Win32State->EXEFilename, 
											sizeof(Win32State->EXEFilename));
	for (char *scan = Win32State->EXEFilename; *scan; ++scan)
	{
		if (*scan == '\\')
		{
			Win32State->OnePastLastEXEFilenameSlash = scan + 1;
		}
	}
}

int CALLBACK wWinMain(HINSTANCE Instance,	// Module handle 
					HINSTANCE PrevInstance, 
					PWSTR lpCmdLine, 
					int CommandLine)
{
	// This is the stat of the current win32 instance, contains loop recording data etc.
	win32_state Win32State = {};

	Win32GetEXEFilename(&Win32State);

	char SourceGameCodeDLLPath[MAX_PATH];
	Win32BuildEXEFilename(&Win32State, "game.dll",
						sizeof(SourceGameCodeDLLPath), SourceGameCodeDLLPath);

	char TempGameCodeDLLPath[MAX_PATH];
	Win32BuildEXEFilename(&Win32State, "game_temp.dll",
						sizeof(TempGameCodeDLLPath), TempGameCodeDLLPath);

	LARGE_INTEGER PerfCountFrequencyResult;	// Fixed at boot
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	// This sets the threading scheduler to be at 1ms granularity.
	// Sets the sleep to also be more granular
	UINT DesiredSchedulerMS = 1;
	bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

	Win32LoadXInput();

	WNDCLASS WindowClass = {};                          	// Initialize to zero

	Win32ResizeDIBSection(&GlobalBackBuffer, 960, 540);		// Set window size

	WindowClass.style = CS_HREDRAW |						// Redraw whole window ipv just that part
						CS_VREDRAW;                     	// Bit flags for window style
	WindowClass.lpfnWndProc = Win32MainWindowCallback;      // Pointer to our windows callback
	WindowClass.hInstance = Instance;                   	// The window instance handle for talking to windows about our window.
	// TODO(Peter): Insert icon here...
	WindowClass.hIcon;                                  	// The icon for our game in the window
	WindowClass.lpszClassName = "Peters Game Engine";   	// The window name

	if (RegisterClassA(&WindowClass))						// Register the window class 
	{
		HWND Window = 
			CreateWindowExA(
							/*WS_EX_TOPMOST|*/WS_EX_LAYERED,	// Extended style bit field
							WindowClass.lpszClassName,
							"Peters Game Engine",
							WS_OVERLAPPEDWINDOW|WS_VISIBLE,	// Window Style
							CW_USEDEFAULT,					// x coordinate
							CW_USEDEFAULT,					// Y coordinate
							CW_USEDEFAULT,					// window width
							CW_USEDEFAULT,					// window height
							0,								// Window to parent to
							0,								// Windows menu
							Instance,						// hInstance
							0);								// Message that would come in as WM_CREATE
		if (Window) 
		{
			// TODO(peter): How do we query this on Windows?
			int MonitorRefreshHz = 60;
			HDC RefreshDC = GetDC(Window);
			int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
			ReleaseDC(Window, RefreshDC);
			if (Win32RefreshRate > 1)
			{
				MonitorRefreshHz = Win32RefreshRate;
			}
			real32 GameUpdateHz = (MonitorRefreshHz / 2.0f);
			real32 TargetSecondsPerFrame = 1.0f/(real32)GameUpdateHz;

			win32_sound_output SoundOutput = {};

			SoundOutput.SamplesPerSecond	= 48000;
			SoundOutput.BytesPerSample		= sizeof(int16) * 2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
			// TODO(Peter): Look how much this needs to be.
			SoundOutput.SafetyBytes = (int)((((real32)SoundOutput.SamplesPerSecond * (real32)SoundOutput.BytesPerSample) / GameUpdateHz) / 3.0f);
			Win32InitDirectSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
			
			// Get messages from the message queue
			// Primitive game loop
			GlobalRunning = true;
			
			// TODO(Peter): Do we not want to pass MEM_LARGE_PAGES on Windows XP?
			// Call ajust token privilages to make large pages work.
			int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, 
													MEM_RESERVE | MEM_COMMIT /*| MEM_LARGE_PAGES*/, 
													PAGE_READWRITE);
#if INTERNAL
			LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
			LPVOID BaseAddress = 0;
#endif
			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TransientStorageSize = Gigabytes((uint64)1);

			GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
			GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
			GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
			
			Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
			Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize,
														MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
			GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage +	// Cast this to uint8 so we advance the pointer in bytes! 
											GameMemory.PermanentStorageSize);

			for (int ReplayIndex = 0; ReplayIndex < ArrayCount(Win32State.ReplayBuffers); ++ReplayIndex)
			{
				// Allocate and map the memory used for the looping code on a file.
				// NOTE(Peter): This is all for creating a backing store for the loop editing.
				win32_replay_buffer *ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];
				
				Win32GetInputFileLocation(&Win32State, false, ReplayIndex, 
											sizeof(ReplayBuffer->ReplayFilename), 
											ReplayBuffer->ReplayFilename);

				ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->ReplayFilename, 
														GENERIC_READ|GENERIC_WRITE, 
														0, 0, CREATE_ALWAYS, 0, 0); 
 				
 				DWORD MaxSizeHigh = (Win32State.TotalSize >> 32);
 				DWORD MaxSizeLow = (Win32State.TotalSize & 0xFFFFFFFF);
				ReplayBuffer->MemoryMap = CreateFileMapping(ReplayBuffer->FileHandle, 0, 
															PAGE_READWRITE,
															MaxSizeHigh, MaxSizeLow, 0);
				ReplayBuffer->MemoryBlock =  MapViewOfFile(ReplayBuffer->MemoryMap, 
															FILE_MAP_ALL_ACCESS, 0, 0,
															Win32State.TotalSize);
				if (ReplayBuffer->MemoryBlock)
				{
					// Succeeded...
				}
				else
				{
					// TODO(Peter): Logging
				}
			}

			if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage) 
			{
				// Input handling
				game_input Input[2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];

				DWORD AuidoLatencyBytes = 0;
				real32 AudioLatencySeconds = 0;
				bool32 SoundIsValid = false;

				// Check the time
				LARGE_INTEGER LastCounter = Win32GetWallClock();
				LARGE_INTEGER FlipWallClock = Win32GetWallClock();

				int DebugTimeMarkerIndex = 0;
				win32_debug_time_marker DebugTimeMarkers[30] = {0};

				// Load game code
				win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLPath, 
														TempGameCodeDLLPath);

				// Start of game loop
				int64 LastCycleCount = __rdtsc();
				while(GlobalRunning)
				{
					// Update Delta time
					NewInput->DeltaTime = TargetSecondsPerFrame;
					
					FILETIME NewDLLWriteTime =  Win32GetLastWriteTime(SourceGameCodeDLLPath);
					if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0) 
					{
						Win32UnloadGameCode(&Game);
						Game = Win32LoadGameCode(SourceGameCodeDLLPath, TempGameCodeDLLPath);
					}

					// Check if we got a message
					game_controller_input *OldKeyboardController = GetController(OldInput, 0);
					game_controller_input *NewKeyboardController = GetController(NewInput, 0);
					*NewKeyboardController = {};
					// Preserve the ended down state of the keyboard.
					NewKeyboardController->IsConnected = true;
					for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
					{
						NewKeyboardController->Buttons[ButtonIndex].EndedDown = 
							OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}

					Win32ProcessPendingMessages(&Win32State, NewKeyboardController);

					if (!GlobalPause)
					{
						POINT MousePosition;
						GetCursorPos(&MousePosition);
						ScreenToClient(Window, &MousePosition);	// Convert to relative window coordintes
						NewInput->MouseX = MousePosition.x;
						NewInput->MouseY = MousePosition.y;
 						NewInput->MouseZ = 0;					// TODO(Peter): Mouse wheel support?
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0], 
													GetKeyState(VK_LBUTTON) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1], 
													GetKeyState(VK_RBUTTON) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2], 
													GetKeyState(VK_MBUTTON) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3], 
													GetKeyState(VK_XBUTTON1) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4], 
													GetKeyState(VK_XBUTTON2) & (1 << 15));
 
						// TODO(peter) : Don't poll disconnected controllers, bug in XInput causes issues there. 
 						// TODO(peter) : Poll input more frequently than every frame?
						DWORD MaxControllerCount = 1 + XUSER_MAX_COUNT;		// Four controllers plus one keyboard
						if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
						{
							MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
						}
						for (DWORD ControllerIndex = 0; 
							ControllerIndex < MaxControllerCount; 
							++ControllerIndex)
						{
							DWORD OurControllerIndex = ControllerIndex + 1;
							game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
							game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

							XINPUT_STATE ControllerState;
							if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
							{
								NewController->IsConnected = true;
								NewController->IsAnalog = OldController->IsAnalog;

								// Controller is plugged in
								// TODO(peter) : See if the ControllerState.dwPacketNumber changes too fast
								XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

								bool32 LeftThumb = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
								bool32 RightThumb = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);

								// TODO (peter) : Min/Max macros
								NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX,
																		XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY,
																		XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								
								if ((NewController->StickAverageX != 0.0f) || 
									(NewController->StickAverageY != 0.0f))
								{
									NewController->IsAnalog = true;
								}

								// DPad analog override
								// TODO(Peter): Is this a good idea?
								if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
								{
									NewController->StickAverageY = 1.0f;
									NewController->IsAnalog = false;
								}
								if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
								{
									NewController->StickAverageY = -1.0f;
									NewController->IsAnalog = false;
								}
								if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
								{
									NewController->StickAverageX = -1.0f;
									NewController->IsAnalog = false;
								}
								if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
								{
									NewController->StickAverageX = 1.0f;
									NewController->IsAnalog = false;
								}


								// Analogue stick as buttons
								real32 Threshold = 0.5f;
								Win32ProcessXInputDigitalButton(
										(NewController->StickAverageY < -Threshold)? 1 : 0, 
																&OldController->StickUp, 
																&NewController->StickUp,
																1);
								Win32ProcessXInputDigitalButton(
										(NewController->StickAverageY > Threshold)? 1 : 0, 
																&OldController->StickDown, 
																&NewController->StickDown,
																1);
								Win32ProcessXInputDigitalButton(
										(NewController->StickAverageX > Threshold)? 1 : 0, 
																&OldController->StickRight, 
																&NewController->StickRight,
																1);
								Win32ProcessXInputDigitalButton(
										(NewController->StickAverageX < -Threshold)? 1 : 0, 
																&OldController->StickLeft, 
																&NewController->StickLeft,
																1);

								// Buttons
								Win32ProcessXInputDigitalButton(Pad->wButtons, 
																&OldController->DPadUp, 
																&NewController->DPadUp,
																XINPUT_GAMEPAD_DPAD_UP);
								Win32ProcessXInputDigitalButton(Pad->wButtons, 
																&OldController->DPadDown,
																&NewController->DPadDown,
																XINPUT_GAMEPAD_DPAD_DOWN);
								Win32ProcessXInputDigitalButton(Pad->wButtons, 
																&OldController->DPadLeft,
																&NewController->DPadLeft,
																XINPUT_GAMEPAD_DPAD_LEFT);
								Win32ProcessXInputDigitalButton(Pad->wButtons, 
																&OldController->DPadRight, 
																&NewController->DPadRight,
																XINPUT_GAMEPAD_DPAD_RIGHT);
								
								Win32ProcessXInputDigitalButton(Pad->wButtons, 
																&OldController->Start, 
																&NewController->Start,
																XINPUT_GAMEPAD_START);
								Win32ProcessXInputDigitalButton(Pad->wButtons, 
																&OldController->Back, 
																&NewController->Back,
																XINPUT_GAMEPAD_BACK);

								Win32ProcessXInputDigitalButton(Pad->wButtons, 
																&OldController->LeftShoulder, 
																&NewController->LeftShoulder,
																XINPUT_GAMEPAD_LEFT_SHOULDER);
								Win32ProcessXInputDigitalButton(Pad->wButtons, 
																&OldController->RightShoulder,
																&NewController->RightShoulder,
																XINPUT_GAMEPAD_RIGHT_SHOULDER);

								Win32ProcessXInputDigitalButton(Pad->wButtons,
																&OldController->AButton,
																&NewController->AButton,
																XINPUT_GAMEPAD_A);
								Win32ProcessXInputDigitalButton(Pad->wButtons,
																&OldController->BButton,
																&NewController->BButton,
																XINPUT_GAMEPAD_B);
								Win32ProcessXInputDigitalButton(Pad->wButtons, 
																&OldController->XButton,
																&NewController->XButton,
																XINPUT_GAMEPAD_X);
								Win32ProcessXInputDigitalButton(Pad->wButtons, 
																&OldController->YButton,
																&NewController->YButton,
																XINPUT_GAMEPAD_Y);
							}
							else
							{
								// Controller is not available
								NewController->IsConnected = false;
							}
						}

						thread_context Thread = {};

						game_offscreen_buffer Buffer = {};
						Buffer.Memory = GlobalBackBuffer.Memory;
						Buffer.Width = GlobalBackBuffer.Width;
						Buffer.Height = GlobalBackBuffer.Height;
						Buffer.Pitch = GlobalBackBuffer.Pitch;
						Buffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;
						
						// Loop input recording and playback. 
						if (Win32State.InputRecordingIndex)
						{
							Win32RecordInput(&Win32State, NewInput);
						}
						if (Win32State.InputPlayingIndex)
						{
							Win32PlayBackInput(&Win32State, NewInput);
						}
						if (Game.UpdateAndRender) 
						{
							Game.UpdateAndRender(&Thread, &GameMemory, &Buffer, NewInput);
						}

						LARGE_INTEGER AudioWallClock = Win32GetWallClock();
						real32 FlipTillAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

						DWORD PlayCursor;
						DWORD WriteCursor;
						if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
						{
							/* Compute amount of sound to write to buffer
								When we wake up to write audio we look where the play cursor is, and then
								forcas where we will think the play cursor will be after one frame.
								We have two cases of audio output:
								1.	We have non-latend auto, which means that the write cursor is before the
									frame flip. 
									[pc]----[wc]--|-------------|---------|
									Here we write all the audio to the page flip, and untill the next pagefilp.
									--------[wc]ww|wwwwwwwwwwwww|---------|
								2.	We have latent audio, which means that the write cursor is after the
									frame flip. 
									[pc]----------|---[wc]------|---------|
									Here we write a whole frame of audio to the sound buffer, plus a bit
									of safety margin.
									--------------|---[WC]wwwwww|wwwwww---|
								All of this is done with regartd to a guard amount which is the 'granularity'
								of which the sound card checks it's curosors.
							 */
							if (!SoundIsValid)
							{
								SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
								SoundIsValid = true;
							}

							// TODO(Peter): If it is the first time, write from the write cursor
							// TODO (peter) : This is a second late, which is no good for sound effects
							DWORD ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % 
												SoundOutput.SecondaryBufferSize);
							
							// Subtract already spend time in update loop from the expected frame boundry bytes
							DWORD ExpectedSoundBytesPerFrame = 
								(int)((real32)(SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / 
									GameUpdateHz);
							real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FlipTillAudioSeconds);
							DWORD ExpectedBytesUntillFlip = (DWORD)(( SecondsLeftUntilFlip / TargetSecondsPerFrame) * (real32)ExpectedSoundBytesPerFrame); 
							// How many bytes we expect to write from the play cursor.
							DWORD ExpectedFrameBoundryByte = PlayCursor + ExpectedBytesUntillFlip;
							
							// Make sure the safe write cursor (with safety bytes) doesn't wrap the buffer.
							DWORD SafeWriteCursor = WriteCursor;
							if (SafeWriteCursor < PlayCursor)
							{
								// Fix the case where the WriteCursor has wrapped 
								SafeWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							Assert(SafeWriteCursor >= PlayCursor);
							SafeWriteCursor += SoundOutput.SafetyBytes;

							// Check if the audiocard is latent
							bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundryByte);

							// Calculate where we think the play cursor will end up at after one frame
							DWORD TargetCursor = 0;
							if (AudioCardIsLowLatency)	// No latency
							{
								TargetCursor = (ExpectedFrameBoundryByte + ExpectedSoundBytesPerFrame);
							}
							else 						// Latency
							{
								TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame +
												SoundOutput.SafetyBytes);
							}
							TargetCursor %= SoundOutput.SecondaryBufferSize;

							DWORD BytesToWrite = 0;
							if (ByteToLock > TargetCursor)
							{
								BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
								BytesToWrite += TargetCursor;
							}
							else
							{
								BytesToWrite = TargetCursor - ByteToLock;
							}

							game_sound_output_buffer SoundBuffer = {};
							SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
							SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
							SoundBuffer.Samples = Samples;
							
							if (Game.GetSoundSamples)
							{
								Game.GetSoundSamples(&Thread, &GameMemory, &SoundBuffer);
							}

							Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
#if INTERNAL
							win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex]; 
							Marker->OutputPlayCursor = PlayCursor;
							Marker->OutputWriteCursor = WriteCursor;
							Marker->OutputLocation = ByteToLock;
							Marker->OutputByteCount = BytesToWrite;
							Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundryByte;

							DWORD UnwrappedWriteCursor = WriteCursor;
							if (UnwrappedWriteCursor < PlayCursor) 	// Wirte cursor has wrapped around the circular buffer
							{
								UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							AuidoLatencyBytes = UnwrappedWriteCursor - PlayCursor;		// Audio latency in bytes
							AudioLatencySeconds = (((real32)AuidoLatencyBytes / 
													(real32)SoundOutput.BytesPerSample) / 
													(real32)SoundOutput.SamplesPerSecond);	// Autio latency in seconds
#if 0
							char AudioDebugBuffer[265];
							snprintf(AudioDebugBuffer, sizeof(AudioDebugBuffer), 
								"Delta:%u (%fs)\n", AuidoLatencyBytes, AudioLatencySeconds);
							OutputDebugStringA(AudioDebugBuffer);
#endif
#endif
						}
						else 
						{
							SoundIsValid = false;
						}

						LARGE_INTEGER WorkCounter = Win32GetWallClock();
						real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

						real32 SecondsElapsedForFrame = WorkSecondsElapsed;
						if (SecondsElapsedForFrame < TargetSecondsPerFrame) 
						{
							if (SleepIsGranular) 
							{
								DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - 
																	SecondsElapsedForFrame));
								if (SleepMS > 0) 
								{
									Sleep(SleepMS);
								}
							}
							
							real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, 
																						Win32GetWallClock());
							// TODO(Peter): Look this stuff up!
							if (TestSecondsElapsedForFrame < TargetSecondsPerFrame)
							{
								// TODO(Peter): Logg here, missed sleep
							}

							while(SecondsElapsedForFrame < TargetSecondsPerFrame) 
							{
								SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, 
																				Win32GetWallClock());
							}
						} 
						else 
						{
							// TODO(Peter): Missed framerate!
							// TODO(Peter): Logging
						}

						LARGE_INTEGER EndCounter = Win32GetWallClock();
						real64 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
						LastCounter = EndCounter;

						win32_window_dimension Dimension = Win32GetWindowDimension(Window);
						HDC DeviceContext = GetDC(Window);
						Win32SwapBackBuffer(DeviceContext, &GlobalBackBuffer,
											Dimension.Width, Dimension.Height);
						ReleaseDC(Window, DeviceContext);
						
						FlipWallClock = Win32GetWallClock();
#if INTERNAL
						// NOTE(Peter): This is debug code
						{
							if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
							{
								Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
								win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
								Marker->FlipPlayCursor = PlayCursor;
								Marker->FlipWriteCursor = WriteCursor;
							}
						}
#endif

						// TODO (peter) : Clear these variables
						game_input *Temp = NewInput;
						NewInput = OldInput;
						OldInput = Temp	;

#if 1
						// Framerate updating
						// TODO(peter): Display delta time
						int64 EndCycleCount = __rdtsc();
						uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
						LastCycleCount = EndCycleCount;

						real64 FPS = 1000.0 / MSPerFrame; //GlobalPerfCountFrequency / CounterElapsed;
						real64 MCPF = ((real64)CyclesElapsed / (1000.0f * 1000.0f));

						char TextBuffer[265];
						snprintf(TextBuffer, sizeof(TextBuffer), "%.02fms/f, %.02ff/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);
						OutputDebugStringA(TextBuffer);
#endif
#if INTERNAL
						++DebugTimeMarkerIndex;					
						if (DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers))
						{
							DebugTimeMarkerIndex = 0;
						}
					}
#endif
				}
			}
			else
			{
				// TODO (peter) : Memory allocation failed
			}
		}
		else
		{
			// TODO(peter) : Log that creating the window for some reason failed.
		}
	} 
	else 
	{
		// TODO(peter): Log an error that the window class has failed regestering.
	}

	return (0);
}