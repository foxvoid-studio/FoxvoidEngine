#pragma once
#include <pybind11/pybind11.h>

namespace py = pybind11;

// Forward declarations of binding functions
void BindInput(py::module_& m);
void BindMathAndPhysics(py::module_& m);
void BindGraphics(py::module_& m);
void BindCore(py::module_& m);
void BindGUI(py::module_& m);
void BindAudio(py::module_& m);
void BindNetwork(py::module_& m);
