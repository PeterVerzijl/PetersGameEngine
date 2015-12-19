/**************************************************************************** 
* File:					main.cpp
* Version:			0.0.1a
* Date:					18-12-2015
* Creator:			Peter Verzijl
* Description:	Windows API Layer
* Notice:				(c) Copyright 2015 by Peter Verzijl. All Rights Reserved.
***************************************************************************/ 

#include <windows.h>

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
LRESULT CALLBACK MainWindowCallback(HWND Window, 
									UINT Message, 
									WPARAM wParam, 
									LPARAM lParam)
{
	LRESULT Result = 0;

	switch(Message) 
	{
		case WM_SIZE:
		{
			OutputDebugStringA("WM_SIZE\n");
		} break;
		
		case WM_DESTROY:
		{
			OutputDebugStringA("WM_DESTROY\n");
		} break;
		
		case WM_CLOSE:
		{
			OutputDebugStringA("WM_CLOSE\n");
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
			// Debugging
			static DWORD Operation = WHITENESS;
			if (Operation == WHITENESS)
			{
				Operation = BLACKNESS;
			}
			else
			{
				Operation = WHITENESS;
			}
			PatBlt(DeviceContext, x, y, width, height, Operation);
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
	WNDCLASS WindowClass = {};                          // Initialize to zero
	WindowClass.style = CS_OWNDC |                      
						CS_HREDRAW |                    
						CS_VREDRAW;                     // Bit flags for window style
	WindowClass.lpfnWndProc = MainWindowCallback;       // Pointer to our windows callback
	WindowClass.hInstance = Instance;                   // The window instance handle for talking to windows about our window.
	WindowClass.hIcon;                                  // The icon for our game in the window
	WindowClass.lpszClassName = "Peters Game Engine";   // The window name

	if (RegisterClass(&WindowClass))					// Register the window class 
	{
		HWND WindowHandle = 
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
		if (WindowHandle) 
		{
			// Get messages from the message queue
			MSG Message;
			for(;;) 
			{
				BOOL MessageResult = GetMessage(&Message,	// Pointer to the message
												0,			// The window handle, 0 is all messages
												0,			// Message filter range
												0); 
				if (MessageResult > 0) 
				{
					TranslateMessage(&Message);				// Takes message and gets ready for keyboard events etc.
					DispatchMessage(&Message);
				}
				else 
				{
					break;
				}
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