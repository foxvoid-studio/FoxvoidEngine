#include "scripting/ScriptBindings.hpp"
#include <raylib.h>
#include <raymath.h>
#include <iostream>
#include <pybind11/stl.h>
#include "world/GameObject.hpp"
#include "physics/Transform2d.hpp"
#include <world/ComponentRegistry.hpp>
#include "physics/RectCollider.hpp"
#include "physics/PolygonCollider.hpp"
#include "physics/RigidBody2d.hpp"
#include "physics/Collision2D.hpp"
#include <physics/PhysicsEngine.hpp>
#include <core/Engine.hpp>
#include <physics/CircleCollider.hpp>
#include <physics/CapsuleCollider.hpp>

void BindMathAndPhysics(py::module_& m) {
    py::class_<Vector2>(m, "Vector2")
        .def(py::init<>()) // Default constructor
        .def(py::init<float, float>(), py::arg("x"), py::arg("y")) // Constructor with params
        // def_readwrite exposes the public member variable directly with direct memory access
        .def_readwrite("x", &Vector2::x)
        .def_readwrite("y", &Vector2::y)

        // Operator Overloading for Python (+, -, *, /)
        .def("__add__", [](const Vector2& a, const Vector2& b) {
            return Vector2Add(a, b);
        })
        .def("__sub__", [](const Vector2& a, const Vector2& b) {
            return Vector2Subtract(a, b);
        })
        .def("__mul__", [](const Vector2& a, float scalar) {
            return Vector2Scale(a, scalar); // Multiply Vector by a float
        })
        .def("__truediv__", [](const Vector2& a, float scalar) {
            // Prevent division by zero
            if (scalar == 0.0f) return a;
            return Vector2Scale(a, 1.0f / scalar); 
        })

        // Utility Methods (Executed in fast C++)
        .def("length", [](const Vector2& a) {
            return Vector2Length(a);
        })
        .def("distance_to", [](const Vector2& a, const Vector2& b) {
            return Vector2Distance(a, b);
        })
        .def("normalized", [](const Vector2& a) {
            return Vector2Normalize(a);
        })
        
        // String representation for easy debugging in Python (print(my_vec))
        .def("__repr__", [](const Vector2& a) {
            return "Vector2(" + std::to_string(a.x) + ", " + std::to_string(a.y) + ")";
        });

    py::class_<Rectangle>(m, "Rectangle")
        .def(py::init<>())
        .def(py::init<float, float, float, float>(), 
             py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"))
        .def_readwrite("x", &Rectangle::x)
        .def_readwrite("y", &Rectangle::y)
        .def_readwrite("width", &Rectangle::width)
        .def_readwrite("height", &Rectangle::height);

    py::class_<Transform2d, Component>(m, "Transform2d")
        .def(py::init<float, float>(), py::arg("x") = 0.0f, py::arg("y") = 0.0f)
        
        // Local properties
        .def_readwrite("position", &Transform2d::position)
        .def_readwrite("scale", &Transform2d::scale)
        .def_readwrite("rotation", &Transform2d::rotation)
        .def_readwrite("z_index", &Transform2d::zIndex)

        // Global Getters
        .def("get_global_position", &Transform2d::GetGlobalPosition)
        .def("get_global_rotation", &Transform2d::GetGlobalRotation)
        .def("get_global_scale", &Transform2d::GetGlobalScale)
        .def("get_global_z_index", &Transform2d::GetGlobalZIndex)
        
        // Global Setters
        .def("set_global_position", &Transform2d::SetGlobalPosition, py::arg("target_global_pos"))
        .def("set_global_rotation", &Transform2d::SetGlobalRotation, py::arg("target_global_rot"))
        .def("set_global_scale", &Transform2d::SetGlobalScale, py::arg("target_global_scale"));

    ComponentRegistry::Register<Transform2d>("Transform2d", 
        [](GameObject& go, py::args args) -> py::object {
            float x = 0.0f, y = 0.0f;
            
            // Extract optional X and Y if Python provided them
            if (args.size() >= 1) x = args[0].cast<float>();
            if (args.size() >= 2) y = args[1].cast<float>();
            
            auto* t = go.AddComponent<Transform2d>(x, y);
            return py::cast(t, py::return_value_policy::reference);
        }
    );

    py::class_<Collision2D>(m, "Collision2D")
        .def_readwrite("other", &Collision2D::other, py::return_value_policy::reference)
        .def_readwrite("normal", &Collision2D::normal);

    py::class_<RectCollider, Component>(m, "RectCollider")
        .def(py::init<float, float>(), py::arg("width") = 50.0f, py::arg("height") = 50.0f)
        .def_readwrite("size", &RectCollider::size)
        .def_readwrite("offset", &RectCollider::offset)
        .def_readwrite("is_trigger", &RectCollider::isTrigger);

    ComponentRegistry::Register<RectCollider>("RectCollider",
        [](GameObject& go, py::args args) -> py::object {
            float width = 50.0f;
            float height = 50.0f;

            // Extract optional width and height if Python provided them
            if (args.size() >= 1) width = args[0].cast<float>();
            if (args.size() >= 2) height = args[1].cast<float>();

            auto* t = go.AddComponent<RectCollider>(width, height);
            return py::cast(t, py::return_value_policy::reference);
        }
    );

    py::class_<PolygonCollider, Component>(m, "PolygonCollider")
        .def(py::init<>()) // Uses the default constructor (triangle)
        // With <pybind11/stl.h>, Python lists of Vector2 are natively converted to std::vector<Vector2>
        .def_readwrite("local_vertices", &PolygonCollider::localVertices) 
        .def_readwrite("offset", &PolygonCollider::offset)
        .def_readwrite("is_trigger", &PolygonCollider::isTrigger);

    ComponentRegistry::Register<PolygonCollider>("PolygonCollider",
        [](GameObject& go, py::args args) -> py::object {
            // We just add it with default values for now. 
            // In python: go.add_component(PolygonCollider)
            auto* t = go.AddComponent<PolygonCollider>();
            return py::cast(t, py::return_value_policy::reference);
        }
    );

    py::class_<RigidBody2d, Component>(m, "RigidBody2d")
        .def(py::init<>())
        .def_readwrite("velocity", &RigidBody2d::velocity)
        .def_readwrite("mass", &RigidBody2d::mass)
        .def_readwrite("gravity_scale", &RigidBody2d::gravityScale)
        .def_readwrite("is_kinematic", &RigidBody2d::isKinematic)
        .def_readonly("is_grounded", &RigidBody2d::isGrounded);

    ComponentRegistry::Register<RigidBody2d>("RigidBody2d",
        [](GameObject& go, py::args args) -> py::object {
            auto* t = go.AddComponent<RigidBody2d>();
            return py::cast(t, py::return_value_policy::reference);
        }
    );

    py::class_<RaycastHit>(m, "RaycastHit")
        .def_readonly("hit", &RaycastHit::hit)
        .def_readonly("collider", &RaycastHit::collider) // Returns the GameObject
        .def_property_readonly("point", [](const RaycastHit& r) { return py::make_tuple(r.point.x, r.point.y); })
        .def_readonly("distance", &RaycastHit::distance);

    py::class_<PhysicsEngine>(m, "Physics")
        // Use a lambda to inject the active scene from the Engine
        .def_static("raycast", [](py::tuple origin, py::tuple direction, float distance) {
            Vector2 o = { origin[0].cast<float>(), origin[1].cast<float>() };
            Vector2 d = { direction[0].cast<float>(), direction[1].cast<float>() };
            Scene& activeScene = Engine::Get()->GetActiveScene();
            return PhysicsEngine::Raycast(activeScene, o, d, distance);
        });

    py::class_<CircleCollider, Component>(m, "CircleCollider")
        .def(py::init<float>(), py::arg("radius") = 25.0f)
        .def_readwrite("radius", &CircleCollider::radius)
        .def_readwrite("offset", &CircleCollider::offset)
        .def_readwrite("is_trigger", &CircleCollider::isTrigger);

    ComponentRegistry::Register<CircleCollider>("CircleCollider",
        [](GameObject& go, py::args args) -> py::object {
            float radius = 25.0f;
            if (args.size() >= 1) radius = args[0].cast<float>();
            auto* c = go.AddComponent<CircleCollider>(radius);
            return py::cast(c, py::return_value_policy::reference);
        }
    );

    py::class_<CapsuleCollider, Component>(m, "CapsuleCollider")
        .def(py::init<float, float>(), py::arg("radius") = 25.0f, py::arg("height") = 50.0f)
        .def_readwrite("radius", &CapsuleCollider::radius)
        .def_readwrite("height", &CapsuleCollider::height)
        .def_readwrite("offset", &CapsuleCollider::offset)
        .def_readwrite("is_trigger", &CapsuleCollider::isTrigger);

    ComponentRegistry::Register<CapsuleCollider>("CapsuleCollider",
        [](GameObject& go, py::args args) -> py::object {
            float radius = 25.0f;
            float height = 50.0f;
            if (args.size() >= 1) radius = args[0].cast<float>();
            if (args.size() >= 2) height = args[1].cast<float>();
            auto* c = go.AddComponent<CapsuleCollider>(radius, height);
            return py::cast(c, py::return_value_policy::reference);
        }
    );
}
