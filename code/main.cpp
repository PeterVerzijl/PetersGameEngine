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

// TODO(player) : This should maybe not be global?
global_variable bool Running; 

global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable int BytesPerPixel = 4;

internal void RenderAwesomeGradient(int xOffset, int yOffset) 
{
	int width = BitmapWidth;
	int height = BitmapHeight;

	int Pitch = width * BytesPerPixel;				// Difference in memory by moving one row down.
	uint8 *Row = (uint8 *)BitmapMemory;
	for (int y = 0; y < BitmapHeight; ++y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int x = 0; x < BitmapWidth; ++x)
		{
			/*	Pixel +... bytes  0  1  2  3
				Little endian	 xx RR GG BB
				-> they swapped the pixel values so you can read xrgb
				Pixel in memory: 0x XX RR GG BB */
			uint8 blue = (x + xOffset);
			uint8 green = (y + yOffset);
			
			*Pixel++ = ((green << 8) | blue);
		}

		Row += Pitch;
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
internal void Win32ResizeDIBSection(int width, int height)
{
	// TODO(peter) : Free memory in a nice way
	// Maybe don't free first, but free after, then free first if that fails

	if (BitmapMemory)
	{
		// Release memeory pages
		VirtualFree(BitmapMemory,					// The start address for the memory
					0,								// Memory size 0 since we want to free everything
					MEM_RELEASE);					// Release ipv decommit
	}

	BitmapWidth = width;
	BitmapHeight = height;

	BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
	BitmapInfo.bmiHeader.biWidth = BitmapWidth;
	BitmapInfo.bmiHeader.biHeight = -BitmapHeight;	// Negative value so we can refer to our pixel rows from top to bottom
	BitmapInfo.bmiHeader.biPlanes = 1;				// 1 color plane, legacy
	BitmapInfo.bmiHeader.biBitCount = 32;			// Bits per color (8 bytes alignment)
	BitmapInfo.bmiHeader.biCompression = BI_RGB;
													// Rest is cleared to 0 due to being static
	
	// NOTE(peter) We don't need to have to get a DC or a BitmapHandle
	// 8 bits red, 8 bits green, 8 bits blue and 8 bits padding (memory aligned accessing) 
	int BitmapMemorySize = (width * height) * BytesPerPixel;

	// Allocate enough pages to store the bitmap.
	BitmapMemory = VirtualAlloc(0,					// Starting address, 0 is ok for us
	  							BitmapMemorySize,
	  							MEM_COMMIT,			// Tell windows we are using the memory and not just reserving
	  							PAGE_READWRITE);	// Access codes, we just want to read and write the memory

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
internal void Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect, int x, int y, int width, int height)
{
	int WindowWidth = ClientRect->right - ClientRect->left;
	int WindowHeight = ClientRect->bottom - ClientRect->top;
	StretchDIBits(DeviceContext,			// The device context
					/*
					x, y, width, height,	// Destination
					x, y, width, height,	// Source
					*/
					0, 0, BitmapWidth, BitmapHeight,
					0, 0, WindowWidth, WindowHeight,
					BitmapMemory,
					&BitmapInfo,
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
			RECT ClientRect;
			GetClientRect(Window, &ClientRect);
			int width = ClientRect.right - ClientRect.left;
			int height = ClientRect.bottom - ClientRect.top;
			Win32ResizeDIBSection(width, height);
			OutputDebugStringA("WM_SIZE\n");
		} break;
		
		case WM_DESTROY:
		{
			// TODO(peter) : This is not good, maybe restart the game and save if possible.
			Running = false;
		} break;
		
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
			
			RECT ClientRect;
			GetClientRect(Window, &ClientRect);

			Win32UpdateWindow(DeviceContext, &ClientRect, x, y, width, height);
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
	WNDCLASS WindowClass = {};                          	// Initialize to zero
	WindowClass.style = CS_OWNDC |                      
						CS_HREDRAW |                    
						CS_VREDRAW;                     	// Bit flags for window style
	WindowClass.lpfnWndProc = Win32MainWindowCallback;      // Pointer to our windows callback
	WindowClass.hInstance = Instance;                   	// The window instance handle for talking to windows about our window.
	WindowClass.hIcon;                                  	// The icon for our game in the window
	WindowClass.lpszClassName = "Peters Game Engine";   	// The window name

	if (RegisterClass(&WindowClass))						// Register the window class 
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
			// Get messages from the message queue
			// Primitive game loop
			Running = true;
			int xOffset = 0;
			int yOffset = 0;
			while(Running)
			{
				// Check if we got a message
				MSG Message;
				while (PeekMessage(&Message, 	// Pointer to message
									0, 			// The window handle, 0 get all messages
									0, 			// Message filter inner bounds
									0, 			// .. outer bounds
									PM_REMOVE))
				{
					if (Message.message == WM_QUIT) 
					{
						Running = false;
					}
					TranslateMessage(&Message);	// Takes message and gets ready for keyboard events etc.
					DispatchMessage(&Message);
				}

				RenderAwesomeGradient(xOffset, yOffset);

				HDC DeviceContext = GetDC(Window);
				RECT ClientRect;
				GetClientRect(Window, &ClientRect);
				int WindowWidth = ClientRect.right - ClientRect.left;
				int WindowHeight = ClientRect.bottom - ClientRect.top;
				Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, WindowWidth, WindowHeight);
				ReleaseDC(Window, DeviceContext);

				++xOffset;
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