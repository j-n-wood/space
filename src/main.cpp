/*
Raylib example file.
This is an example main file for a simple raylib project.
Use this as a starting point or replace it with your code.

by Jeffery Myers is marked with CC0 1.0. To view a copy of this license, visit https://creativecommons.org/publicdomain/zero/1.0/

*/
#include <cstdlib>

#define RAYGUI_IMPLEMENTATION // define once, must come before include

extern "C"
{
#include "raylib.h"
#include "resource_dir.h" // utility header for SearchAndSetResourceDir
#include "raygui/raygui.h"
}

#include "loaders/loader.h"
#include "assets/textures.h"
#include "pages/pages.h"

#include "wrappers/texture.h"
#include "pages/pages.h"
#include "pages/overlay.h"
#include "state/game.h"
#include "state/autopilot.h"
#include "state/resourceFacility.h"
#include "loaders/save_game.h"

void takeDefaultFocus()
{
	auto &pm{PageManager::getInstance()};
	Game *game = Game::getCurrent();
	System *system = game->allSystems()[0].get();
	Location *earth = system->primary->children[2];
	pm.viewState.setCurrentSystem(system);
	pm.viewState.setCurrentLocation(earth);
	pm.viewState.setCurrentFacility(game->orbitalAt(earth));
}

void buildTestData(Game *game)
{
	System *system = game->allSystems()[0].get();
	Location *earth = system->primary->children[2];
	EarthCity *ec{game->createEarthCity(earth)};
	game->createFactory(ec); // EC production
	ec->num_derricks = 1;
	Orbital *of{game->createOrbital(earth)};

	// test resources
	for (int idx = ResourceType::Iron; idx <= ResourceType::Silica; ++idx)
	{
		of->stores.resources[idx] = 128;
	}

	// test research
	for (auto &item : game->items)
	{
		item.researched = true;
	}

	Shuttle *sh = game->createShuttle(of); // create shuttle at earth orbital
	sh->drive = true;
	sh->fuel = 250;
	sh->setPodType(0, PT_SUPPLY);
	sh->autopilot->flow[ResourceType::Iron] = RF_LOAD_AT_SOURCE;
	sh->autopilot->flow[ResourceType::Titanium] = RF_LOAD_AT_SOURCE;
	sh->autopilot->flow[ResourceType::Copper] = RF_LOAD_AT_SOURCE;
	sh->engageAutopilot();

	// test pod loading
	of->stores.items[0] = 3; // lets put derricks into orbit :)
	of->stores.items[I_Chassis] = 1;
	of->stores.items[I_Drive] = 2;

	// test IOS
	Location *mars = system->primary->children[3];
	IOS *ios = game->createIOS(of);
	ios->drive = true;
	ios->fuel = 250;
	ios->setPodType(0, PT_SUPPLY);
	ios->setDestination(0, mars); // where to go next
	ios->engageAutopilot();

	// mars orbital
	Orbital *mars_orbital{game->createOrbital(mars)};
}

int main()
{
	int uiWidth = 1280;
	int uiHeight = 1024;

	// Tell the window to use vsync and work on high DPI displays
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);

	// Create the window and OpenGL context
	InitWindow(uiWidth, uiHeight, "Space");

	// Utility function from resource_dir.h to find the resources folder and set it as the current working directory so we can load from it
	SearchAndSetResourceDir("resources");

	Game *game = Game::createCurrent();

	BasePage::setWindowSize(uiWidth, uiHeight);

	{
		{
			Loader loader("initial.db");
			if (!loader.isValid())
			{
				TraceLog(LOG_ERROR, "Failed to create loader");
				return 1;
			}

			if (!game->initialise(&loader))
			{
				TraceLog(LOG_ERROR, "Failed to initialise game data");
				return 2;
			}
			buildTestData(game);
		}

		// set game UI state to focus on default selection
		takeDefaultFocus();

		bool advanceTime = false;
		float lastTime = GetTime();

		PageManager &pageManager = PageManager::getInstance();
		pageManager.switchToPage(PAGE_SYSTEM_VIEW);

		Overlay &overlay = Overlay::getInstance(); // create the overlay instance, which will render on top of all pages

		// game loop
		while (!WindowShouldClose()) // run the loop until the user presses ESCAPE or presses the Close button on the window
		{
			overlay.start(); // start the overlay for this frame, can be used to reset any state tracked by the overlay at the start of each frame
			// drawing
			BeginDrawing();

			// Setup the back buffer for drawing (clear color and depth buffers)
			ClearBackground(BLACK);

			pageManager.getCurrentPage()->render();

			// end the frame and get ready for the next one  (display frame, poll input, etc...)
			overlay.render(); // render the overlay after all pages

			EndDrawing();

			pageManager.getCurrentPage()->input();

			if (IsKeyPressed(KEY_SPACE))
			{
				advanceTime = !advanceTime;
			}

			// hotkeys to switch pages
			if (IsKeyPressed(KEY_F1))
			{
				pageManager.switchToPage(PAGE_SYSTEM_VIEW);
			}
			else if (IsKeyPressed(KEY_F2))
			{
				System *system = game->allSystems()[0].get();
				Location *earth = system->primary->children[2];
				pageManager.viewState.setCurrentSystem(system);
				pageManager.viewState.setCurrentLocation(earth);
				pageManager.viewState.setCurrentFacility(game->resourceFacilityAt(earth));
				pageManager.switchToPage(PAGE_EARTH_CITY);
			}
			else if (IsKeyPressed(KEY_F3))
			{
				// switch to first IOS
				auto &ioss = game->allIOS();
				if (!ioss.empty())
				{
					pageManager.viewState.setCurrentCraft(ioss[0].get());
					pageManager.switchToPage(PAGE_COCKPIT);
					pageManager.getCurrentPage()->activate(pageManager.viewState); // force update of page state to reflect new craft focus
				}
			}

			// test save/load
			if (IsKeyPressed(KEY_F5))
			{
				// save to quicksave.db
				SaveGame savegame;
				if (savegame.save("./quicksave.db") != 0)
				{
					TraceLog(LOG_ERROR, "Quicksave failed");
				}
			}

			if (IsKeyPressed(KEY_F8))
			{
				// load from quicksave.db into a new gameState
				std::unique_ptr<Game> tempGame = std::make_unique<Game>();
				Loader quickload("./quicksave.db");
				if (tempGame->initialise(&quickload))
				{
					// reset current values
					game = Game::setCurrent(tempGame);
					takeDefaultFocus();

					// force UI pages to reset
					auto &pm{PageManager::getInstance()};
					pm.getCurrentPage()->activate(pm.viewState); // TODO ugly
				}
				else
				{
					// bork!
					TraceLog(LOG_ERROR, "Quickload failed");
				}
			}

			float currentTime = GetTime();
			float deltaTime = currentTime - lastTime;
			lastTime = currentTime;
			if (advanceTime)
			{
				game->update(deltaTime);
			}
		}

		TextureManager::getInstance().dispose(); // explicitly dispose of textures before exiting, to ensure proper cleanup, though the destructor should also handle this when the program exits and static objects are destroyed
	} // resource scope

	// destroy the window and cleanup the OpenGL context
	CloseWindow();
	return 0;
}
