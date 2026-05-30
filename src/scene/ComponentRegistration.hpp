#pragma once

namespace EngineSetup {
    // Registers all native C++ components into the Engine's ComponentRegistry.
    // Call this exactly once during engine initialization.
    void RegisterNativeComponents();
}
