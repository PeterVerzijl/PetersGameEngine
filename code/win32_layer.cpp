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

#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>

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

typedef float real32;
typedef double real64;

#include "game.cpp"

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
	int SecondaryBufferSize;
	int LatencySampleCount;
};

// TODO(player) : This should maybe not be global?
global_variable bool Running;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

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

internal debug_file_result DEBUGPlatformReadEntireFile(char *FileName)
{
	debug_file_result Result = {}  ;
	HANDLE FileHandle = CreateFileA(FileName,
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
					DEBUGPlatformFreeFileMemory(Result.Content);
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

internal void DEBUGPlatformFreeFileMemory(void *Memory)
{
	if (Memory)
	{
		VirtualFree(Memory,			// The address of the memory to free
					0,				// Must be 0 if MEM_RELEASE (releases the whole page)
					MEM_RELEASE);	// We want to just free it forever
	}
}

internal bool32 DEBUGPlatformWriteEntireFile(char *FileName, uint32 MemorySize, void *Memory)
{
	bool32 Result = false;;
	HANDLE FileHandle = CreateFileA(FileName,
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
	StretchDIBits(DeviceContext,			// The device context
					0, 0, WindowWidth, WindowHeight,
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
		
		case WM_DESTROY:
		{
			// TODO(peter) : This is not good, maybe restart the game and save if possible.
			Running = false;
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
			Running = false;
		} break;
		
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
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
	NewState->EndedDown = IsDown;
	++NewState->HalfTransitionCount;
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, 
											game_button_state *OldState,
											game_button_state *NewState,
											DWORD ButtonBit)
{
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

int CALLBACK wWinMain(HINSTANCE Instance, 
					HINSTANCE PrevInstance, 
					PWSTR lpCmdLine, 
					int CommandLine)
{
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);	// Fixed at boot
	int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	Win32LoadXInput();

	WNDCLASS WindowClass = {};                          	// Initialize to zero

	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);	// Set window size

	WindowClass.style = CS_HREDRAW |						// Redraw whole window ipv just that part
						CS_VREDRAW;                     	// Bit flags for window style
	WindowClass.lpfnWndProc = Win32MainWindowCallback;      // Pointer to our windows callback
	WindowClass.hInstance = Instance;                   	// The window instance handle for talking to windows about our window.
	WindowClass.hIcon;                                  	// The icon for our game in the window
	WindowClass.lpszClassName = "Peters Game Engine";   	// The window name

	if (RegisterClassA(&WindowClass))						// Register the window class 
	{
		HWND Window = 
			CreateWindowEx(0,								// Extended style bit field
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
			// Graphics test
			int xOffset = 0;
			int yOffset = 0;

			win32_sound_output SoundOutput = {};
			SoundOutput.SamplesPerSecond	= 48000;
			SoundOutput.BytesPerSample		= sizeof(int16) * 2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
			SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
			Win32InitDirectSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			// Get messages from the message queue
			// Primitive game loop
			Running = true;

			int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, 
													MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#if INTERNAL
			LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
			LPVOID BaseAddress = 0;
#endif
			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TransientStorageSize = Gigabytes((uint64)1);
			
			uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
			GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, (size_t)TotalSize,
														MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage +	// Cast this to uint8 so we advance the pointer in bytes! 
											GameMemory.PermanentStorageSize);

			if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage) 
			{
				// Input handling
				game_input Input[2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];
				
				// Check the time
				LARGE_INTEGER LastCounter;
				QueryPerformanceCounter(&LastCounter);

				int64 LastCycleCount = __rdtsc();
				while(Running)
				{
					
					// Check if we got a message
					MSG Message;

					game_controller_input *KeyboardController = &NewInput->Controllers[0];
					game_controller_input ZeroController = {};
					*KeyboardController = ZeroController;

					while (PeekMessage(&Message, 		// Pointer to message
										0, 				// The window handle, 0 get all messages
										0, 				// Message filter inner bounds
										0, 				// .. outer bounds
										PM_REMOVE))
					{
						if (Message.message == WM_QUIT) 
						{
							Running = false;
						}

						switch(Message.message)
						{
							case WM_SYSKEYDOWN:
							case WM_SYSKEYUP:
							case WM_KEYDOWN:
							case WM_KEYUP:
							{
								uint32 VKCode = (uint32)Message.wParam;
								bool WasDown = ((Message.lParam & (1 << 30)) != 0);		// Check if the key came up	
								bool IsDown = ((Message.lParam & (1 << 31)) == 0);		// Check if the key is up now
								if (WasDown != IsDown)									// Remove key repeat messages
								{
									if (VKCode == 'W')
									{
									}
									else if (VKCode == 'A')
									{
									}
									else if (VKCode == 'S')
									{
									}
									else if (VKCode == 'D')
									{
									}
									else if (VKCode == 'Q')
									{
										Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder,\
																		IsDown);
									}
									else if (VKCode == 'E')
									{
										Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder,
																		IsDown);
									}
									else if (VKCode == VK_UP)
									{
										Win32ProcessKeyboardMessage(&KeyboardController->Up,
																		IsDown);
									}
									else if (VKCode == VK_DOWN)
									{
										Win32ProcessKeyboardMessage(&KeyboardController->Down,
																		IsDown);
									}
									else if (VKCode == VK_LEFT)
									{
										Win32ProcessKeyboardMessage(&KeyboardController->Left,
																		IsDown);
									}
									else if (VKCode == VK_RIGHT)
									{
										Win32ProcessKeyboardMessage(&KeyboardController->Right,
																		IsDown);
									}
									else if (VKCode == VK_ESCAPE)
									{
										Running = false;
									}
									else if (VKCode == VK_SPACE)
									{
									}
								}
								// Force exit alt-F4
								bool AltKeyWasDown = ((Message.lParam & (1 << 29)) != 0);
								if (VKCode == VK_F4 && AltKeyWasDown)
								{
									Running = false;
								}
							}break;

							default:
							{
								TranslateMessage(&Message);	// Takes message and gets ready for keyboard events etc.
								DispatchMessage(&Message);
							}break;
						}
					}

					// TODO(peter) : Poll input more frequently than every frame?
					DWORD MaxControllerCount = XUSER_MAX_COUNT;
					if (MaxControllerCount > ArrayCount(NewInput->Controllers))
					{
						MaxControllerCount = ArrayCount(NewInput->Controllers);
					}
					for (DWORD ControllerIndex = 0; 
						ControllerIndex < MaxControllerCount; 
						ControllerIndex++)
					{
						game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
						game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];

						XINPUT_STATE ControllerState;
						if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
						{
							// Controller is plugged in
							// TODO(peter) : See if the ControllerState.dwPacketNumber changes too fast
							XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

							// TODO (peter) : Deadzone handeling
							// TODO (peter) : Min/Max macros
							// XINPUT_GAMEPAD_LEFT_THUB_DEADZONE
							// XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE
							
							// Buttons
							Win32ProcessXInputDigitalButton(Pad->wButtons, 
															&OldController->Up, 
															&NewController->Up,
															XINPUT_GAMEPAD_DPAD_UP);
							Win32ProcessXInputDigitalButton(Pad->wButtons, 
															&OldController->Down,
															&NewController->Down,
															XINPUT_GAMEPAD_DPAD_DOWN);
							Win32ProcessXInputDigitalButton(Pad->wButtons, 
															&OldController->Left,
															&NewController->Left,
															XINPUT_GAMEPAD_DPAD_LEFT);
							Win32ProcessXInputDigitalButton(Pad->wButtons, 
															&OldController->Right, 
															&NewController->Right,
															XINPUT_GAMEPAD_DPAD_RIGHT);
							
							Win32ProcessXInputDigitalButton(Pad->wButtons, 
															&OldController->Start, 
															&NewController->Start,
															XINPUT_GAMEPAD_START);
							Win32ProcessXInputDigitalButton(Pad->wButtons, 
															&OldController->Back, 
															&NewController->Back,
															XINPUT_GAMEPAD_BACK);

							bool32 LeftThumb = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
							bool32 RightThumb = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);

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

							NewController->IsAnalog = true;

							// Between -32768 and 32767.
							real32 x;
							if (Pad->sThumbLX < 0)
							{
								x = Pad->sThumbLX / 32768.0f;	// Normalize
							}
							else
							{
								x = Pad->sThumbLX / 32767.0f;	// We need to do this, since negative values are one bigger!
							}
							NewController->StartX = OldController->EndX;
							NewController->MinX = NewController->MaxX = NewController->EndX;

							real32 y;
							if (Pad->sThumbLY < 0)
							{
								y = Pad->sThumbLY / 32768.0f;	// Normalize
							}
							else
							{
								y = Pad->sThumbLY / 32767.0f;	// We need to do this, since negative values are one bigger!
							}
							NewController->MinY = NewController->MaxY = NewController->EndY;
							NewController->StartY = OldController->EndY;
						}
						else
						{
							// Controller is not available
						}
					}

					DWORD ByteToLock = 0;
					DWORD TargetCursor;
					DWORD BytesToWrite = 0;
					DWORD PlayCursor;
					DWORD WriteCursor;
					bool32 SoundIsValid = false;
					if (SUCCEEDED(GlobalSecondaryBuffer->
						GetCurrentPosition(&PlayCursor, &WriteCursor))) 
					{
						// TODO (peter) : This is a second late, which is no good for sound effects
						ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % 
										SoundOutput.SecondaryBufferSize;
						TargetCursor = ((PlayCursor + 
							(SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) %
							SoundOutput.SecondaryBufferSize);
						if (ByteToLock > TargetCursor)
						{
							BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
							BytesToWrite += TargetCursor;
						}
						else
						{
							BytesToWrite = TargetCursor - ByteToLock;
						}

						SoundIsValid = true;
					}
	 				
					game_sound_output_buffer SoundBuffer = {};
					SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
					SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
					SoundBuffer.Samples = Samples;

					game_offscreen_buffer Buffer = {};
					Buffer.Memory = GlobalBackBuffer.Memory;
					Buffer.Width = GlobalBackBuffer.Width;
					Buffer.Height = GlobalBackBuffer.Height;
					Buffer.Pitch = GlobalBackBuffer.Pitch;

					GameUpdateAndRender(&GameMemory, &Buffer, &SoundBuffer, NewInput);

					if (SoundIsValid) 
					{
						Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
					}

					HDC DeviceContext = GetDC(Window);
					win32_window_dimension Dimension = Win32GetWindowDimension(Window);
					Win32SwapBackBuffer(DeviceContext, &GlobalBackBuffer,
										Dimension.Width, Dimension.Height);
					ReleaseDC(Window, DeviceContext);

					// Framerate updating
					LARGE_INTEGER EndCounter;
					QueryPerformanceCounter(&EndCounter);

					int64 EndCycleCount = __rdtsc();

					// TODO(peter): Display delta time
					int64 CyclesElapsed = EndCycleCount - LastCycleCount;
					int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
					int32 MSPerFrame = (int32)((1000 * CounterElapsed) / PerfCountFrequency);
					int64 FPS = PerfCountFrequency / CounterElapsed;
					int32 MCPF = (int32)(CyclesElapsed / (1000 * 1000));

#if 0
					char Buffer[265];
					wsprintf(Buffer, "%dms/f, %df/s, %dmc/f\n", MSPerFrame, (int32)FPS, MCPF);
					OutputDebugStringA(Buffer);
#endif

					LastCounter = EndCounter;
					LastCycleCount = EndCycleCount;

					game_input *Temp = NewInput;
					NewInput = OldInput;
					OldInput = Temp	;
					// TODO (peter) : Clear these variables
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