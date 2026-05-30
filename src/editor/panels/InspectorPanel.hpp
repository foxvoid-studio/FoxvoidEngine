#pragma once

#ifndef STANDALONE_MODE
#include <imgui.h>
#endif

#include "scene/GameObject.hpp"
#include "scene/ComponentRegistry.hpp"

class InspectorPanel {
    public:
        void Draw(GameObject*& selectedObject, py::object& selectedAsset, std::string& selectedAssetPath);
};
