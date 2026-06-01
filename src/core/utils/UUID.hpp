#pragma once

#include <cstdint>
#include <functional>

class UUID {
    public:
        // Generates a random 64-bit ID
        UUID();
        
        // Creates a UUID from an existing 64-bit ID (used when loading from JSON)
        UUID(uint64_t uuid);
        
        // Copy constructor
        UUID(const UUID&) = default;

        // Operator overload to easily get the numeric value
        operator uint64_t() const { return m_UUID; }
    private:
        uint64_t m_UUID;
};

// We must provide a hash function so our UUID can be used as a key in std::unordered_map
namespace std {
    template<>
    struct hash<UUID> {
        std::size_t operator()(const UUID& uuid) const {
            return hash<uint64_t>()((uint64_t)uuid);
        }
    };
}
