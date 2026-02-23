/*
Raylib example file.
This is an example main file for a simple raylib project.
Use this as a starting point or replace it with your code.

by Jeffery Myers is marked with CC0 1.0. To view a copy of this license, visit https://creativecommons.org/publicdomain/zero/1.0/

*/

#include "raylib.h"

#include "resource_dir.h"	// utility header for SearchAndSetResourceDir

#include "orrery.h"

int main ()
{
	// Tell the window to use vsync and work on high DPI displays
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);

	// Create the window and OpenGL context
	InitWindow(1280, 800, "Hello Raylib");

	// Utility function from resource_dir.h to find the resources folder and set it as the current working directory so we can load from it
	SearchAndSetResourceDir("resources");

	// Load a texture from the resources directory
	Texture wabbit = LoadTexture("wabbit_alpha.png");

	System* system = createSystem(true);

	Orrery* orrery = createOrrery((Vector2){640, 400}, 1.f);
	setSystem(orrery, system);

	bool advanceTime = false;
	float worldTime = 0.f;
	float lastTime = GetTime();
	
	// game loop
	while (!WindowShouldClose())		// run the loop until the user presses ESCAPE or presses the Close button on the window
	{
		// drawing
		BeginDrawing();

		// Setup the back buffer for drawing (clear color and depth buffers)
		ClearBackground(BLACK);

		// draw some text using the default font
		DrawText("Hello Raylib", 200,200,20,WHITE);

		// draw our texture to the screen
		DrawTexture(wabbit, 400, 200, WHITE);

		renderOrrery(orrery);
		
		// end the frame and get ready for the next one  (display frame, poll input, etc...)
		EndDrawing();

		Vector2 mouseWheel = GetMouseWheelMoveV();
		if (mouseWheel.y != 0)
		{
			orrery->scale += mouseWheel.y * 0.1f;
			if (orrery->scale < 0.1f) orrery->scale = 0.1f;
		}

		if (IsKeyPressed(KEY_SPACE))
		{
			advanceTime = !advanceTime;
		}

		float currentTime = GetTime();
		float deltaTime = currentTime - lastTime;
		lastTime = currentTime;
		if (advanceTime)
		{						
			worldTime += deltaTime;
			updateSystem(system, worldTime);
		}
	}

	// cleanup
	// unload our texture so it can be cleaned up
	UnloadTexture(wabbit);

	destroyOrrery(orrery);
	destroySystem(system);

	// destroy the window and cleanup the OpenGL context
	CloseWindow();
	return 0;
}
