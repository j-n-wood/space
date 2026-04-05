#pragma once

#include "wrappers/texture.h"

// Texture manager class, can load textures by identifiers, is owner of resources.

// Start with fixed array of preloaded textures.

typedef enum {    
    TEXTURE_UI,
    TEXTURE_UI_BUTTONS,
    TEXTURE_COUNT
} TextureID;

// sprite sheet coordinates for known images
/*
  46: 272x168+24+32 159.5,115.5 45696 srgb(255,255,255)
  47: 272x168+304+32 439.5,115.5 45696 srgb(255,255,255)
  48: 272x168+584+32 719.5,115.5 45696 srgb(255,255,255)
  49: 272x168+864+32 989.0,102.3 19712 srgb(255,255,255) 
  57: 272x168+24+208 159.8,291.7 45550 srgb(255,255,255) 
  58: 272x168+304+208 439.5,291.5 45696 srgb(255,255,255)
  59: 272x168+584+208 719.5,291.5 45696 srgb(255,255,255)
  60: 272x168+864+208 999.5,291.5 45696 srgb(255,255,255)  
*/
typedef enum {
    PB_RESEARCH,
    PB_RESOURCES,
    PB_EARTH_CITY,
    PB_HANGAR,
    PB_COCKPIT,
    PB_FACTORY,
    PB_TRAINING,
    PB_COUNT
} PageBackgroundID;

// use a simple array of Rectangle to store source image coordinates
const Rectangle pageBackgroundSources[PB_COUNT] = {
    {24, 32, 272, 168},
    {304, 32, 272, 168},
    {584, 32, 272, 168},
    {864, 32, 272, 168},
    {24, 208, 272, 168},
    {304, 208, 272, 168},
    {584, 208, 272, 168}
};

class TextureManager {
    // owning pointers to texture assets, initialized in constructor and cleaned up in destructor
    // No smart pointers at this point
    TextureAsset* textures[TEXTURE_COUNT];
public:
    TextureManager();
    ~TextureManager();

    // Get a texture by ID, returns nullptr if the ID is invalid
    const TextureAsset* getTexture(TextureID id) const {
        if (id < 0 || id >= TEXTURE_COUNT) {
            return nullptr;
        }
        return textures[id];
    }

    // singleton pattern for access
    static TextureManager& getInstance() {
        static TextureManager instance;
        return instance;
    }

    void dispose();
};