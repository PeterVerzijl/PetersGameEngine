#if !defined(PLATFORM_H)
/**************************************************************************** 
* File:			platform.h
* Version:		0.0.1a
* Date:			27-12-2015
* Creator:		Peter Verzijl
* Description:	Game Layer Header File
* Notice:		(c) Copyright 2015 by Peter Verzijl. All Rights Reserved.
***************************************************************************/

#ifdef __cplusplus
	extern "C" {
#endif

//
// NOTE(Peter): Compilers
//
#if !defined(COMPILER_MSVC)
	#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
	#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
	// NOTE(Peter): Define the compiler ourselves
	#if _MSC_VER
		#undef COMPILER_MSVC
		#define COMPILER_MSVC 1
	#else
		// TODO(Peter): More compilers
		#undef COMPILER_LLVM
		#define COMPILER_LLVM 1
	#endif
#endif

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

/*
Services that the platform layer provides to the game
*/
#if INTERNAL
struct debug_read_file_result 
{
	uint32 ContentSize;
	void *Content;
};
// NOTE(Peter): These get passed to the game via the game_memory struct.
#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif

#define PLATFORM_H
#endif