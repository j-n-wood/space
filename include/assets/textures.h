#pragma once

#include "wrappers/texture.h"

// Texture manager class, can load textures by identifiers, is owner of resources.

// Start with fixed array of preloaded textures.

typedef enum
{
    TEXTURE_UI,
    TEXTURE_UI_BUTTONS,
    TEXTURE_ITEMS,
    TEXTURE_BODIES,
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
typedef enum
{
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
    {584, 208, 272, 168}};

typedef enum
{
    II_DOC_DERRICK,
    II_DOC_SHUTTLE_CHASSIS,
    II_DOC_SHUTTLE_DRIVE,
    II_DOC_OFRAME,
    II_DOC_CRYO_POD,
    II_DOC_SUPPLY_POD,
    II_DOC_TOOL_POD,
    II_DOC_IOS_CHASSIS,
    II_DOC_RFRAME,
    II_DOC_AMA,
    II_DOC_GRAPPLE,
    II_DOC_BANDAID,
    II_DOC_AOC,
    II_DOC_ACC,
    II_DOC_I_DRIVE,
    II_DOC_IOS_DRONE,
    II_DOC_COMMSPOD,
    II_DOC_DFCC,
    II_PROD_CRATE,
    II_PROD_DERRICK,
    II_PROD_DERRICK2,
    II_PROD_DERRICK3,
    II_PROD_OFRAME,
    II_PROD_OFRAME2,
    II_PROD_OFRAME3,
    II_PROD_SDRIVE,
    II_PROD_SDRIVE2,
    II_PROD_SDRIVE3,
    II_PROD_SUPPLY,
    II_PROD_SUPPLY2,
    II_PROD_SUPPLY3,
    II_PROD_CHASSIS,
    II_PROD_CHASSIS2,
    II_PROD_CHASSIS3,
    II_PROD_TOOL,
    II_PROD_TOOL2,
    II_PROD_TOOL3,
    II_PROD_CRYO,
    II_PROD_CRYO2,
    II_PROD_CRYO3,
    II_PROD_ACC,
    II_PROD_ACC2,
    II_PROD_ACC3,
    II_PROD_GRAPPLE,
    II_PROD_GRAPPLE2,
    II_PROD_GRAPPLE3,
    II_PROD_BANDAID,
    II_PROD_BANDAID2,
    II_PROD_BANDAID3,
    II_PROD_AOC,
    II_PROD_AOC2,
    II_PROD_AOC3,
    II_PROD_AMA,
    II_PROD_AMA2,
    II_PROD_AMA3,
    II_PROD_IOSDRONE,
    II_PROD_IOSDRONE2,
    II_PROD_IOSDRONE3,
    II_PROD_COMMSPOD,
    II_PROD_COMMSPOD2,
    II_PROD_COMMSPOD3,
    II_PROD_DFCC,
    II_PROD_DFCC2,
    II_PROD_DFCC3,
    II_COUNT
} ItemImageID;

const Rectangle itemImageSources[II_COUNT] = {
    {872, 72, 48, 48},
    {872, 128, 48, 48},
    {816, 128, 48, 48},
    {928, 72, 48, 48},
    {984, 72, 48, 48},
    {928, 128, 48, 48},
    {984, 128, 48, 48},
    {816, 184, 48, 48},
    {872, 184, 48, 48},
    {928, 184, 48, 48},
    {984, 184, 48, 48},
    {816, 240, 48, 48},
    {872, 240, 48, 48},
    {928, 240, 48, 48},
    {984, 240, 48, 48},
    {816, 296, 48, 48},
    {872, 296, 48, 48},
    {928, 296, 48, 48},
    {344, 72, 64, 56},
    {344, 136, 64, 56},
    {416, 136, 64, 56},
    {488, 136, 64, 56},
    {560, 136, 64, 56},
    {632, 136, 64, 56},
    {704, 136, 64, 56},
    {344, 200, 64, 56},
    {416, 200, 64, 56},
    {488, 200, 64, 56},
    {560, 200, 64, 56},
    {632, 200, 64, 56},
    {704, 200, 64, 56},
    {344, 264, 64, 56},
    {416, 264, 64, 56},
    {488, 264, 64, 56},
    {560, 264, 64, 56},
    {632, 264, 64, 56},
    {704, 264, 64, 56},
    {344, 328, 64, 56},
    {416, 328, 64, 56},
    {488, 328, 64, 56},
    {560, 328, 64, 56},
    {632, 328, 64, 56},
    {704, 328, 64, 56},
    {344, 392, 64, 56},
    {416, 392, 64, 56},
    {488, 392, 64, 56},
    {560, 392, 64, 56},
    {632, 392, 64, 56},
    {704, 392, 64, 56},
    {344, 456, 64, 56},
    {416, 456, 64, 56},
    {488, 456, 64, 56},
    {560, 456, 64, 56},
    {632, 456, 64, 56},
    {704, 456, 64, 56},
    {344, 520, 64, 56},
    {416, 520, 64, 56},
    {488, 520, 64, 56},
    {560, 520, 64, 56},
    {632, 520, 64, 56},
    {704, 520, 64, 56},
    {344, 584, 64, 56},
    {416, 584, 64, 56},
    {488, 584, 64, 56},
};

class TextureManager
{
    // owning pointers to texture assets, initialized in constructor and cleaned up in destructor
    // No smart pointers at this point
    TextureAsset *textures[TEXTURE_COUNT];

public:
    TextureManager();
    ~TextureManager();

    // Get a texture by ID, returns nullptr if the ID is invalid
    const TextureAsset *getTexture(TextureID id) const
    {
        if (id < 0 || id >= TEXTURE_COUNT)
        {
            return nullptr;
        }
        return textures[id];
    }

    // singleton pattern for access
    static TextureManager &getInstance()
    {
        static TextureManager instance;
        return instance;
    }

    void dispose();
};