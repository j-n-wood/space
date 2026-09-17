#pragma once

#include <cstdint>
#include "state/craft_type.h"

// define actions that craft can perform
// these can be driven from user input, or automated

enum CraftAction : uint8_t
{
    CA_NONE,
    CA_DOCK,
    CA_LAUNCH,  // undock
    CA_DESCEND, // from orbit to surface
    CA_ASCEND,
    CA_WORK,
    CA_CANCEL_WORK, // give up current work
    CA_ENGAGE_DRIVE,
    CA_DISENGAGE_DRIVE,
    CA_COUNT
};

extern const char *craftActionNames[CA_COUNT]; // "Dock", "Launch", ...

enum CraftCapability : uint16_t
{
    CC_NONE = 0,
    CC_ATMOSPHERIC = 1 << 1,
    CC_INTERPLANETARY = 1 << 2,
    CC_INTERSTELLAR = 1 << 3,
    CC_BOARDING = 1 << 4 // can board and capture other craft/facilities
};

extern const uint16_t craftCapabilities[CT_COUNT];

inline bool craftHasCapability(CraftType t, uint16_t bits)
{
    return (craftCapabilities[t] & bits) == bits;
}

// result code of asking for some action, so we can log/give feedback to the player
enum CraftActionCode : uint8_t
{
    CAC_UNKNOWN = 0,
    CAC_OK,            // accepted -- the manoeuvre has STARTED, not finished
    CAC_NOT_CAPABLE,   // capability
    CAC_NO_DRIVE,      // fitment
    CAC_NO_SUPPLY_POD, // fitment (autopilot)
    CAC_WRONG_STATE,   // situation
    CAC_BUSY,
    CAC_NO_ORBITAL,
    CAC_ORBITAL_INCOMPLETE,
    CAC_DEFENDED,
    CAC_NO_DESTINATION,
    CAC_ROUTE_UNREACHABLE,
    CAC_COUNT
};
extern const char *craftActionCodeText[CAC_COUNT]; // "No drive fitted", ...

// wrap code in a class that can have operator bool() to test for success, and a method to get the text for logging/display
class CraftActionResult
{
    CraftActionCode code_{CAC_UNKNOWN};

public:
    constexpr CraftActionResult() = default;
    constexpr CraftActionResult(CraftActionCode c) : code_{c} {} // implicit: `return CAC_NO_DRIVE;`

    constexpr explicit operator bool() const { return code_ == CAC_OK; }
    constexpr CraftActionCode code() const { return code_; }
    const char *text() const { return craftActionCodeText[code_]; }

    friend constexpr bool operator==(CraftActionResult a, CraftActionResult b)
    {
        return a.code_ == b.code_;
    }
};