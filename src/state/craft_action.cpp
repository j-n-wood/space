#include "state/craft_action.h"

const char *craftActionNames[CA_COUNT] = {
    "None", "Dock", "Launch", "Descend", "Ascend",
    "Work", "Cancel work", "Engage drive", "Disengage drive"};

const uint16_t craftCapabilities[CT_COUNT] = {
    /* CT_SHUTTLE */ CC_ATMOSPHERIC,
    /* CT_IOS     */ CC_INTERPLANETARY | CC_BOARDING,
    /* CT_SCG     */ CC_INTERPLANETARY | CC_INTERSTELLAR | CC_BOARDING,
};

const char *craftActionCodeText[CAC_COUNT] = {
    "Unknown",
    "OK",
    "Not capable",
    "No drive fitted",
    "No supply pod fitted",
    "Not possible from here",
    "Manoeuvre in progress",
    "No orbital station here",
    "Orbital station incomplete",
    "Defended by drones",
    "No destination set",
    "Route not flyable by this craft",
};