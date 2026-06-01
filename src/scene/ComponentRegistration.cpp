#include "ComponentRegistration.hpp"
#include "ComponentRegistry.hpp"

#include "math/Transform2d.hpp"

#include "graphics/renderers/ShapeRenderer.hpp"
#include "graphics/renderers/SpriteRenderer.hpp"
#include "graphics/renderers/SpriteSheetRenderer.hpp"
#include "graphics/renderers/TileMap.hpp"

#include "graphics/animation/Animation2d.hpp"
#include "graphics/animation/Animator2d.hpp"

#include "graphics/particles/ParticleSystem2d.hpp"

#include "graphics/Camera2d.hpp"

#include "scripting/ScriptComponent.hpp"

#include "physics/colliders/RectCollider.hpp"
#include "physics/RigidBody2d.hpp"

#include "scene/PersistentComponent.hpp"

#include "audio/AudioSource.hpp"

#include "physics/colliders/PolygonCollider.hpp"
#include "physics/colliders/CircleCollider.hpp"
#include "physics/colliders/CapsuleCollider.hpp"

#include "ui/TextRenderer.hpp"
#include "ui/Button.hpp"
#include "ui/RectTransform.hpp"
#include "ui/ImageRenderer.hpp"
#include "ui/VBoxContainer.hpp"
#include "ui/HBoxContainer.hpp"
#include "ui/Mask.hpp"
#include "ui/Checkbox.hpp"
#include "ui/Slider.hpp"
#include "ui/TextInput.hpp"

namespace EngineSetup {
    void RegisterNativeComponents() {
        // Register all native components to the C++ Engine.

        // Core
        ComponentRegistry::RegisterCPP<Transform2d>("Transform2d", "Core");
        ComponentRegistry::RegisterCPP<PersistentComponent>("PersistentComponent", "Core");

        // Physics
        ComponentRegistry::RegisterCPP<RectCollider>("RectCollider", "Physics");
        ComponentRegistry::RegisterCPP<RigidBody2d>("RigidBody2d", "Physics");
        ComponentRegistry::RegisterCPP<PolygonCollider>("PolygonCollider", "Physics");
        ComponentRegistry::RegisterCPP<CircleCollider>("CircleCollider", "Physics");
        ComponentRegistry::RegisterCPP<CapsuleCollider>("CapsuleCollider", "Physics");

        // Graphics
        ComponentRegistry::RegisterCPP<ShapeRenderer>("ShapeRenderer", "Graphics");
        ComponentRegistry::RegisterCPP<SpriteRenderer>("SpriteRenderer", "Graphics");
        ComponentRegistry::RegisterCPP<SpriteSheetRenderer>("SpriteSheetRenderer", "Graphics");
        ComponentRegistry::RegisterCPP<Animation2d>("Animation2d", "Graphics");
        ComponentRegistry::RegisterCPP<Animator2d>("Animator2d", "Graphics");
        ComponentRegistry::RegisterCPP<Camera2d>("Camera2d", "Graphics");
        ComponentRegistry::RegisterCPP<TileMap>("TileMap", "Graphics");
        ComponentRegistry::RegisterCPP<ParticleSystem2d>("ParticleSystem2d", "Graphics");

        // Gui
        ComponentRegistry::RegisterCPP<RectTransform>("RectTransform", "GUI");
        ComponentRegistry::RegisterCPP<TextRenderer>("TextRenderer", "GUI");
        ComponentRegistry::RegisterCPP<Button>("Button", "GUI");
        ComponentRegistry::RegisterCPP<ImageRenderer>("ImageRenderer", "GUI");
        ComponentRegistry::RegisterCPP<VBoxContainer>("VBoxContainer", "GUI");
        ComponentRegistry::RegisterCPP<HBoxContainer>("HBoxContainer", "GUI");
        ComponentRegistry::RegisterCPP<Mask>("Mask", "GUI");
        ComponentRegistry::RegisterCPP<Checkbox>("Checkbox", "GUI");
        ComponentRegistry::RegisterCPP<Slider>("Slider", "GUI");
        ComponentRegistry::RegisterCPP<TextInput>("TextInput", "GUI");

        // Audio
        ComponentRegistry::RegisterCPP<AudioSource>("AudioSource", "Audio");

        // Scripting
        ComponentRegistry::RegisterCPP<ScriptComponent>("ScriptComponent", "Scripting");
    }
}
