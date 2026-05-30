#pragma once

#include <raylib.h>

class Mouse {
    public:
        // Retrieves the active mouse position 
        // Returns the virtual position in Editor mode, or the OS position in Standalone
        static Vector2 GetPosition();

        // Allows the Editor to inject the calculated virtual mouse position
        static void SetVirtualPosition(Vector2 position);

    private:
        // Internal storage for the virtualized mouse coordinates
        static Vector2 s_virtualPosition;
};
