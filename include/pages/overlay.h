#pragma once

extern "C" {
    #include "raylib.h"
}

class Overlay {
    const char* currentToolTip = nullptr;

    inline void setCurrentToolTip(const char* toolTip) {
        if (toolTip != currentToolTip) {            
            currentToolTip = toolTip;
        }        
    }
public:
    Overlay();

    void start();
    void render();
    int renderButton(const Rectangle& buttonRect, const char* buttonText, const char* toolTip, const Color& color);

    // singleton pattern
    static Overlay& getInstance() {
        static Overlay instance; // Guaranteed to be destroyed, instantiated on first use.
        return instance;
    }    
};