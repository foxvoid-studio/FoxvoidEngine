#pragma once

#include "scene/Component.hpp"

class PersistentComponent : public Component {
    public:
        PersistentComponent() = default;
        ~PersistentComponent() override = default;

        std::string GetName() const override { return "Persistent Component"; }

        // No specific data to serialize, just its presence is enough
        nlohmann::json Serialize() const override {
            return { {"type", "PersistentComponent"} };
        }

        void Deserialize(const nlohmann::json& j) override {}
};
