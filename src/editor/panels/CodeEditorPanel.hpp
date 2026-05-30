#pragma once

#ifndef STANDALONE_MODE
#include <imgui.h>
#include <TextEditor.h>
#endif

#include <string>
#include <filesystem>
#include "editor/EditorViewMode.hpp"

class CodeEditorPanel {
    public:
        CodeEditorPanel();
        ~CodeEditorPanel() = default;

        void Draw(EditorViewMode& currentViewMode);

        // Load a python file into the editor
        void OpenFile(const std::filesystem::path& path, EditorViewMode& currentViewMode);

        // Save the current text buffer back to disk
        void SaveFile();

    private:
        TextEditor m_editor;
        std::filesystem::path m_currentFilePath;
};
