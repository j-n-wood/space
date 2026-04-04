extern "C" {
    #include "raylib.h"
}

#include <utility> // for std::swap

class TextureAsset {
public:
    // 1. Constructor: Loads the texture
    explicit TextureAsset(const char* fileName) {
        texture = LoadTexture(fileName);
    }

    // 2. Destructor: Cleanly releases resources
    ~TextureAsset() {
        Unload();
    }

    // 3. Prevent Copying: We don't want two objects managing the same ID
    TextureAsset(const TextureAsset&) = delete;
    TextureAsset& operator=(const TextureAsset&) = delete;

    // 4. Move Constructor: Transfer ownership
    TextureAsset(TextureAsset&& other) noexcept : texture(other.texture) {
        other.texture = { 0 }; // Reset the old object's ID so it doesn't unload on destruction
    }

    // 5. Move Assignment: Transfer ownership
    TextureAsset& operator=(TextureAsset&& other) noexcept {
        if (this != &other) {
            Unload(); // Clean up current resource
            texture = other.texture;
            other.texture = { 0 };
        }
        return *this;
    }

    // Helper to get the underlying raylib Texture2D
    const Texture2D& get() const { return texture; }

    // Overload the cast operator for seamless use with raylib functions
    operator Texture2D() const { return texture; }

private:
    Texture2D texture;

    void Unload() {
        if (texture.id > 0) {
            UnloadTexture(texture);
            texture.id = 0;
        }
    }
};