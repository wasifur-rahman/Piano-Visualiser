#include "audio.h"
#include <fluidsynth.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
    fluid_settings_t* settings = nullptr;
    fluid_synth_t* synth = nullptr;
    fluid_audio_driver_t* audioDriver = nullptr;

    // Live-adjustable audio state. These are the source of truth; init() seeds
    // the synth from them, and the setters below keep them in sync afterward.
    float currentGain = 3.0f;

    double reverbRoomSize = 0.5;
    double reverbDamping = 0.0;
    double reverbWidth = 5.0;
    double reverbLevel = 0.0;

    int chorusVoices = 3;
    double chorusLevel = 1.2;
    double chorusSpeed = 0.3;
    double chorusDepth = 8.0;

    std::string resolveSoundfontPath() {
        const std::vector<std::string> candidates = {
            "assets/SalC5Light2.sf2",
            "SalC5Light2.sf2"
        };

        const std::filesystem::path currentDir = std::filesystem::current_path();
        const std::filesystem::path sourceDir = std::filesystem::path(__FILE__).parent_path();
        const std::filesystem::path projectRoot = sourceDir.parent_path();

        const std::vector<std::filesystem::path> searchRoots = {
            currentDir,
            currentDir / "assets",
            sourceDir,
            sourceDir / "assets",
            projectRoot,
            projectRoot / "assets",
            std::filesystem::path("C:/Users/wasif/Desktop/PianoVisualiser/assets")
        };

        for (const auto& root : searchRoots) {
            for (const auto& candidate : candidates) {
                const std::filesystem::path path = root / candidate;
                if (std::filesystem::exists(path)) {
                    return std::filesystem::absolute(path).string();
                }
            }
        }

        return {};
    }

    bool tryCreateAudioDriver(const char* driverName) {
        if (!settings || !synth) {
            return false;
        }

        fluid_settings_setstr(settings, "audio.driver", driverName);
        audioDriver = new_fluid_audio_driver(settings, synth);
        if (audioDriver) {
            std::cout << "FluidSynth audio driver initialized: " << driverName << std::endl;
            return true;
        }

        std::cerr << "FluidSynth audio driver failed: " << driverName << std::endl;
        return false;
    }
}
namespace audio {
    void init() {
        settings = new_fluid_settings();
        fluid_settings_setnum(settings, "synth.gain", 1.0);
        fluid_settings_setint(settings, "audio.period-size", 64); // smaller buffer = lower latency, but more CPU usage
        fluid_settings_setint(settings, "audio.periods", 2); // number of periods in the audio buffer
        fluid_settings_setint(settings, "synth.interpolation", 4); // linear interpolation for better sound quality

        synth = new_fluid_synth(settings);
        if (!tryCreateAudioDriver("wasapi") && !tryCreateAudioDriver("dsound") && !tryCreateAudioDriver("default")) {
            std::cerr << "FluidSynth: no audio driver could be initialized." << std::endl;
        }

        const std::string soundfontPath = resolveSoundfontPath();
        std::cout << "Resolved SoundFont path: " << soundfontPath << std::endl;

        if (soundfontPath.empty()) {
            std::cerr << "SoundFont not found. Expected assets/SalC5Light2.sf2 under the project root." << std::endl;
            return;
        }

        int sfID = fluid_synth_sfload(synth, soundfontPath.c_str(), 1);
        if (sfID == FLUID_FAILED) {
            std::cerr << "Failed to load SoundFont: " << soundfontPath << std::endl;
            return;
        }

        fluid_synth_program_select(synth, 0, sfID, 0, 0); // channel 0, bank 0, preset 0 (usually grand piano)

        // Built-in reverb/chorus - seeded from the adjustable state above so that
        // any settings restored from disk (if you add that later) take effect here.
        fluid_synth_set_reverb(synth, reverbRoomSize, reverbDamping, reverbWidth, reverbLevel);
        fluid_synth_set_chorus(synth, chorusVoices, chorusLevel, chorusSpeed, chorusDepth, FLUID_CHORUS_MOD_SINE);
        fluid_synth_set_gain(synth, currentGain);
    }

    void noteOn(int pitch, int velocity) {
        if (!synth) {
            return;
        }

        const int safeVelocity = std::clamp(velocity, 0, 127);
        if (safeVelocity <= 0) {
            return;
        }

        fluid_synth_noteon(synth, 0, pitch, safeVelocity);
    }

    void noteOff(int pitch) {
        fluid_synth_noteoff(synth, 0, pitch);
    }

    void controlChange(int cc, int value) {
        fluid_synth_cc(synth, 0, cc, value);
    }

    void shutdown() {
        delete_fluid_audio_driver(audioDriver);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }

    void setGain(float gain) {
        currentGain = std::clamp(gain, 0.0f, 10.0f);
        if (synth) {
            fluid_synth_set_gain(synth, currentGain);
        }
    }

    float getGain() {
        return currentGain;
    }

    void setReverb(double roomSize, double damping, double width, double level) {
        reverbRoomSize = roomSize;
        reverbDamping = damping;
        reverbWidth = width;
        reverbLevel = level;
        if (synth) {
            fluid_synth_set_reverb(synth, reverbRoomSize, reverbDamping, reverbWidth, reverbLevel);
        }
    }

    void getReverb(double& roomSize, double& damping, double& width, double& level) {
        roomSize = reverbRoomSize;
        damping = reverbDamping;
        width = reverbWidth;
        level = reverbLevel;
    }

    void setChorus(int voiceCount, double level, double speed, double depth) {
        chorusVoices = voiceCount;
        chorusLevel = level;
        chorusSpeed = speed;
        chorusDepth = depth;
        if (synth) {
            fluid_synth_set_chorus(synth, chorusVoices, chorusLevel, chorusSpeed, chorusDepth, FLUID_CHORUS_MOD_SINE);
        }
    }

    void getChorus(int& voiceCount, double& level, double& speed, double& depth) {
        voiceCount = chorusVoices;
        level = chorusLevel;
        speed = chorusSpeed;
        depth = chorusDepth;
    }
}