/**************************************************************************** 
* File:			game.cpp
* Version:		
* Date:			13-02-2016
* Creator:		Peter Verzijl
* Description:	Game Layer Main File
* Notice:		(c) Copyright 2015 by Peter Verzijl. All Rights Reserved.
***************************************************************************/

#include "game.h"

internal void GameOutputSound(game_sound_output_buffer *SoundBuffer, game_state *GameState) 
{
	int16 ToneVolume = 3000;
	int16 *SampleOut = SoundBuffer->Samples;
	int WavePeriod = SoundBuffer->SamplesPerSecond / GameState->ToneHz;


	for (uint32 SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
	{
		real32 SineValue = sinf(GameState->tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;

		GameState->tSine += 2.0f * PI32 * 1.0f / (real32)(WavePeriod);
		if (GameState->tSine > 2.0f * PI32) 
		{
			GameState->tSine -= 2.0f*PI32;
		}
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

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	if (!Memory->IsInitialized)
	{
		char *Filename = __FILE__;
		debug_file_result File = Memory->DEBUGPlatformReadEntireFile(Filename);
		if (File.Content)
		{
			Memory->DEBUGPlatformWriteEntireFile("test.out", 
									File.ContentSize, File.Content);
			Memory->DEBUGPlatformFreeFileMemory(File.Content);
		}

 		GameState->ToneHz = 256;
		GameState->xOffset = 0;
		GameState->yOffset = 0;
		GameState->tSine = 0.0f;

		// TODO (peter) : Maybe let the platform layer decide if the game is initialized successfully.
		Memory->IsInitialized = true;
	}

	for (int ControllerIndex = 0; 
		ControllerIndex < ArrayCount(Input->Controllers); 
		ControllerIndex++)
	{
		game_controller_input *Controller = GetController(Input, ControllerIndex);
		if (Controller->IsAnalog)
		{
			GameState->ToneHz = 256 + (int)(128.0f * Controller->StickAverageX);
			GameState->xOffset += (int)(4.0f * Controller->StickAverageY);
		}
		else
		{
			if (Controller->StickLeft.EndedDown)
			{
				GameState->xOffset -= 1;
			} 
			if (Controller->StickRight.EndedDown) 
			{
				GameState->xOffset += 1;
			}
		}

		if (Controller->AButton.EndedDown)
		{
			GameState->yOffset += 1;
		}
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

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	// TODO (peter) : Allow sample offsets for more robust platform options
	GameOutputSound(SoundBuffer, GameState);
} 