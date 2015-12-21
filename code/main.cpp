/**************************************************************************** 
* File:			main.cpp
* Version:		0.0.2a
* Date:			18-12-2015
* Creator:		Peter Verzijl
* Description:	Windows API Layer
* Notice:		(c) Copyright 2015 by Peter Verzijl. All Rights Reserved.
***************************************************************************/ 

#include <windows.h>
#include <stdint.h>
#include <xinput.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

// TODO(player) : This should maybe not be global?
global_variable bool Running;
global_variable win32_offscreen_buffer GlobalBackBuffer;

struct win32_window_dimension
{
	int Width;
	int Height;
};

// XInput function signatures
// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(xinput_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(0);
}
global_variable xinput_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(xinput_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return (0);
}
global_variable xinput_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void Win32LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	if (XInputLibrary)
	{
		XInputGetState = (xinput_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (xinput_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
	}
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
 * @brief Renders a green to blue gradient in the given buffer.
 * @details Renders a green to blue gradient in the given buffer,
 * the gradient takes 255 pixels based on the x and y axes. This coloring
 * is offset by the xOffset and yOffset.
 * @param Buffer The buffer to write the gradient into.
 * @param xOffset The X offset to draw the gradient at.
 * @param yOffset The Y offset to draw the gradient at.
 */
internal void RenderAwesomeGradient(win32_offscreen_buffer *Buffer, 
									int xOffset, int yOffset) 
{
	// TODO(peter) : Pass by value instead of reference?
	
	// int Pitch = width * Buffer->BytesPerPixel;		// Difference in memory by moving one row down.
	uint8 *Row = (uint8 *)Buffer->Memory;
	for (int y = 0; y < Buffer->Height; ++y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int x = 0; x < Buffer->Width; ++x)
		{
			/*	Pixel +... bytes  0  1  2  3
				Little endian	 xx RR GG BB
				-> they swapped the pixel values so you can read xrgb
				Pixel in memory: 0x XX RR GG BB */
			uint8 blue = (x + xOffset);
			uint8 green = (y + yOffset);
			
			*Pixel++ = ((green << 8) | blue);
		}

		Row += Buffer->Pitch;
	}
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
			uint32 VKCode = wParam;
			bool WasDown = ((lParam & (1 << 30)) != 0);		// Check if the key came up	
			bool IsDown = ((lParam & (1 << 31)) == 0);		// Check if the key is up now
			if (WasDown != IsDown)							// Remove key repeat messages
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
				}
				else if (VKCode == 'E')
				{
				}
				else if (VKCode == VK_UP)
				{
				}
				else if (VKCode == VK_DOWN)
				{
				}
				else if (VKCode == VK_LEFT)
				{
				}
				else if (VKCode == VK_RIGHT)
				{
				}
				else if (VKCode == VK_ESCAPE)
				{
				}
				else if (VKCode == VK_SPACE)
				{
				}
			}
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
			int xOffset = 0;
			int yOffset = 0;
			
			// Get messages from the message queue
			// Primitive game loop
			Running = true;
			// Check the time
			LARGE_INTEGER LastCounter;
			QueryPerformanceCounter(&LastCounter);
			
			int64 LastCycleCount = __rdtsc();
			while(Running)
			{
				
				// Check if we got a message
				MSG Message;
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
					TranslateMessage(&Message);	// Takes message and gets ready for keyboard events etc.
					DispatchMessage(&Message);
				}

				// TODO(peter) : Poll input more frequently than every frame?
				for (int ControllerIndex = 0; 
					ControllerIndex < XUSER_MAX_COUNT; 
					ControllerIndex++)
				{
					XINPUT_STATE ControllerState;
					if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
					{
						// Controller is plugged in
						// TODO(peter) : See if the ControllerState.dwPacketNumber changes too fast
						XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
						
						// Buttons
						bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						
						bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
						bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);

						bool LeftThumb = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
						bool RightThumb = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);

						bool LeftBumper = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						bool RightBumper = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

						bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
						bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
						bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
						bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

						int16 StickX = Pad->sThumbLX;
						int16 StickY = Pad->sThumbLY;

						if (AButton)
						{
							xOffset++;
						}

						if (BButton)
						{
							XINPUT_VIBRATION Vibration;
							Vibration.wLeftMotorSpeed = 60000;
							Vibration.wRightMotorSpeed = 60000;
							XInputSetState(0, &Vibration);
						}
						else
						{
							XINPUT_VIBRATION Vibration;
							Vibration.wLeftMotorSpeed = 0;
							Vibration.wRightMotorSpeed = 0;
							XInputSetState(0, &Vibration);
						}
					}
					else
					{
						// Controller is not available
					}
				}

				RenderAwesomeGradient(&GlobalBackBuffer, xOffset, yOffset);

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
				int32 FPS = PerfCountFrequency / CounterElapsed;
				int32 MCPF = (int32)(CyclesElapsed / (1000 * 1000));

				char Buffer[265];
				wsprintf(Buffer, "%dms/f, %df/s, %dmc/f\n", MSPerFrame, FPS, MCPF);
				OutputDebugStringA(Buffer);

				LastCounter = EndCounter;
				LastCycleCount = EndCycleCount;
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