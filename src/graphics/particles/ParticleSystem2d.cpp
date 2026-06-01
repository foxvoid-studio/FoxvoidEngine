#include "ParticleSystem2d.hpp"
#include "scene/GameObject.hpp"
#include "math/Transform2d.hpp"
#include <raymath.h>
#include <cstdlib>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include <imgui.h>
#endif

ParticleSystem2d::ParticleSystem2d() {
    // Pre-allocate the memory for all particles at start to prevent mid-game lag (Object Pooling)
    m_particles.resize(m_maxParticles);
    for (auto& p : m_particles) {
        p.active = false;
    }
}

float ParticleSystem2d::RandomFloat(float min, float max) {
    // Generates a random float between min and max
    float random = ((float)rand()) / (float)RAND_MAX;
    return min + random * (max - min);
}

int ParticleSystem2d::GetFirstUnusedParticle() {
    // Find the first particle that is not currently active
    for (int i = 0; i < m_maxParticles; i++) {
        if (!m_particles[i].active) {
            return i;
        }
    }
    return -1; // Pool is full
}

void ParticleSystem2d::ResetParticle(Particle& p) {
    // Make the particle active
    p.active = true;

    // Position: Particles spawn at the emitter's global position
    Vector2 spawnPos = { 0.0f, 0.0f };
    if (owner) {
        if (auto transform = owner->GetComponent<Transform2d>()) {
            spawnPos = transform->GetGlobalPosition();
        }
    }
    p.position = spawnPos;

    // Lifetime
    p.maxLife = RandomFloat(lifeMin, lifeMax);
    p.life = p.maxLife;

    // Visuals (Start values)
    p.color = startColor;
    p.size = startSize;
    p.rotation = RandomFloat(0.0f, 360.0f); // Random initial rotation

    // Velocity (Trigonometry to calculate vector from angle)
    float randomAngle = RandomFloat(emissionAngle - (angleSpread / 2.0f), emissionAngle + (angleSpread / 2.0f));
    float randomSpeed = RandomFloat(speedMin, speedMax);

    // Convert degrees to radians for math functions
    float radians = randomAngle * DEG2RAD;
    
    p.velocity.x = cos(radians) * randomSpeed;
    p.velocity.y = sin(radians) * randomSpeed;
}

void ParticleSystem2d::EmitBurst(int count) {
    for (int i = 0; i < count; i++) {
        int index = GetFirstUnusedParticle();
        if (index != -1) {
            ResetParticle(m_particles[index]);
        }
    }
}

void ParticleSystem2d::Update(float deltaTime) {
    // Handle Continuous Emission
    if (isEmitting && emissionRate > 0.0f) {
        m_emissionTimer += deltaTime;
        float timeBetweenParticles = 1.0f / emissionRate;

        // While loop allows catching up if emission rate is extremely high
        while (m_emissionTimer >= timeBetweenParticles) {
            m_emissionTimer -= timeBetweenParticles;
            
            int index = GetFirstUnusedParticle();
            if (index != -1) {
                ResetParticle(m_particles[index]);
            }
        }
    }

    // Handle Physics and Lifecycle for active particles
    for (auto& p : m_particles) {
        if (!p.active) continue;

        // Decrease life
        p.life -= deltaTime;
        if (p.life <= 0.0f) {
            p.active = false;
            continue;
        }

        // Apply physics
        p.velocity.y += gravity * deltaTime; // Apply gravity
        p.position.x += p.velocity.x * deltaTime;
        p.position.y += p.velocity.y * deltaTime;

        // Calculate progress (0.0 at birth, 1.0 at death)
        float progress = 1.0f - (p.life / p.maxLife);

        // Interpolate Size
        p.size = startSize + (endSize - startSize) * progress;

        // Interpolate Color (RGBA individually)
        p.color.r = (unsigned char)(startColor.r + (endColor.r - startColor.r) * progress);
        p.color.g = (unsigned char)(startColor.g + (endColor.g - startColor.g) * progress);
        p.color.b = (unsigned char)(startColor.b + (endColor.b - startColor.b) * progress);
        p.color.a = (unsigned char)(startColor.a + (endColor.a - startColor.a) * progress);
    }
}

void ParticleSystem2d::Render() {
    // Draw all active particles
    for (const auto& p : m_particles) {
        if (!p.active) continue;

        Rectangle rec = { p.position.x, p.position.y, p.size, p.size };
        Vector2 origin = { p.size / 2.0f, p.size / 2.0f }; // Center origin for rotation
        
        DrawRectanglePro(rec, origin, p.rotation, p.color);
    }
}

std::string ParticleSystem2d::GetName() const {
    return "Particle System 2D";
}

#ifndef STANDALONE_MODE
void ParticleSystem2d::OnInspector() {
    if (ImGui::TreeNodeEx("Emission", ImGuiTreeNodeFlags_DefaultOpen)) {
        EditorUI::Checkbox("Is Emitting", &isEmitting, this);
        EditorUI::DragFloat("Emission Rate", &emissionRate, 1.0f, this, 0.0f, 1000.0f);
        
        if (ImGui::Button("Emit Burst (50)", ImVec2(-1, 0))) {
            EmitBurst(50);
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Lifetime", ImGuiTreeNodeFlags_DefaultOpen)) {
        EditorUI::DragFloat("Life Min", &lifeMin, 0.1f, this, 0.1f, 10.0f);
        EditorUI::DragFloat("Life Max", &lifeMax, 0.1f, this, 0.1f, 10.0f);
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Physics & Movement", ImGuiTreeNodeFlags_DefaultOpen)) {
        EditorUI::DragFloat("Angle", &emissionAngle, 1.0f, this, -360.0f, 360.0f);
        EditorUI::DragFloat("Spread", &angleSpread, 1.0f, this, 0.0f, 360.0f);
        EditorUI::DragFloat("Speed Min", &speedMin, 5.0f, this, 0.0f, 1000.0f);
        EditorUI::DragFloat("Speed Max", &speedMax, 5.0f, this, 0.0f, 1000.0f);
        EditorUI::DragFloat("Gravity", &gravity, 5.0f, this, -1000.0f, 1000.0f);
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Visuals", ImGuiTreeNodeFlags_DefaultOpen)) {
        EditorUI::DragFloat("Start Size", &startSize, 0.5f, this, 0.0f, 100.0f);
        EditorUI::DragFloat("End Size", &endSize, 0.5f, this, 0.0f, 100.0f);
        EditorUI::ColorEdit4("Start Color", &startColor, this);
        EditorUI::ColorEdit4("End Color", &endColor, this);
        ImGui::TreePop();
    }
}
#endif

nlohmann::json ParticleSystem2d::Serialize() const {
    return {
        { "type", "ParticleSystem2d" },
        { "isEmitting", isEmitting },
        { "emissionRate", emissionRate },
        { "lifeMin", lifeMin },
        { "lifeMax", lifeMax },
        { "speedMin", speedMin },
        { "speedMax", speedMax },
        { "emissionAngle", emissionAngle },
        { "angleSpread", angleSpread },
        { "gravity", gravity },
        { "startSize", startSize },
        { "endSize", endSize },
        { "startColor", { startColor.r, startColor.g, startColor.b, startColor.a } },
        { "endColor", { endColor.r, endColor.g, endColor.b, endColor.a } }
    };
}

void ParticleSystem2d::Deserialize(const nlohmann::json& j) {
    isEmitting = j.value("isEmitting", true);
    emissionRate = j.value("emissionRate", 50.0f);
    lifeMin = j.value("lifeMin", 0.5f);
    lifeMax = j.value("lifeMax", 1.5f);
    speedMin = j.value("speedMin", 50.0f);
    speedMax = j.value("speedMax", 150.0f);
    emissionAngle = j.value("emissionAngle", -90.0f);
    angleSpread = j.value("angleSpread", 45.0f);
    gravity = j.value("gravity", 0.0f);
    startSize = j.value("startSize", 10.0f);
    endSize = j.value("endSize", 0.0f);

    if (j.contains("startColor") && j["startColor"].is_array() && j["startColor"].size() == 4) {
        startColor = { j["startColor"][0], j["startColor"][1], j["startColor"][2], j["startColor"][3] };
    }
    if (j.contains("endColor") && j["endColor"].is_array() && j["endColor"].size() == 4) {
        endColor = { j["endColor"][0], j["endColor"][1], j["endColor"][2], j["endColor"][3] };
    }
}
