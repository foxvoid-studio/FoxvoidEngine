#include "scripting/ScriptBindings.hpp"
#include <pybind11/stl.h>
#include <iostream>
#include "world/GameObject.hpp"
#include <world/ComponentRegistry.hpp>

#include "graphics/Graphics.hpp"
#include "graphics/SpriteRenderer.hpp"
#include "graphics/SpriteSheetRenderer.hpp"
#include "graphics/Animation2d.hpp"
#include "graphics/Animator2d.hpp"
#include "graphics/Camera2d.hpp"
#include "graphics/TileMap.hpp"
#include "graphics/ShapeRenderer.hpp"
#include "graphics/ParticleSystem2d.hpp"

#include "graphics/Camera3d.hpp"
#include "graphics/CuboidMesh.hpp"
#include "graphics/light/DirectionalLight.hpp"
#include "graphics/light/PointLight.hpp"
#include "graphics/MeshRenderer.hpp"
#include "graphics/Animator3d.hpp"

void BindGraphics(py::module_& m) {
    m.def("set_pixel_art_mode", [](bool enable) {
        Graphics::pixelArtMode = enable;
    });
    
    m.def("is_pixel_art_mode", []() {
        return Graphics::pixelArtMode;
    });

    py::class_<SpriteRenderer, Component>(m, "SpriteRenderer")
        .def(py::init<std::string>())
        .def_property_readonly("width", &SpriteRenderer::GetWidth)
        .def_property_readonly("height", &SpriteRenderer::GetHeight);

    ComponentRegistry::Register<SpriteRenderer>("SpriteRenderer", 
        [](GameObject& go, py::args args) -> py::object {
            if (args.size() < 1) {
                std::cerr << "[Python] SpriteRenderer needs a texture path!" << std::endl;
                return py::none();
            }
            std::string path = args[0].cast<std::string>();
            auto* s = go.AddComponent<SpriteRenderer>(path);
            return py::cast(s, py::return_value_policy::reference);
        }
    );

    py::class_<SpriteSheetRenderer, Component>(m, "SpriteSheetRenderer")
        .def(py::init<std::string, int, int>())
        // We bind GetFrame and SetFrame to a clean Python property '.frame'
        .def_property("frame", &SpriteSheetRenderer::GetFrame, &SpriteSheetRenderer::SetFrame)
        // Read-only property for the total frame count
        .def_property_readonly("frame_count", &SpriteSheetRenderer::GetFrameCount);

    ComponentRegistry::Register<SpriteSheetRenderer>("SpriteSheetRenderer", 
        [](GameObject& go, py::args args) -> py::object {
            if (args.size() < 3) {
                std::cerr << "[Python] SpriteSheetRenderer requires (texture_path, columns, rows)!" << std::endl;
                return py::none();
            }
            std::string path = args[0].cast<std::string>();
            int cols = args[1].cast<int>();
            int rows = args[2].cast<int>();
            auto* s = go.AddComponent<SpriteSheetRenderer>(path, cols, rows);
            return py::cast(s, py::return_value_policy::reference);
        }
    );

    py::class_<ShapeRenderer, Component>(m, "ShapeRenderer")
        .def(py::init<>()) // Uses default arguments from the C++ constructor
        .def_readwrite("width", &ShapeRenderer::width)
        .def_readwrite("height", &ShapeRenderer::height)
        .def_readwrite("color", &ShapeRenderer::color)
        .def_readwrite("is_hud", &ShapeRenderer::isHUD);

    ComponentRegistry::Register<ShapeRenderer>("ShapeRenderer", 
        [](GameObject& go, py::args args) -> py::object {
            // No arguments required upon creation via Python script
            auto* s = go.AddComponent<ShapeRenderer>();
            return py::cast(s, py::return_value_policy::reference);
        }
    );

    py::class_<Animation2d, Component>(m, "Animation2d")
        .def(py::init<std::vector<int>, float, bool, bool, bool>(), 
             py::arg("frames"), py::arg("speed"), py::arg("loop") = true, py::arg("flip_x") = false, py::arg("flip_y") = false);

    ComponentRegistry::Register<Animation2d>("Animation2d", 
        [](GameObject& go, py::args args) -> py::object {
            if (args.size() < 2) {
                std::cerr << "[Python] Animation2d requires (frames_list, speed, [loop])!" << std::endl;
                return py::none();
            }
            std::vector<int> frames = args[0].cast<std::vector<int>>();
            float speed = args[1].cast<float>();
            bool loop = true; // Default value
            
            if (args.size() >= 3) {
                loop = args[2].cast<bool>();
            }
            
            auto* a = go.AddComponent<Animation2d>(frames, speed, loop);
            return py::cast(a, py::return_value_policy::reference);
        }
    );

    py::enum_<LoopMode>(m, "LoopMode")
        .value("Once", LoopMode::Once)
        .value("Loop", LoopMode::Loop)
        .value("PingPong", LoopMode::PingPong)
        .export_values();

    py::class_<Animator2d, Component>(m, "Animator2d")
        .def(py::init<>())
        .def("add_animation", &Animator2d::AddAnimation, 
            py::arg("name"), 
            py::arg("frames"), 
            py::arg("frame_duration"), 
            py::arg("loop"),
            py::arg("flip_x") = false, 
            py::arg("flip_y") = false,
            py::arg("events") = std::unordered_map<int, std::vector<std::string>>()
        )
        .def("play", &Animator2d::Play)
        .def("pause", &Animator2d::Pause)
        .def("resume", &Animator2d::Resume)
        .def("stop", &Animator2d::Stop)
        .def("is_playing", &Animator2d::IsPlaying)
        .def("is_finished", &Animator2d::IsFinished)
        .def_property("playback_speed", &Animator2d::GetPlaybackSpeed, &Animator2d::SetPlaybackSpeed);

    ComponentRegistry::Register<Animator2d>("Animator2d", 
        [](GameObject& go, py::args args) -> py::object {
            // Animator2d does not require any arguments upon creation.
            // Animations are added later via the 'add_animation' method in Python.
            auto* animator = go.AddComponent<Animator2d>();
            
            // Return the reference to Python
            return py::cast(animator, py::return_value_policy::reference);
        }
    );

    py::class_<Color>(m, "Color")
        .def(py::init<unsigned char, unsigned char, unsigned char, unsigned char>(),
             py::arg("r") = 0, py::arg("g") = 0, py::arg("b") = 0, py::arg("a") = 255)
        .def_readwrite("r", &Color::r)
        .def_readwrite("g", &Color::g)
        .def_readwrite("b", &Color::b)
        .def_readwrite("a", &Color::a)
        .def("__repr__", [](const Color& c) {
            return "<Color(r=" + std::to_string(c.r) + ", g=" + 
                   std::to_string(c.g) + ", b=" + 
                   std::to_string(c.b) + ", a=" + 
                   std::to_string(c.a) + ")>";
        });

    py::enum_<Camera2dAnchor>(m, "Camera2dAnchor")
        .value("TopLeft", Camera2dAnchor::TopLeft)
        .value("Center", Camera2dAnchor::Center)
        .export_values();

    py::class_<Camera2d, Component>(m, "Camera2d")
        .def(py::init<>())
        .def_readwrite("zoom", &Camera2d::zoom)
        .def_readwrite("offset", &Camera2d::offset)
        .def_readwrite("anchor", &Camera2d::anchor)
        .def_readwrite("is_main", &Camera2d::isMain)
        .def_readwrite("background_color", &Camera2d::backgroundColor)
        .def("shake", &Camera2d::Shake);

    ComponentRegistry::Register<Camera2d>("Camera2d",
        [](GameObject& go, py::args args) -> py::object {
            auto* cam = go.AddComponent<Camera2d>();
            return py::cast(cam, py::return_value_policy::reference);
        }
    );

    py::class_<TileLayer>(m, "TileLayer")
        .def_readwrite("name", &TileLayer::name)
        .def_readwrite("is_visible", &TileLayer::isVisible)
        .def_readwrite("is_solid", &TileLayer::isSolid);

    py::class_<Tileset>(m, "Tileset")
        .def_readonly("name", &Tileset::name)
        .def_readonly("first_tile_id", &Tileset::firstTileID)
        .def_readonly("tile_count", &Tileset::tileCount)
        .def_readonly("columns", &Tileset::columns);

    py::class_<TileMap, Component>(m, "TileMap")
        .def(py::init<>())
        // Expose public properties
        .def_readwrite("grid_width", &TileMap::gridWidth)
        .def_readwrite("grid_height", &TileMap::gridHeight)
        .def_readwrite("tile_spacing", &TileMap::tileSpacing)
        // Expose public methods
        .def("add_tileset", py::overload_cast<const std::string&>(&TileMap::AddTileset))
        .def("clear_tilesets", &TileMap::ClearTilesets)
        .def("get_tilesets", &TileMap::GetTilesets, py::return_value_policy::reference_internal)
        .def("resize", &TileMap::ResizeMap)
        .def("add_layer", &TileMap::AddLayer)
        .def("get_tile", &TileMap::GetTile)
        .def("set_tile", &TileMap::SetTile)
        // Overloaded methods to get a layer by index or by name
        .def("get_layer", py::overload_cast<int>(&TileMap::GetLayer), py::return_value_policy::reference_internal)
        .def("get_layer", py::overload_cast<const std::string&>(&TileMap::GetLayer), py::return_value_policy::reference_internal);

    ComponentRegistry::Register<TileMap>("TileMap", 
        [](GameObject& go, py::args args) -> py::object {
            // TileMap does not require constructor arguments
            auto* tileMap = go.AddComponent<TileMap>();
            return py::cast(tileMap, py::return_value_policy::reference);
        }
    );

    py::class_<ParticleSystem2d, Component>(m, "ParticleSystem2d")
        .def(py::init<>())
        // Bind the burst method
        .def("emit_burst", &ParticleSystem2d::EmitBurst, py::arg("count"))
        // Bind all the public parameters so they can be tweaked from Python
        .def_readwrite("is_emitting", &ParticleSystem2d::isEmitting)
        .def_readwrite("emission_rate", &ParticleSystem2d::emissionRate)
        .def_readwrite("life_min", &ParticleSystem2d::lifeMin)
        .def_readwrite("life_max", &ParticleSystem2d::lifeMax)
        .def_readwrite("speed_min", &ParticleSystem2d::speedMin)
        .def_readwrite("speed_max", &ParticleSystem2d::speedMax)
        .def_readwrite("emission_angle", &ParticleSystem2d::emissionAngle)
        .def_readwrite("angle_spread", &ParticleSystem2d::angleSpread)
        .def_readwrite("gravity", &ParticleSystem2d::gravity)
        .def_readwrite("start_color", &ParticleSystem2d::startColor)
        .def_readwrite("end_color", &ParticleSystem2d::endColor)
        .def_readwrite("start_size", &ParticleSystem2d::startSize)
        .def_readwrite("end_size", &ParticleSystem2d::endSize);

    ComponentRegistry::Register<ParticleSystem2d>("ParticleSystem2d",
        [](GameObject& go, py::args args) -> py::object {
            // ParticleSystem2d does not require constructor arguments
            auto* ps = go.AddComponent<ParticleSystem2d>();
            return py::cast(ps, py::return_value_policy::reference);
        }
    );

    py::class_<Camera3d, Component>(m, "Camera3d")
        .def(py::init<>())
        // Explicitly map FOV and main camera flag
        .def_readwrite("fov", &Camera3d::fov)
        .def_readwrite("is_main", &Camera3d::isMain)
        .def_readwrite("projection", &Camera3d::projection);

    ComponentRegistry::Register<Camera3d>("Camera3d",
        [](GameObject& go, py::args args) -> py::object {
            auto* cam = go.AddComponent<Camera3d>();
            return py::cast(cam, py::return_value_policy::reference);
        }
    );

    py::class_<CuboidMesh, Component>(m, "CuboidMesh")
        .def(py::init<>())
        .def_readwrite("size", &CuboidMesh::size)
        .def_readwrite("color", &CuboidMesh::color)
        .def_readwrite("is_visible", &CuboidMesh::isVisible)
        .def_readwrite("draw_wireframe", &CuboidMesh::drawWireframe);

    ComponentRegistry::Register<CuboidMesh>("CuboidMesh",
        [](GameObject& go, py::args args) -> py::object {
            auto* mesh = go.AddComponent<CuboidMesh>();
            return py::cast(mesh, py::return_value_policy::reference);
        }
    );

    py::class_<DirectionalLight, Component>(m, "DirectionalLight")
        .def(py::init<>())
        .def_readwrite("color", &DirectionalLight::color)
        .def_readwrite("intensity", &DirectionalLight::intensity);

    ComponentRegistry::Register<DirectionalLight>("DirectionalLight",
        [](GameObject& go, py::args args) -> py::object {
            auto* light = go.AddComponent<DirectionalLight>();
            return py::cast(light, py::return_value_policy::reference);
        }
    );

    py::class_<PointLight, Component>(m, "PointLight")
        .def(py::init<>())
        .def_readwrite("color", &PointLight::color)
        .def_readwrite("intensity", &PointLight::intensity)
        .def_readwrite("radius", &PointLight::radius);

    ComponentRegistry::Register<PointLight>("PointLight",
        [](GameObject& go, py::args args) -> py::object {
            auto* light = go.AddComponent<PointLight>();
            return py::cast(light, py::return_value_policy::reference);
        }
    );

    py::class_<MeshRenderer, Component>(m, "MeshRenderer")
        .def(py::init<>())
        .def("load_model", &MeshRenderer::LoadModelFromPath, py::arg("path"))
        .def_readwrite("tint", &MeshRenderer::tint)
        .def_readonly("is_loaded", &MeshRenderer::isLoaded);

    ComponentRegistry::Register<MeshRenderer>("MeshRenderer",
        [](GameObject& go, py::args args) -> py::object {
            auto* mr = go.AddComponent<MeshRenderer>();
            return py::cast(mr, py::return_value_policy::reference);
        }
    );

    py::class_<Animator3d, Component>(m, "Animator3d")
        .def(py::init<>())
        // Expose core methods
        .def("load_animations", &Animator3d::LoadAnimationsFromPath, py::arg("path"))
        .def("play", &Animator3d::Play, py::arg("anim_index") = 0)
        .def("pause", &Animator3d::Pause)
        .def("stop", &Animator3d::Stop)
        // Expose editable properties
        .def_readwrite("speed", &Animator3d::speed)
        .def_readwrite("loop", &Animator3d::loop)
        // Expose read-only state properties
        .def_readonly("current_anim_index", &Animator3d::currentAnimIndex)
        .def_readonly("current_frame", &Animator3d::currentFrame)
        .def_readonly("is_playing", &Animator3d::isPlaying);

    ComponentRegistry::Register<Animator3d>("Animator3d",
        [](GameObject& go, py::args args) -> py::object {
            auto* animator = go.AddComponent<Animator3d>();
            return py::cast(animator, py::return_value_policy::reference);
        }
    );
}
