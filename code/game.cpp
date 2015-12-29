/**************************************************************************** 
* File:			game.cpp
* Version:		0.0.1a
* Date:			27-12-2015
* Creator:		Peter Verzijl
* Description:	Game Layer Main File
* Notice:		(c) Copyright 2015 by Peter Verzijl. All Rights Reserved.
***************************************************************************/

#include "game.h"

internal void GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz) 
{
	local_persist real32 tSine;
	int16 ToneVolume = 3000;
	int16 *SampleOut = SoundBuffer->Samples;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;


	for (DWORD SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
	{
		real32 SineValue = sinf(tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;

		tSine += 2.0f * PI32 * 1.0f / (real32)(WavePeriod);
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

internal void GameUpdateAndRender(game_memory *Memory, game_offscreen_buffer *Buffer, 
								game_sound_output_buffer *SoundBuffer,
								game_input *Input)
{
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	if (!Memory->IsInitialized)
	{
		char *Filename = __FILE__;
		debug_file_result File = DEBUGPlatformReadEntireFile(Filename);
		if (File.Content)
		{
			DEBUGPlatformWriteEntireFile("test.out", 
									File.ContentSize, File.Content);
			DEBUGPlatformFreeFileMemory(File.Content);
		}

 		GameState->ToneHz = 256;
		GameState->xOffset = 0;
		GameState->yOffset = 0;

		// TODO (peter) : Maybe let the platform layer decide if the game is initialized successfully.
		Memory->IsInitialized = true;
	}

	// TODO (peter) : Allow sample offsets for more robust platform options
	GameOutputSound(SoundBuffer, GameState->ToneHz);

	game_controller_input *Input0 = &Input->Controllers[0];
	if (Input0->IsAnalog)
	{
		GameState->ToneHz = 256 + (int)(128.0f * Input0->EndX);
		GameState->xOffset += (int)(4.0f * Input0->EndY);
	}
	else
	{

	}

	if (Input0->AButton.EndedDown)
	{
		GameState->yOffset += 1;
	}

	/*
	if (Input0->BButton.EndedDown)
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
	*/
	RenderAwesomeGradient(Buffer, GameState->xOffset, GameState->yOffset);
}