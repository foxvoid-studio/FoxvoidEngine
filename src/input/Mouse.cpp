#include "Mouse.hpp"

// Initialize the static member variable
Vector2 Mouse::s_virtualPosition = { 0.0f, 0.0f };

Vector2 Mouse::GetPosition() {
    return s_virtualPosition;
}

void Mouse::SetVirtualPosition(Vector2 position) {
    s_virtualPosition = position;
}
