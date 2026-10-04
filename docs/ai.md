# AI model

The opposing faction (id 1, methanoids) in the source material operates like so.

* They are initially not hostile
* They have predefined orbitals and resource facilities at some bodies in all systems
* Docking at a non-hostile other-faction orbital is allowed
* If the docked craft does NOT have a commspod in a tool pod, a gibberish message will be presented. If commspod has not been researched, but a grapple is equipped, a story item will be loaded into the grapple, which when taken to a player faction orbital and removed, unlocks commspod research.
* Docking with a commspod allows trade of any cargo in supply pods.
* Trade events are counted. When a certain number of trades are completed, a message will be shown of a sympathiser warning the player that the methanoids are preparing for way. The commspod tool will be replaced with a grapple holding a story object that unlocks the 'fuzlaser' research topic. The faction then goes hostile and the docked ship is undocked. It will be under attack immediately, as a consequence of being at a location with a hostile orbital with nonzero drones (or a warship present with nonzero drones).
* Ships in dock cannot be directly attacked.
* Trade is a swap of any supply pod content for a different resource type. A mapping table for the faction will be populated to define the resource types swapped.
* Unarmed player ships at a hostile orbital with drones (or any location with a hostile craft with drones) are under attack.
* Unarmed ships, once under attack, have a timer in _real time_ for player response. The craft must change location (engage drive) to escape. There is a threshold for realtime to escape with no damage. Above this, there is another threshold to escape with drive damage. Above that, the final threshold will result in craft destroyed if this time is passed.
* A damaged drive will have 1/2 efficiency for transit time calculation.
* The game will have a pause mode that stops this real-time passage, used for system menus like save/load game. No ingame action will pause the real-time passage as used for animation and such timers as this.
* Saving while a real-time limit timer is in action will immediately proceed as if the maximum threshold has been passed (craft destroyed).
* If 6 orbitals are completed and operational, faction 1 will go hostile. This activates the 'faction hostile' story event which in turn unlocks research topics.
* Setting faction 1 hostile can initally mark faction 0 as hostile as well.
* While hostile, ticks will be run at some interval for enemy faction activity.
* Enemy action will build IOS drones at orbitals. This could be using the same rules as the player (resources must be available), or simply that they are produced at some rate based on game progression and difficulty, and the number of orbitals controlled in that system. The latter option is easier to tweak, so use that.
* Once sufficient total drones are available to the faction in a system, a warship will be commissioned, drones loaded, and a player orbital set as a target.
* Warships will travel (normal craft updates) from their commissioning orbital to the target to attack.
* Players get a short while (4s of game tume) to reinforce, then the attack is resolved. If the player has no warships at the orbital, it is undefended and no combat occurs.
* Players can initiate combat where they have a warship and so does the enemy, or a hostile orbital is present (and is defended by having drone count > 0).
* The player must engage combat. Combat is not engaged automatically.
* The enemy will not initially retreat from combat.
* When the enemy captures an undefended orbital (or player does not engage in combat), the player loses it.
* On capture by an enemy, resource stocks are reduced to random values between 100-1000 and cannot increase. Item stocks are reduced to 0.
* Enemy drones at an orbital do count for defence in craft combat. Currently player drones do not, they must have a DFCC equipped craft to initiate combat.
* Selection of target orbitals by the enemy shall be random between some number (e.g. 4) of player orbitals, those of greater distance from the star first.
* If a system is cleared of enemy orbitals, there is nothing to contribute to local warship fleet construction.
* A warship in a system with no player orbitals can travel interstellar and take effect in the target system. A random system with player presence is selected in this case. The warship is commissioned as an SCG not an IOS in this case. No resources are considered for AI enemies to commission warships.
* Player capture of an enemy orbital transfers that orbital and any resource facility at the same location to the player.
* Player docking at an enemy orbital equipped with a self-destruct (SDM) will trigger the SDM. A new page will be created to show the SDM controls.
* On the first view of the SDM control page, the player cannot disengage it. This triggers a game event unlocking the research topic for SDM.
* Once SDM is researched, the controls in the SDM page are available, and players can stop self-destruct.
* An active SDM has 10s of real time (same rules as per unarmed craft under attack) to be disabled, or the orbital is destroyed, including any docked craft.
* Advancing game time automatically expires any realtime timers (craft under attack, SDM). This means that save can expire such timers by performing a game time advance of say 0.01s.

## Implementation

Ideally game logic remains in Game.

UI interaction can be driven by events (which can be game Events).

We have mechanics like:

* realtime game updates (SDM, craft under attack)
* modal dialogs
* research objects
* game Events
* event bus from Game to subscribers - PageManager is a subscriber.

### Dock at methanoid station with no relevant tools

Desired outcome: on craft page, show comms dialog requesting 'bring a grapple'.

Trigger: docked at methanoid station, no grapple or commspod.

Action: switch to that craft, show dialog, trigger craft launch.

How: Game::onSpacecraftDocked() can detect the condition. Game should not alter UI state. PageManager
'modal' state stops game time advance. Therefore game can trigger an event message that PM picks up
to set state.

What is that event message? Largely a hardcoded 'game event' or 'comms event'. Do we need both?
Note that this event can be repeated. 'Events' are stored in the DB without triggers but with
message text - though not all events are faction related (e.g. reach orbit). They are more about
'what to show'.

Proposal: triggers are just code.
The 'game event' is sent via the event bus. Receivers (other than Game) can check/update game status.

Is this a 'game event' or a 'craft/faction interaction event'? I.e. how do we know what craft to
focus on (show message) from the _trigger_ in the 'game event'? Game events may not have consistent
triggers. Optional parameters for craft/facility reference?

Alternative: make onSpacecraftDocked an eventsink handler? So something meta-game can handle it?
Probably not: want to retain logic in 'Game'. Therefore additional event times for the faction
events. Existing handlers are fairly specific.

e.g. onFactionInteraction(type, craft, facility, event)
PageManager can switch UI to cockpit page, on craft c, and go modal with comms dialog - stopping
game time advance.
Reference to an 'Event' could add some state (e.g. events that only fire once) and is a place to 
reference stored message text outside code.

Game::completeEvent() is already called from inside one of the 'raiseXXX' methods.

This can work with PageManager to e.g. show a message from research on achieving orbital status.