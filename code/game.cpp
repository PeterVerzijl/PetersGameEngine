/**************************************************************************** 
* File:			game.cpp
* Version:		
* Date:			13-02-2016
* Creator:		Peter Verzijl
* Description:	Game Layer Main File
* Notice:		(c) Copyright 2015 by Peter Verzijl. All Rights Reserved.
***************************************************************************/

#include "game.h"

internal void GameOutputSound(game_sound_output_buffer *SoundBuffer, game_state *GameState, int ToneHz) 
{
	int16 ToneVolume = 3000;
	int16 *SampleOut = SoundBuffer->Samples;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

	for (uint32 SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
	{
#if 0
		real32 SineValue = sinf(GameState->tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
		int16 SampleValue = 0;
#endif
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
#if 0
		GameState->tSine += 2.0f * PI32 * 1.0f / (real32)(WavePeriod);
		if (GameState->tSine > 2.0f * PI32) 
		{
			GameState->tSine -= 2.0f*PI32;
		}
#endif 
	}
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
internal void RenderAwesomeGradient(game_offscreen_buffer *Buffer, 
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
			uint8 blue = (uint8)(x + xOffset);
			uint8 green = (uint8)(y + yOffset);
			
			*Pixel++ = ((green << 8) | blue);
		}
		Row += Buffer->Pitch;
	}
} 

inline int32 TruncateReal32ToInt32(real32 Value)
{
	int32 Result = (int32)Value;
	// TODO(Peter): Intrinsic?
	return(Result);
}

internal void DrawRectangle(game_offscreen_buffer *Buffer,
							real32 realMinX, real32 realMinY, real32 realMaxX, real32 realMaxY, 
							real32 r, real32 g, real32 b) 
{
	int32 minx = RoundReal32ToUInt32(realMinX);
	int32 miny = RoundReal32ToUInt32(realMinY);
	int32 maxx = RoundReal32ToUInt32(realMaxX);
	int32 maxy = RoundReal32ToUInt32(realMaxY);

	// Clipping code
	if (minx < 0)
	{
		minx = 0;
	}
	if (miny < 0)
	{
		miny = 0;
	}
	if (maxx > Buffer->Width)
	{
		maxx = Buffer->Width;
	}
	if (maxy > Buffer->Height)
	{
		maxy = Buffer->Height;
	}

	uint32 color = ((RoundReal32ToUInt32(r * 255.0f) << 16) |
					(RoundReal32ToUInt32(g * 255.0f) << 8) |  
					(RoundReal32ToUInt32(b * 255.0f) << 0));
	uint8 *row = ((uint8 *)Buffer->Memory + 
					minx * Buffer->BytesPerPixel + 
					miny * Buffer->Pitch);

	for (int y = miny; y < maxy; ++y)
	{
		uint32 *Pixel = (uint32 *)row;
		for (int x = minx; x < maxx; ++x)
		{
			*Pixel++ = color;
		}
		row += Buffer->Pitch;
	}
}
#pragma pack(push, 1)
struct bitmap_header
{
	uint16 FileType;
	uint32 FileSize;
	uint16 Reserved1;
	uint16 Reserved2;
	uint32 BitmapOffset;
	uint32 Size;
	int32 Width;
	int32 Height;
	uint16 Planes;
	uint16 BitsPerPixel;	// From beginning of file

	uint32 Compression;
	uint32 SizeOfBitmap;
	int32 HorizontalResolution;
	int32 VerticalResolution;
	uint32 ColorUsed;
	uint32 ColorImportant;

	uint32 RedMask;
	uint32 BlueMask;
	uint32 GreenMask;
	// Alpha mask is ~(RedMask | BlueMask | GreenMask)
};
#pragma pack(pop)

internal loaded_bitmap DEBUGLoadBMP(thread_context *Thread, game_memory *Memory, char *Filename)
{
	loaded_bitmap Result = {};
	
	// NOTE(Peter): Byte order of the image is: AA RR GG BB, bottom up

	debug_read_file_result ReadResult = Memory->DEBUGPlatformReadEntireFile(Thread, Filename);
	if (ReadResult.ContentSize != 0)	// If read succeeded
	{
		bitmap_header *Header = (bitmap_header *)ReadResult.Content;
		Result.Pixels = (uint32 *)((uint8 *)ReadResult.Content + Header->BitmapOffset);	// 4 bytes aligned
		Result.Width = Header->Width;
		Result.Height = Header->Height;
		
		uint32 RedMask = Header->RedMask;
		uint32 GreenMask = Header->GreenMask;
		uint32 BlueMask = Header->BlueMask;
		uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);
		
		bit_scan_result RedShift = BitScanForward(RedMask);
		bit_scan_result GreenShift = BitScanForward(GreenMask);
		bit_scan_result BlueShift = BitScanForward(BlueMask);
		bit_scan_result AlphaShift = BitScanForward(AlphaMask);

		Assert(RedShift.Found && BlueShift.Found && GreenShift.Found && AlphaShift.Found);

		// Read out masks and convert to our pixel format which is AA BB GG RR
		// Little endian -> 0xRRGGBBAA
		uint32 *SourceDest = Result.Pixels;
		for (int32 y = 0; y < Header->Height; ++y)
		{
			for (int32 x = 0; x < Header->Width; ++x)
			{
				uint32 c = *SourceDest;
				*SourceDest++ = ((((c >> AlphaShift.Index) & 0xFF) << 24) |
								(((c >> RedShift.Index) & 0xFF) << 16) |
								(((c >> GreenShift.Index) & 0xFF) << 8) |
								(((c >> BlueShift.Index) & 0xFF) << 0));
			}
		}
	}
	return(Result);
}

internal void DrawBitmap(game_offscreen_buffer *Buffer, 
						real32 realX, real32 realY, loaded_bitmap *Bitmap) 
{
	int32 minx = RoundReal32ToUInt32(realX);
	int32 miny = RoundReal32ToUInt32(realY);
	int32 maxx = RoundReal32ToUInt32(realX + Bitmap->Width);
	int32 maxy = RoundReal32ToUInt32(realY + Bitmap->Height);
	// Clipping code
	if (minx < 0)
	{
		minx = 0;
	}
	if (miny < 0)
	{
		miny = 0;
	}
	if (maxx > Buffer->Width)
	{
		maxx = Buffer->Width;
	}
	if (maxy > Buffer->Height)
	{
		maxy = Buffer->Height;
	}
	// Bitmaps are stored from bottom to top, so we need to swap that around.
	// TODO(Peter): This is wrong at the moment we start clipping...
	uint32 *SourceRow = Bitmap->Pixels + Bitmap->Width * (Bitmap->Height - 1); 
	uint8 *DestRow = ((uint8 *)Buffer->Memory + 
					minx * Buffer->BytesPerPixel + 
					miny * Buffer->Pitch);
	for (int32 y = miny; y < maxy; ++y)
	{
		uint32 *Dest = (uint32 *)DestRow;
		uint32 *Source = SourceRow;
		for (int32 x = minx; x < maxx; ++x)
		{
			*Dest++ = *Source++; 
		}
		DestRow += Buffer->Pitch;
		SourceRow -= Bitmap->Width;	// Move from bottom to top.
	}
}

inline tile_map * GetTileMap(world *World, int32 TileMapX, int32 TileMapY)
{
	tile_map *Result;
	if ((TileMapX >= 0) && (TileMapX < World->TileMapCountX) &&
		(TileMapY >= 0) && (TileMapY < World->TileMapCountY))
	{
		Result = &World->TileMaps[World->TileMapCountX * TileMapY + TileMapX];
	}
	return(Result);
}

inline uint32 GetTileValueUnchecked(tile_map *TileMap, int32 x, int32 y)
{
	// TODO(Peter): Bounds checking?
	uint32 Result = TileMap->Tiles[TileMap->CountX * y + x];
	return(Result);
}

internal bool32 IsTileMapPointEmpty(tile_map *TileMap, real32 TestPointX, real32 TestPointY)
{
	bool32 Result = false;

	int32 TestTileX = TruncateReal32ToInt32((TestPointX - TileMap->UpperLeftX) / TileMap->TileSize);
	int32 TestTileY = TruncateReal32ToInt32((TestPointY - TileMap->UpperLeftY) / TileMap->TileSize);

	// Tile map bounds check
	if ((TestTileX >= 0) && (TestTileX < TileMap->CountX) &&
		(TestTileX >= 0) && (TestTileY < TileMap->CountY))
	{
		uint32 TileMapValue = GetTileValueUnchecked(TileMap, TestTileX, TestTileY);
		Result = (TileMapValue == 0);
	}

	return(Result);
}

internal bool32 IsWorldPointEmpty(world *World, int32 TileMapX, int32 TileMapY, 
									real32 TestPointX, real32 TestPointY)
{
	bool32 Result = false;

	tile_map *TileMap = GetTileMap(World, TileMapX, TileMapY);
	if (TileMap) 
	{
		int32 TestTileX = TruncateReal32ToInt32((TestPointX - TileMap->UpperLeftX) / TileMap->TileSize);
		int32 TestTileY = TruncateReal32ToInt32((TestPointY - TileMap->UpperLeftY) / TileMap->TileSize);

		// Tile map bounds check
		if ((TestTileX >= 0) && (TestTileX < TileMap->CountX) &&
			(TestTileX >= 0) && (TestTileY < TileMap->CountY))
		{
			uint32 TileMapValue = GetTileValueUnchecked(TileMap, TestTileX, TestTileY);
			Result = (TileMapValue == 0);
		}
	}

	return(Result);
}
extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

	game_state *GameState = (game_state *)Memory->PermanentStorage;
	if (!Memory->IsInitialized)
	{
		GameState->TestImage = DEBUGLoadBMP(Thread, Memory, "test_background.bmp");

		GameState->PlayerX = 200;
		GameState->PlayerY = 200;

		Memory->IsInitialized = true;
	}

	// Row column
#define TILE_MAP_COUNT_X 17
#define TILE_MAP_COUNT_Y 9
	uint32 Tiles00[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = 
	{
		1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1,
		1, 0, 1, 0,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 1, 1,  0, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0,
		1, 0, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0,  1,
		1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1,
	};
	uint32 Tiles01[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = 
	{
		1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1,
	};
	uint32 Tiles11[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = 
	{
		1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1,
		1, 0, 1, 0,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 1, 1,  0, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 1, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0,  1,
		1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1,
	};
	uint32 Tiles10[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = 
	{
		1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
		1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1,  1,
	};


	tile_map TileMaps[2][2];
	TileMaps[0][0].CountX = TILE_MAP_COUNT_X;
	TileMaps[0][0].CountY = TILE_MAP_COUNT_Y;
	
	TileMaps[0][0].UpperLeftX = -30;
	TileMaps[0][0].UpperLeftY = 0;
	TileMaps[0][0].TileSize = 60;

	TileMaps[0][0].Tiles = (uint32 *)Tiles00;
	
	TileMaps[0][1] = TileMaps[0][0];
	TileMaps[0][1].Tiles = (uint32 *)Tiles01;
	
	TileMaps[1][1] = TileMaps[0][0];
	TileMaps[1][1].Tiles = (uint32 *)Tiles11;
	
	TileMaps[1][0] = TileMaps[0][0];
	TileMaps[1][0].Tiles = (uint32 *)Tiles10;

	tile_map *TileMap = &TileMaps[0][0];

	world World;
	World.TileMaps = (tile_map *)TileMaps;

	// Player varaibles
	real32 PlayerWidth = TileMap->TileSize * 0.7f;
	real32 PlayerHeight = TileMap->TileSize;
 
	for (int ControllerIndex = 0; 
		ControllerIndex < ArrayCount(Input->Controllers); 
		ControllerIndex++)
	{
		game_controller_input *Controller = GetController(Input, ControllerIndex);
		if (Controller->IsAnalog)
		{
			// NOTE(Peter): Use analog movement
		}
		else
		{
			// NOTE(Peter): Use digital movement
			real32 PlayerSpeed = 60.0f;
			real32 dPlayerX = 0.0f;
			real32 dPlayerY = 0.0f;

			if (Controller->DPadUp.EndedDown)
			{
				dPlayerY -= PlayerSpeed;
			}
			if (Controller->DPadDown.EndedDown)
			{
				dPlayerY += PlayerSpeed;
			}
			if (Controller->DPadLeft.EndedDown)
			{
				dPlayerX -= PlayerSpeed;
			}
			if (Controller->DPadRight.EndedDown)
			{
				dPlayerX += PlayerSpeed;
			}

			// TODO(Peter): Diagonal is now faster... :(
			// Vectors YEAHYS
			real32 NewPlayerX = GameState->PlayerX + dPlayerX * Input->DeltaTime;
			real32 NewPlayerY = GameState->PlayerY + dPlayerY * Input->DeltaTime;

			if (IsTileMapPointEmpty(TileMap, NewPlayerX - 0.5f*PlayerWidth, NewPlayerY) &&
				IsTileMapPointEmpty(TileMap, NewPlayerX + 0.5f*PlayerWidth/2, NewPlayerY)) 
			{
				GameState->PlayerX = NewPlayerX;
				GameState->PlayerY = NewPlayerY;
			}
		}
	}

	// DrawBitmap(Buffer, 0, 0, &GameState->TestImage);
	DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, 
					1.0f, 0.0f, 1.0f);

	for (int row = 0; row < 9; ++row)
	{
		for (int column = 0; column < 17; ++column)
		{
			uint32 TileID = GetTileValueUnchecked(TileMap, column, row);
			real32 GrayValue = 0.5f;
			if (TileID == 1)
			{
				GrayValue = 1.0f;
			}
			real32 minx = TileMap->UpperLeftX + ((real32)column) * TileMap->TileSize;
			real32 miny = TileMap->UpperLeftY + ((real32)row) * TileMap->TileSize;
			real32 maxx = minx + TileMap->TileSize;
			real32 maxy = miny + TileMap->TileSize;
			DrawRectangle(Buffer, minx, miny, maxx, maxy,
						GrayValue, GrayValue, GrayValue);
		}
	}

	// Draw Player
	DrawRectangle(Buffer, 
				GameState->PlayerX - PlayerWidth/2, GameState->PlayerY - PlayerHeight, 
				GameState->PlayerX + PlayerWidth/2, GameState->PlayerY,
				1.0f, 1.0f, 0.0f);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	GameOutputSound(SoundBuffer, GameState, 400);
} 