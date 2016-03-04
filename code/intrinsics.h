#if !defined(GAME_INTRINSICS_H)
/* ========================================================================
	 $File: $
	 $Date: $
	 $Revision: $
	 $Creator: Casey Muratori $
	 $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
	 ======================================================================== */

// TODO(Peter): Convert all of these to platform-efficient versions
// and remove math.h
// NOTE(Peter): This is for performance optimalisation. 
// Since most CPU's have baked in calculations for these kinds of things.
// NOTE(Peter): We wna

#include "math.h"

inline int32 RoundReal32ToInt32(real32 Real32)
{
	int32 Result = (int32)roundf(Real32);
	return(Result);
}

inline uint32 RoundReal32ToUInt32(real32 Real32)
{
	uint32 Result = (uint32)roundf(Real32);
	return(Result);
}

inline int32 FloorReal32ToInt32(real32 Real32)
{
	int32 Result = (int32)floorf(Real32);
	return(Result);
}

inline real32 Sin(real32 Angle)
{
	real32 Result = sinf(Angle);
	return(Result);
}

inline real32 Cos(real32 Angle)
{
	real32 Result = cosf(Angle);
	return(Result);
}

struct bit_scan_result 
{
	bool32 Found;
	uint32 Index;
};

inline bit_scan_result BitScanForward(uint32 value)
{
	bit_scan_result Result = {};
#if COMPILER_MSVC
	Result.Found = _BitScanForward(&Result.Index, value);
#else
	for (uint32 test = 0; test < 32; ++test)
	{
		if (value & (1 << test))
		{
			Result.Index = value;
			Result.Found = true;
			break;
		}
	}
#endif
	return (Result);
}

inline real32 ATan2(real32 Y, real32 X)
{
	real32 Result = atan2f(Y, X);
	return(Result);
}

#define GAME_INTRINSICS_H
#endif
