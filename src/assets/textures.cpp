#include "assets/textures.h"

// registered standard textures, loaded at startup and owned by the texture manager, can be accessed by pages and other systems as needed
// loaded from resources dir.

const char *textureFileNames[TEXTURE_COUNT] = {
    "ui.png",
    "ui_buttons.png",
    "Items.png",
    "bodies.png"};

const bool textureSmoothing[TEXTURE_COUNT] = {
    false, false, false, true};

TextureManager::TextureManager()
{
    // Preload textures here, for example:
    for (int i = 0; i < TEXTURE_COUNT; ++i)
    {
        textures[i] = new TextureAsset(textureFileNames[i]);
        if (textureSmoothing[i])
        {

            SetTextureFilter(Texture2D(*textures[i]), TEXTURE_FILTER_BILINEAR);
        }
    }
}

TextureManager::~TextureManager()
{
    // Clean up textures
    dispose();
}

void TextureManager::dispose()
{
    // Explicitly clean up textures if needed, otherwise rely on destructor
    for (int i = 0; i < TEXTURE_COUNT; ++i)
    {
        delete textures[i];
        textures[i] = nullptr;
    }
}