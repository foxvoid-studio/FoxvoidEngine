#pragma once

#include <string>
#include <pybind11/pybind11.h>

namespace py = pybind11;

// Suppress visibility warnings triggered by pybind11 types (like py::dict) in GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"

class CloudItem {
    public:
        int itemId = 0;
        std::string name = "";
        int quantity = 0;
        bool isActive = false;

        virtual ~CloudItem() = default;

        // Virtual method to be overriden in Python
        virtual void OnDeserialized() {}
};

class PyCloudItem : public CloudItem {
    public:
        using CloudItem::CloudItem;

        void OnDeserialized() override {
            PYBIND11_OVERRIDE_NAME(
                void,
                CloudItem,
                "on_deserialized",
                OnDeserialized
            );
        }
};

#pragma GCC diagnostic pop
