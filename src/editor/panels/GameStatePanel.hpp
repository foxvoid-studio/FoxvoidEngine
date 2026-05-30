#pragma once

class GameStatePanel {
    public:
        // Call this from the MainMenuBar to request the popup to open
        void Open() { m_openRequested = true; }

        // Renders the ImGui PopupModal
        void Draw();

    private:
        bool m_isOpen = false;
        bool m_openRequested = false;
};
