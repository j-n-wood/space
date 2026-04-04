/*
Raylib example file.
This is an example main file for a simple raylib project.
Use this as a starting point or replace it with your code.

by Jeffery Myers is marked with CC0 1.0. To view a copy of this license, visit https://creativecommons.org/publicdomain/zero/1.0/

*/
#include <cstdlib>

extern "C" {
	#include "raylib.h"
	#include "resource_dir.h"	// utility header for SearchAndSetResourceDir
}

#include "loaders/loader.h"
#include "loaders/load_system.h"
#include "pages/system_view.h"
#include "wrappers/texture.h"

int main ()
{
	int uiWidth = 1280;
	int uiHeight = 800;

	// Tell the window to use vsync and work on high DPI displays
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);

	// Create the window and OpenGL context
	InitWindow(uiWidth, uiHeight, "Hello Raylib");

	// Utility function from resource_dir.h to find the resources folder and set it as the current working directory so we can load from it
	SearchAndSetResourceDir("resources");

	Loader loader("initial.db");
	if (!loader.isValid())
	{
		TraceLog(LOG_ERROR, "Failed to create loader");		
		return 1;
	}

	BasePage::mainScreenDest = {0, 0, (float)uiWidth, (float)uiHeight};

	{
		// Load a texture from the resources directory
		TextureAsset wabbit{"wabbit_alpha.png"};
		TextureAsset ui{"ui.png"};		

		SystemPtr system = createSystem(false);
		if (!loadSystem(&loader, 1, system.get()))
		{
			TraceLog(LOG_ERROR, "Failed to load system");
			return 2;
		}	

		OrreryPtr orrery = createOrrery((Vector2){640, 400}, 1.f);
		orrery->setSystem(system.get());

		std::unique_ptr<SystemView> systemView = std::make_unique<SystemView>(orrery.get());

		Rectangle source = {304, 32, 272, 168};
		systemView->setBackground(&ui, source);		

		bool advanceTime = false;
		float worldTime = 0.f;
		float lastTime = GetTime();

		BasePage* currentPage = systemView.get();
		
		// game loop
		while (!WindowShouldClose())		// run the loop until the user presses ESCAPE or presses the Close button on the window
		{
			// drawing
			BeginDrawing();

			// Setup the back buffer for drawing (clear color and depth buffers)
			ClearBackground(BLACK);

			// 272x168+24+32
			// Rectangle source = {24, 32, 272, 168};		
			// Rectangle source = {304, 32, 272, 168};			

			currentPage->render();		
			
			// end the frame and get ready for the next one  (display frame, poll input, etc...)
			EndDrawing();

			currentPage->input();

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
				system->update(worldTime);
			}
		}		
	} // resource scope

	// destroy the window and cleanup the OpenGL context
	CloseWindow();
	return 0;
}
