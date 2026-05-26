#include "ComponentRegistration.hpp"
#include "ComponentRegistry.hpp"

#include "graphics/ShapeRenderer.hpp"
#include "graphics/SpriteRenderer.hpp"
#include "graphics/SpriteSheetRenderer.hpp"
#include "graphics/Animation2d.hpp"
#include "graphics/Animator2d.hpp"
#include "graphics/Camera2d.hpp"
#include "graphics/TileMap.hpp"
#include "graphics/ParticleSystem2d.hpp"
#include "graphics/Camera3d.hpp"
#include "graphics/CuboidMesh.hpp"

#include "scripting/ScriptComponent.hpp"

#include "physics/RectCollider.hpp"
#include "physics/RigidBody2d.hpp"

#include "world/PersistentComponent.hpp"

#include "audio/AudioSource.hpp"

#include "physics/Transform2d.hpp"
#include "physics/PolygonCollider.hpp"
#include "physics/CircleCollider.hpp"
#include "physics/CapsuleCollider.hpp"
#include "physics/Transform3d.hpp"

#include "gui/RectTransform.hpp"
#include "gui/ImageRenderer.hpp"
#include "gui/VBoxContainer.hpp"
#include "gui/HBoxContainer.hpp"
#include "gui/TextRenderer.hpp"
#include "gui/Button.hpp"
#include "gui/Mask.hpp"
#include "gui/Checkbox.hpp"
#include "gui/Slider.hpp"
#include "gui/TextInput.hpp"

namespace EngineSetup {
    void RegisterNativeComponents() {
        // Register all native components to the C++ Engine.

        // Core
        ComponentRegistry::RegisterCPP<Transform2d>("Transform2d", "Core");
        ComponentRegistry::RegisterCPP<PersistentComponent>("PersistentComponent", "Core");
        ComponentRegistry::RegisterCPP<Transform3d>("Transform3d", "Core");

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
        ComponentRegistry::RegisterCPP<Camera3d>("Camera3d", "Graphics");
        ComponentRegistry::RegisterCPP<CuboidMesh>("CuboidMesh", "Graphics");

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
