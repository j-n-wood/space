/*
Raylib example file.
This is an example main file for a simple raylib project.
Use this as a starting point or replace it with your code.

by Jeffery Myers is marked with CC0 1.0. To view a copy of this license, visit https://creativecommons.org/publicdomain/zero/1.0/

*/
#include <stdlib.h>

#include "raylib.h"

#include "resource_dir.h"	// utility header for SearchAndSetResourceDir

#include "loaders/loader.h"
#include "loaders/load_system.h"
#include "pages/system_view.h"

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

	Texture ui = LoadTexture("ui.png");

	Loader* loader = createLoader("initial.db");

	if (loader == NULL)
	{
		TraceLog(LOG_ERROR, "Failed to create loader");
		goto cleanup;
	}

	System* system = createSystem(false);
	if (!loadSystem(loader, 1, system))
	{
		TraceLog(LOG_ERROR, "Failed to load system");
		goto cleanup;		
	}	

	Orrery* orrery = createOrrery((Vector2){640, 400}, 1.f);
	setSystem(orrery, system);

	SystemView* systemView = createSystemView(orrery);

	bool advanceTime = false;
	float worldTime = 0.f;
	float lastTime = GetTime();

	Rectangle mainScreenDest = {0, 0, (float)ui.width, (float)ui.height};
	
	// game loop
	while (!WindowShouldClose())		// run the loop until the user presses ESCAPE or presses the Close button on the window
	{
		// drawing
		BeginDrawing();

		// Setup the back buffer for drawing (clear color and depth buffers)
		ClearBackground(BLACK);

		// 272x168+24+32
		Rectangle source = {24, 32, 272, 168};		
		DrawTexturePro(ui, source, mainScreenDest, (Vector2){0, 0}, 0.f, WHITE);

		// renderOrrery(orrery);
		RenderSystemView(systemView);
		
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
	cleanup:
	// unload our texture so it can be cleaned up
	UnloadTexture(wabbit);
	UnloadTexture(ui);

	destroySystemView(systemView);
	destroyOrrery(orrery);
	destroySystem(system);
	destroyLoader(loader);

	// destroy the window and cleanup the OpenGL context
	CloseWindow();
	return 0;
}
