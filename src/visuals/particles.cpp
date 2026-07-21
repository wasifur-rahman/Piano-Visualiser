#include "particles.h"
#include "blocks.h"
#include "layout.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
    struct Particle {
        float x;
        float y;
        float vx;
        float vy;
        Color color;
        float lifetime; // in seconds
    };

    struct HeldNoteEmitter {
        int pitch;
        int velocity;
        float emitTimer = 0.0f;
    };

    std::vector<Particle> particleList;
    std::vector<HeldNoteEmitter> noteEmitters;
}

namespace particles {

    void spawn(float x, float y, Color c, int velocity) {
        int numParticles = 2 + velocity / 64; // Adjust particle count based on velocity
        for (int i = 0; i < numParticles; ++i) {
            float angle = ((float)rand() / RAND_MAX) * 2.0f * 3.14159f;
            float speed = ((float)rand() / RAND_MAX) * 100.0f + 50.0f; // Random speed between 50 and 150
            particleList.push_back({
                x,
                y,
                (float)(cos(angle) * speed),
                (float)(sin(angle) * speed),
                c,
                0.5f + ((float)rand() / RAND_MAX) * 1.0f // Lifetime between 0.5 and 1.0 seconds
            });
        }
    }

    void update(float dt, int screenWidth, int screenHeight, int pianoHeight) {
        for (auto& p : particleList) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.lifetime -= dt;
        }
        particleList.erase(
            std::remove_if(particleList.begin(), particleList.end(),
                [](const Particle& p) { return p.lifetime <= 0; }),
            particleList.end());

        const auto& activeNotes = blocks::getActive();
        std::vector<HeldNoteEmitter> nextEmitters;
        nextEmitters.reserve(activeNotes.size());

        for (const auto& note : activeNotes) {
            if (!note.held) {
                continue;
            }

            auto existingIt = std::find_if(noteEmitters.begin(), noteEmitters.end(),
                [&](const HeldNoteEmitter& emitter) { return emitter.pitch == note.pitch; });

            HeldNoteEmitter emitter;
            if (existingIt != noteEmitters.end()) {
                emitter = *existingIt;
            }
            else {
                emitter.pitch = note.pitch;
                emitter.velocity = note.velocity;
            }

            emitter.emitTimer += dt;
            constexpr float emitInterval = 0.035f;
            while (emitter.emitTimer >= emitInterval) {
                emitter.emitTimer -= emitInterval;

                KeyRect kr = layout::getKeyRect(note.pitch, screenWidth);
                float spawnX = kr.x + kr.width / 2.0f;
                float spawnY = (float)(screenHeight - pianoHeight);
                Color c = layout::colorForPitch(note.pitch, note.velocity);
                spawn(spawnX, spawnY, c, note.velocity);
            }

            nextEmitters.push_back(emitter);
        }

        noteEmitters.swap(nextEmitters);
    }

    void draw() {
        for (auto& p : particleList) {
            unsigned char alpha = (unsigned char)(255 * (p.lifetime / 1.0f));
            Color c = p.color;
            c.a = alpha;
            DrawCircle((int)p.x, (int)p.y, 5, c);
        }
    }

}
