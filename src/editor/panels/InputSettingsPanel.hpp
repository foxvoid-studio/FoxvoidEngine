#pragma once

#include <string>

class InputSettingsPanel {
    public:
        void Open() { m_openRequested = true; }

        // Draw the ImGui window
        void Draw();

    private:
        bool m_isOpen = false;
        bool m_openRequested = false;
        
        // State for the "Listen to key press" feature
        bool m_isListening = false;
        std::string m_listeningAction = "";
        char m_newActionBuffer[64] = "";

        // Tracks the state to trigger the ImGui::OpenPopup correctly
        bool m_wasOpen = false;
};
