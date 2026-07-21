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

        // Built-in reverb/chorus - this is your "tone shaping" starting point
        fluid_synth_set_reverb(synth, /*roomsize*/10.0, /*damping*/0.0, /*width*/5.0, /*level*/0.0);
        fluid_synth_set_chorus(synth, /*nr*/3, /*level*/1.2, /*speed*/0.3, /*depth*/8.0, FLUID_CHORUS_MOD_SINE);
        fluid_synth_set_gain(synth, 3.0f); // overall volume
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

}