#include "recording.h"
#include "midi_file_io.h"
#include <fstream>
#include <iostream>
#include <cstring>

namespace {
    bool recordingActive = false;
    double elapsedTime = 0.0;
    std::vector<TimedMidiEvent> events;

    // Tiny custom format, not a real Standard MIDI File:
    //   [4 bytes]  magic  "PVRC"
    //   [uint32]   version (currently 1)
    //   [uint32]   event count
    //   [event count * (double + 3 bytes)]  timestamp, status, note, velocity
    constexpr char kMagic[4] = { 'P', 'V', 'R', 'C' };
    constexpr uint32_t kVersion = 1;
}

namespace recording {

    void start() {
        events.clear();
        elapsedTime = 0.0;
        recordingActive = true;
        std::cout << "[Recording] Started." << std::endl;
    }

    void stop() {
        recordingActive = false;
        std::cout << "[Recording] Stopped. Captured " << events.size()
                   << " events over " << elapsedTime << "s." << std::endl;
    }

    void toggle() {
        if (recordingActive) stop();
        else start();
    }

    bool isRecording() {
        return recordingActive;
    }

    void update(float dt) {
        if (recordingActive) {
            elapsedTime += dt;
        }
    }

    void onEvent(const MidiEvent& ev) {
        if (!recordingActive) return;
        events.push_back(TimedMidiEvent{ elapsedTime, ev.status, ev.note, ev.velocity });
    }

    double getElapsedTime() {
        return elapsedTime;
    }

    size_t getEventCount() {
        return events.size();
    }

    const std::vector<TimedMidiEvent>& getEvents() {
        return events;
    }

    void clear() {
        events.clear();
        elapsedTime = 0.0;
    }

    bool saveToFile(const std::string& path) {
        std::ofstream out(path, std::ios::binary);
        if (!out) {
            std::cerr << "[Recording] Failed to open file for writing: " << path << std::endl;
            return false;
        }

        out.write(kMagic, sizeof(kMagic));
        out.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));

        uint32_t count = static_cast<uint32_t>(events.size());
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));

        for (const auto& ev : events) {
            out.write(reinterpret_cast<const char*>(&ev.timestamp), sizeof(ev.timestamp));
            out.write(reinterpret_cast<const char*>(&ev.status), sizeof(ev.status));
            out.write(reinterpret_cast<const char*>(&ev.note), sizeof(ev.note));
            out.write(reinterpret_cast<const char*>(&ev.velocity), sizeof(ev.velocity));
        }

        if (!out) {
            std::cerr << "[Recording] Write error while saving: " << path << std::endl;
            return false;
        }

        std::cout << "[Recording] Saved " << count << " events to " << path << std::endl;
        return true;
    }

    bool saveToMidFile(const std::string& path) {
        if (!midi_file_io::saveMidFile(path, events)) {
            std::cerr << "[Recording] Failed to export MIDI file: " << path << std::endl;
            return false;
        }
        std::cout << "[Recording] Exported " << events.size() << " events to " << path << std::endl;
        return true;
    }

    bool loadFromFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            std::cerr << "[Recording] Failed to open file for reading: " << path << std::endl;
            return false;
        }

        char magic[4];
        in.read(magic, sizeof(magic));
        if (!in || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
            std::cerr << "[Recording] Not a valid recording file: " << path << std::endl;
            return false;
        }

        uint32_t version = 0;
        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != kVersion) {
            std::cerr << "[Recording] Unsupported recording version: " << version << std::endl;
            return false;
        }

        uint32_t count = 0;
        in.read(reinterpret_cast<char*>(&count), sizeof(count));

        std::vector<TimedMidiEvent> loaded;
        loaded.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            TimedMidiEvent ev{};
            in.read(reinterpret_cast<char*>(&ev.timestamp), sizeof(ev.timestamp));
            in.read(reinterpret_cast<char*>(&ev.status), sizeof(ev.status));
            in.read(reinterpret_cast<char*>(&ev.note), sizeof(ev.note));
            in.read(reinterpret_cast<char*>(&ev.velocity), sizeof(ev.velocity));

            if (!in) {
                std::cerr << "[Recording] Unexpected end of file while reading: " << path << std::endl;
                return false;
            }
            loaded.push_back(ev);
        }

        events = std::move(loaded);
        elapsedTime = 0.0;
        recordingActive = false;

        std::cout << "[Recording] Loaded " << events.size() << " events from " << path << std::endl;
        return true;
    }
}