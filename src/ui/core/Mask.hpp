#pragma once

#include "scene/Component.hpp"
#include <nlohmann/json.hpp>
#include <string>

class Mask : public Component {
    public:
        // Allows toggling the mask on and off
        bool isActive;

        Mask();
        ~Mask() override = default;

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;
};
