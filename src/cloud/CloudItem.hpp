#pragma once

#include <string>
#include <pybind11/pybind11.h>

namespace py = pybind11;

class CloudItem {
    public:
        int itemId = 0;
        std::string name = "";
        int quantity = 0;
        bool isActive = false;
        py::dict customData;

        virtual ~CloudItem() = default;

        // Called automatically by the engine after fetching from Django
        virtual void Deserialize(py::dict data) {
            if (data.contains("item_id")) itemId = data["item_id"].cast<int>();
            if (data.contains("name")) name = data["name"].cast<std::string>();
            if (data.contains("quantity")) quantity = data["quantity"].cast<int>();
            if (data.contains("is_active")) isActive = data["is_active"].cast<bool>();
            if (data.contains("custom_data")) customData = data["custom_data"].cast<py::dict>();
            
            // Trigger the hook for Python child classes
            OnDeserialized();
        }

        // Virtual method to be overriden in Python
        virtual void OnDeserialized();
};

class PyCloudItem : public CloudItem {
    public:
        using CloudItem::CloudItem;

        void OnDeserialized() override {
            PYBIND11_OVERRIDE(
                void,
                CloudItem,
                OnDeserialized,
            );
        }
};
