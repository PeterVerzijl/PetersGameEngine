#include <X11/Xlib.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

global_variable bool32 IsRunning;

int main (int argc, char *argv[])
{
    Display                 *display;
    Visual                  *visual;
    int                     depth;
    int                     text_x;
    int                     text_y;
    XSetWindowAttributes    frame_attributes;
    Window                  frame_window;
    XFontStruct             *fontinfo;
    XGCValues               gr_values;
    GC                      graphical_context;
    XEvent                  event;
    char                    hello_string[] = "Hello World";
    int                     hello_string_length = strlen(hello_string);
 
    display = XOpenDisplay(NULL);
    visual = DefaultVisual(display, 0);
    depth  = DefaultDepth(display, 0);
     
    frame_attributes.background_pixel = XWhitePixel(display, 0);
    /* create the application window */
    frame_window = XCreateWindow(display, XRootWindow(display, 0),
                                 0, 0, 400, 400, 5, depth,
                                 InputOutput, visual, CWBackPixel,
                                 &frame_attributes);
    XStoreName(display, frame_window, "Hello World Example");
    XSelectInput(display, frame_window, ExposureMask | StructureNotifyMask);
 
    fontinfo = XLoadQueryFont(display, "10x20");
    gr_values.font = fontinfo->fid;
    gr_values.foreground = XBlackPixel(display, 0);
    graphical_context = XCreateGC(display, frame_window,
                                  GCFont+GCForeground, &gr_values);
    XMapWindow(display, frame_window);
    
    IsRunning = true;
    while ( IsRunning ) {
        XNextEvent(display, (XEvent *)&event);
        switch ( event.type ) {
            case Expose:
            {
                XWindowAttributes window_attributes;
                int font_direction, font_ascent, font_descent;
                XCharStruct text_structure;
                XTextExtents(fontinfo, hello_string, hello_string_length,
                             &font_direction, &font_ascent, &font_descent,
                             &text_structure);
                XGetWindowAttributes(display, frame_window, &window_attributes);
                text_x = (window_attributes.width - text_structure.width)/2;
                text_y = (window_attributes.height -
                          (text_structure.ascent+text_structure.descent))/2;
                XDrawString(display, frame_window, graphical_context,
                            text_x, text_y, hello_string, hello_string_length);
                break;
            }
            case ClientMessage:
            {
                IsRunning = false;
            }
            default:
                break;
        }
    }

    XDestroyWindow(display, frame_window);
    XCloseDisplay(display);
    printf("%s\n", "Closing...");
    return(0);
}