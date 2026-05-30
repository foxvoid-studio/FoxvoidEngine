#include "scripting/ScriptBindings.hpp"
#include "scene/Component.hpp"
#include "scene/ComponentRegistry.hpp"
#include "audio/AudioSource.hpp"

void BindAudio(py::module_& m) {
    py::class_<AudioSource, Component>(m, "AudioSource")
        .def(py::init<>())
        .def("load_sfx", py::overload_cast<const std::string&, const std::string&>(&AudioSource::LoadSFX))
        .def("play_sfx", &AudioSource::PlaySFX, py::arg("name"))
        .def("load_music", py::overload_cast<const std::string&>(&AudioSource::LoadMusic))
        .def("play_music", &AudioSource::PlayMusic)
        .def("stop_music", &AudioSource::StopMusic)
        .def("set_music_volume", &AudioSource::SetMusicVolume, py::arg("volume"));

    ComponentRegistry::Register<AudioSource>("AudioSource",
        [](GameObject& go, py::args args) -> py::object {
            auto* a = go.AddComponent<AudioSource>();
            return py::cast(a, py::return_value_policy::reference);
        }
    );
}
