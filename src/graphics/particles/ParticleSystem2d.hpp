#pragma once

#include "scene/Component.hpp"
#include <raylib.h>
#include <vector>

// The base structure of an individual particle
struct Particle {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float rotation;
    float size;

    float life;     // Remaining lifetime
    float maxLife;  // Total initial lifetime (used to calculate percentages)
    bool active;    // If false, the particle is dead and ready to be reused
};

// The emitter component
class ParticleSystem2d : public Component {
    public:
        ParticleSystem2d();
        ~ParticleSystem2d() = default;

        void Update(float deltaTime) override;
        void Render() override; // Ensures the particles are displayed

        // Method to force a burst of particles (ideal for explosions or monster deaths)
        void EmitBurst(int count);

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

        // Emitter parameters
        bool isEmitting = true;       // Is the emitter continuously active?
        float emissionRate = 50.0f;   // Particles generated per second

        // Lifetime (Min/Max for randomness)
        float lifeMin = 0.5f;
        float lifeMax = 1.5f;

        // Speed and Direction
        float speedMin = 50.0f;
        float speedMax = 150.0f;
        float emissionAngle = -90.0f; // Main direction (-90 is straight up in 2D)
        float angleSpread = 45.0f;    // Cone of dispersion in degrees

        // Physics
        float gravity = 0.0f;         // Applies downward force over time

        // Visuals (Interpolation)
        Color startColor = WHITE;
        Color endColor = {255, 255, 255, 0}; // Becomes transparent at the end
        float startSize = 10.0f;
        float endSize = 0.0f;

    private:
        std::vector<Particle> m_particles;
        int m_maxParticles = 1000;
        float m_emissionTimer = 0.0f;
        
        // Finds a dead particle in the pool to reuse (Object Pooling)
        int GetFirstUnusedParticle();
        void ResetParticle(Particle& p);

        // Quick helper for random floats
        float RandomFloat(float min, float max);
};
