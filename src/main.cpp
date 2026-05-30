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
	System *system = game->allSystems()[1].get();
	Location *earth = game->locationByID(4);
	pm.viewState.setCurrentSystem(system);
	pm.viewState.setCurrentLocation(earth);
	pm.viewState.setCurrentFacility(game->orbitalAt(earth));
}

void buildTestData(Game *game)
{

	System *system = game->allSystems()[1].get();
	Location *earth = game->locationByID(4);
	auto of = game->orbitalAt(earth);

	Shuttle *sh = game->createShuttle(of); // create shuttle at earth orbital
	sh->drive = true;
	sh->fuel = 250;
	sh->setPodType(0, PT_SUPPLY);
	sh->autopilot->flow[ResourceType::Iron] = RF_LOAD_AT_SOURCE;
	sh->autopilot->flow[ResourceType::Titanium] = RF_LOAD_AT_SOURCE;
	sh->autopilot->flow[ResourceType::Aluminium] = RF_LOAD_AT_SOURCE;
	sh->autopilot->flow[ResourceType::Copper] = RF_LOAD_AT_SOURCE;
	sh->autopilot->flow[ResourceType::Carbon] = RF_LOAD_AT_SOURCE;
	sh->engageAutopilot();

	// test pod loading
	of->stores.items[0] = 3; // lets put derricks into orbit :)
	of->stores.items[I_Chassis] = 1;
	of->stores.items[I_Drive] = 2;
	of->stores.items[Of_Frame] = 5;
	of->stores.items[DFCC] = 1;
	of->stores.items[Ios_Drone] = 10;

	// test IOS
	IOS *ios = game->createIOS(of);
	ios->drive = true;
	ios->fuel = 250;
	ios->setPodType(0, PT_SUPPLY);
	Location *mars = game->locationByID(6);
	ios->setDestination(0, mars); // where to go next
	ios->engageAutopilot();

	// test IOS 2 at luna
	Location *luna = earth->children[0];
	IOS *ios2 = game->createIOS(luna);
	ios2->state = CS_ORBIT; // there is no orbital here
	ios2->drive = true;
	ios2->fuel = 250;
	ios2->setPodType(0, PT_TOOL);
	Pod &pod = ios2->pods[0];
	pod.contentType = ItemType::Of_Frame;
	pod.amount = 1;

	ios2->setPodType(1, PT_TOOL);
	ios2->pods[1].contentType = ItemType::Of_Frame;
	ios2->pods[1].amount = 1;

	ios2->setPodType(2, PT_TOOL);
	ios2->pods[2].contentType = ItemType::Of_Frame;
	ios2->pods[2].amount = 1;

	// set dest -> earth orbital
	ios2->setDestination(0, earth);
	ios2->setDestination(1, luna);

	// mars orbital
	Orbital *mars_orbital{game->createOrbital(mars)};
	mars_orbital->operational = true;

	auto ec = game->resourceFacilityAt(earth);
	PageManager::getInstance().viewState.setCurrentResearchFacility(ec->research_facility.get()); // currently global and single

	// set OF frame research complete so we can test orbital construction
	game->researchTopics[6].progress = game->researchTopics[6].requiredTime;
	game->items[ItemType::Of_Frame].researched = true;
}

int main()
{
	int uiWidth = 1280;
	int uiHeight = 1024;

	// Tell the window to use vsync and work on high DPI displays
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);

	// Create the window and OpenGL context
	InitWindow(uiWidth, uiHeight, "Space");
	TraceLog(LOG_INFO, "Window initialized: %d x %d", uiWidth, uiHeight);

	BasePage::setWindowSize(uiWidth, uiHeight);

	// Utility function from resource_dir.h to find the resources folder and set it as the current working directory so we can load from it
	SearchAndSetResourceDir("resources");

	Game *game = Game::createCurrent();

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

			auto currentPage = pageManager.getCurrentPage();

			currentPage->render();

			// input can affect rendering as tooltips can come from buttons
			currentPage->input();

			// end the frame and get ready for the next one  (display frame, poll input, etc...)
			overlay.render(); // render the overlay after all pages

			EndDrawing();

			if (IsKeyPressed(KEY_TAB))
			{
				advanceTime = !advanceTime; // auto-advance mode
			}

			// hotkeys to switch pages
			if (IsKeyPressed(KEY_F1))
			{
				pageManager.switchToPage(PAGE_SYSTEM_VIEW);
			}
			else if (IsKeyPressed(KEY_F2))
			{
				System *system = game->allSystems()[1].get();
				Location *earth = game->locationByID(4);
				pageManager.viewState.setCurrentSystem(system);
				pageManager.viewState.setCurrentLocation(earth);
				pageManager.viewState.setCurrentFacility(game->resourceFacilityAt(earth));
				pageManager.viewState.setCurrentCraft(nullptr);
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
					currentPage->activate(pageManager.viewState); // force update of page state to reflect new craft focus
				}
			}
			else if (IsKeyPressed(KEY_F4))
			{
				// switch to luna IOS
				auto &ioss = game->allIOS();
				if (!ioss.empty())
				{
					pageManager.viewState.setCurrentCraft(ioss[1].get());
					pageManager.switchToPage(PAGE_COCKPIT);
					currentPage->activate(pageManager.viewState); // force update of page state to reflect new craft focus
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
					pm.reactivateCurrentPage();
				}
				else
				{
					// bork!
					TraceLog(LOG_ERROR, "Quickload failed");
				}
			}

			// gui design output
			if (IsKeyPressed(KEY_F11))
			{
				// emit mouse coords
				Vector2 mousePos = GetMousePosition();
				TraceLog(LOG_INFO, "Mouse Position: (%.2f, %.2f)", mousePos.x, mousePos.y);
			}

			// time rate
			if (IsKeyPressed(KEY_EQUAL))
			{
				game->time_rate *= 2.0f;
				TraceLog(LOG_INFO, "Time rate: %.2fx", game->time_rate);
			}
			else if (IsKeyPressed(KEY_MINUS))
			{
				game->time_rate *= 0.5f;
				TraceLog(LOG_INFO, "Time rate: %.2fx", game->time_rate);
			}

			// time

			float currentTime = GetTime();
			float deltaTime = currentTime - lastTime;
			lastTime = currentTime;
			if (advanceTime || IsKeyDown(KEY_SPACE)) // hold space to advance time while paused
			{
				game->update(deltaTime);
			}

			currentPage->update(deltaTime); // not game state, view state
		}

		TextureManager::getInstance().dispose(); // explicitly dispose of textures before exiting, to ensure proper cleanup, though the destructor should also handle this when the program exits and static objects are destroyed
	} // resource scope

	// destroy the window and cleanup the OpenGL context
	CloseWindow();
	return 0;
}
