#include "playback.h"
#include "midi_file_io.h"
#include "audio.h"
#include "visuals/blocks.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <array>
#include <unordered_set>

namespace {
    std::vector<TimedMidiEvent> events;
    size_t nextEventIndex = 0;
    double playheadTime = 0.0;
    bool playing = false;
    float speedMultiplier = 1.0f;

    // Notes playback has turned on that haven't been turned off yet. Lets
    // pause()/stop() release everything cleanly instead of leaving a note
    // stuck sounding/lit if you pause mid-note.
    std::unordered_set<unsigned char> activeNotes;

    void dispatch(const TimedMidiEvent& ev) {
        unsigned char statusType = ev.status & 0xF0;
        bool isNoteOn = (statusType == 0x90 && ev.velocity > 0);
        bool isNoteOff = (statusType == 0x80 || (statusType == 0x90 && ev.velocity == 0));

        if (isNoteOn) {
            audio::noteOn(ev.note, ev.velocity);
            blocks::onNoteOn(ev.note, ev.velocity);
            activeNotes.insert(ev.note);
        }
        else if (isNoteOff) {
            audio::noteOff(ev.note);
            blocks::onNoteOff(ev.note);
            activeNotes.erase(ev.note);
        }
        else if (statusType == 0xB0) {
            audio::controlChange(ev.note, ev.velocity);
        }
    }

    void releaseAllActiveNotes() {
        for (unsigned char note : activeNotes) {
            audio::noteOff(note);
            blocks::onNoteOff(note);
        }
        activeNotes.clear();
    }

    // Pairs up note-on/note-off events per pitch into NoteSpans so blocks'
    // reverse (falling) mode knows each note's full on->off duration ahead
    // of time. Assumes events are already timestamp-sorted (loadFromEvents
    // guarantees this before calling here).
    std::vector<NoteSpan> buildSchedule(const std::vector<TimedMidiEvent>& evs) {
        std::vector<NoteSpan> spans;
        std::array<double, 128> pendingOnTime{};
        std::array<int, 128> pendingVelocity{};
        pendingOnTime.fill(-1.0);

        for (const auto& ev : evs) {
            if (ev.note >= 128) continue;

            unsigned char statusType = ev.status & 0xF0;
            bool isNoteOn = (statusType == 0x90 && ev.velocity > 0);
            bool isNoteOff = (statusType == 0x80 || (statusType == 0x90 && ev.velocity == 0));

            if (isNoteOn) {
                pendingOnTime[ev.note] = ev.timestamp;
                pendingVelocity[ev.note] = ev.velocity;
            }
            else if (isNoteOff && pendingOnTime[ev.note] >= 0.0) {
                spans.push_back({ (int)ev.note, pendingVelocity[ev.note], pendingOnTime[ev.note], ev.timestamp });
                pendingOnTime[ev.note] = -1.0;
            }
        }

        return spans;
    }
}

namespace playback {

    bool loadFromFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            std::cerr << "[Playback] Failed to open file: " << path << std::endl;
            return false;
        }

        char magic[4];
        in.read(magic, sizeof(magic));
        if (!in || std::memcmp(magic, "PVRC", 4) != 0) {
            std::cerr << "[Playback] Not a valid recording file: " << path << std::endl;
            return false;
        }

        uint32_t version = 0;
        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != 1) {
            std::cerr << "[Playback] Unsupported recording version: " << version << std::endl;
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
                std::cerr << "[Playback] Unexpected end of file: " << path << std::endl;
                return false;
            }
            loaded.push_back(ev);
        }

        loadFromEvents(std::move(loaded));
        std::cout << "[Playback] Loaded " << events.size() << " events from " << path << std::endl;
        return true;
    }

    bool loadFromMidFile(const std::string& path) {
        std::vector<TimedMidiEvent> loaded;
        if (!midi_file_io::loadMidFile(path, loaded)) {
            std::cerr << "[Playback] Failed to load MIDI file: " << path << std::endl;
            return false;
        }

        size_t count = loaded.size();
        loadFromEvents(std::move(loaded)); // handles stop() + sort + blocks::setSchedule
        std::cout << "[Playback] Loaded " << count << " events from " << path << std::endl;
        return true;
    }

    void loadFromEvents(std::vector<TimedMidiEvent> newEvents) {
        stop(); // reset any in-progress playback before swapping the data out
        events = std::move(newEvents);

        // Recordings should already be in timestamp order, but this guards
        // against anything hand-edited or generated out of order.
        std::sort(events.begin(), events.end(),
            [](const TimedMidiEvent& a, const TimedMidiEvent& b) {
                return a.timestamp < b.timestamp;
            });

        nextEventIndex = 0;
        playheadTime = 0.0;

        // Hand blocks the full schedule so reverse (falling-block) mode
        // can show upcoming notes ahead of when they actually play.
        blocks::setSchedule(buildSchedule(events));
    }

    void play() {
        if (events.empty()) {
            std::cout << "[Playback] No recording loaded." << std::endl;
            return;
        }
        playing = true;
    }

    void pause() {
        playing = false;
        releaseAllActiveNotes();
    }

    void stop() {
        playing = false;
        releaseAllActiveNotes();
        playheadTime = 0.0;
        nextEventIndex = 0;
    }

    void togglePlayback() {
        if (playing) pause();
        else play();
    }

    bool isPlaying() {
        return playing;
    }

    bool hasLoadedRecording() {
        return !events.empty();
    }

    // Jumps the playhead to an arbitrary point (scrub bar / skip buttons).
    // Notes currently sounding are released rather than "fast-forwarded
    // through", since silently replaying a whole span of history's audio
    // would be surprising - the visuals/audio just pick back up cleanly
    // from wherever we land.
    void seek(double time) {
        if (events.empty()) return;

        double duration = getDuration();
        time = std::clamp(time, 0.0, duration);

        releaseAllActiveNotes();
        playheadTime = time;

        // First event strictly after the target time is where playback
        // resumes dispatching from - works identically whether we jumped
        // forward or backward.
        auto it = std::upper_bound(events.begin(), events.end(), time,
            [](double t, const TimedMidiEvent& ev) { return t < ev.timestamp; });
        nextEventIndex = (size_t)(it - events.begin());
    }

    void skip(double deltaSeconds) {
        seek(playheadTime + deltaSeconds);
    }

    void setSpeed(float multiplier) {
        speedMultiplier = std::clamp(multiplier, 0.1f, 4.0f);
    }

    float getSpeed() {
        return speedMultiplier;
    }

    void update(float dt) {
        if (!playing) return;

        playheadTime += dt * speedMultiplier;

        while (nextEventIndex < events.size() && events[nextEventIndex].timestamp <= playheadTime) {
            dispatch(events[nextEventIndex]);
            ++nextEventIndex;
        }

        if (nextEventIndex >= events.size()) {
            // Reached the end - stop cleanly (also releases any held notes,
            // though there shouldn't be any left if the recording ended
            // with matching note-offs).
            stop();
            std::cout << "[Playback] Finished." << std::endl;
        }
    }

    double getPlaybackTime() {
        return playheadTime;
    }

    double getDuration() {
        if (events.empty()) return 0.0;
        return events.back().timestamp;
    }

    size_t getTotalEventCount() {
        return events.size();
    }

    size_t getEventsPlayedCount() {
        return nextEventIndex;
    }
}