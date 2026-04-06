/*
Raylib example file.
This is an example main file for a simple raylib project.
Use this as a starting point or replace it with your code.

by Jeffery Myers is marked with CC0 1.0. To view a copy of this license, visit https://creativecommons.org/publicdomain/zero/1.0/

*/
#include <cstdlib>

#define RAYGUI_IMPLEMENTATION // define once, must come before include

extern "C" {
	#include "raylib.h"
	#include "resource_dir.h"	// utility header for SearchAndSetResourceDir
	#include "raygui/raygui.h"
}

#include "loaders/loader.h"
#include "loaders/load_system.h"
#include "pages/system_view.h"
#include "pages/earth_city.h"
#include "assets/textures.h"

#include "wrappers/texture.h"
#include "pages/pages.h"

int main ()
{
	int uiWidth = 1280;
	int uiHeight = 1024;

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

	// BasePage::mainScreenDest = {0, 0, (float)uiWidth, (float)uiHeight};
	BasePage::setWindowSize(uiWidth, uiHeight);

	{
		SystemPtr system = createSystem(false);
		if (!loadSystem(&loader, 1, system.get()))
		{
			TraceLog(LOG_ERROR, "Failed to load system");
			return 2;
		}			

		bool advanceTime = false;
		float worldTime = 0.f;
		float lastTime = GetTime();		
		
		PageManager& pageManager = PageManager::getInstance();
		BasePage* systemView = pageManager.switchToPage(PAGE_SYSTEM_VIEW);
		((SystemView*)systemView)->setSystem(system.get());

		// game loop
		while (!WindowShouldClose())		// run the loop until the user presses ESCAPE or presses the Close button on the window
		{
			// drawing
			BeginDrawing();

			// Setup the back buffer for drawing (clear color and depth buffers)
			ClearBackground(BLACK);

			pageManager.getCurrentPage()->render();		
			
			// end the frame and get ready for the next one  (display frame, poll input, etc...)
			EndDrawing();

			pageManager.getCurrentPage()->input();

			if (IsKeyPressed(KEY_SPACE))
			{
				advanceTime = !advanceTime;
			}

			// hotkeys to switch pages
			if (IsKeyPressed(KEY_ONE))
			{
				BasePage* currentPage = pageManager.switchToPage(PAGE_SYSTEM_VIEW);
				((SystemView*)currentPage)->setSystem(system.get());
			}
			else if (IsKeyPressed(KEY_TWO))
			{
				pageManager.switchToPage(PAGE_EARTH_CITY);
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

		TextureManager::getInstance().dispose(); // explicitly dispose of textures before exiting, to ensure proper cleanup, though the destructor should also handle this when the program exits and static objects are destroyed
	} // resource scope

	// destroy the window and cleanup the OpenGL context
	CloseWindow();
	return 0;
}
