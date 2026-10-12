#include "state/game.h"

void Game::scanHostileActivities()
{
    // iterate craft that can be performing hostile activities and update the hostile_activities list accordingly
    hostile_activities.clear();

    for (auto &i : ios)
    {
        if (i->isWarship())
        {
            // is it docking somewhere that belongs to a hostile faction?
            if (i->isDocking())
            {
                Facility *docking_loc = i->dockingLocation();
                if (docking_loc && docking_loc->faction_id != i->faction_id)
                {
                    // add to hostile_activities list
                    hostile_activities.push_back(HostileActivity(HostileActivityType::HAT_BOARDING_ORBITAL, i.get(), docking_loc));
                    continue; // skip to next craft after adding hostile activity
                }
            }
            if (i->inTransit())
            {
                // only detectable if remaining transit time is within a certain threshold
                // currently hardcoded to 4.0

                if (i->remainingStateTime() > 4.0f)
                {
                    continue; // skip to next craft if remaining transit time is above threshold
                }

                // determine destination location, and if there is an opposing faction orbital there
                // TODO if this is first detection of hostile approach, trigger alert
                Orbital *orbital = orbitalAt(i->currentDestination().location);
                if (orbital && orbital->faction_id != i->faction_id)
                {
                    hostile_activities.push_back(HostileActivity(HostileActivityType::HAT_APPROACH_ORBITAL, i.get(), nullptr));
                    continue; // skip to next craft after adding hostile activity
                }
            }
        }
    }
}

bool Game::facilityUnderAttack(Facility *facility)
{
    for (auto &activity : hostile_activities)
    {
        if (activity.target == facility)
        {
            return true;
        }
    }
    return false;
}