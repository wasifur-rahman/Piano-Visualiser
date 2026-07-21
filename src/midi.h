#pragma once
#include <rtmidi/RtMidi.h>
#include <mutex>
#include <queue>
#include <memory>

struct MidiEvent {
    unsigned char status;
    unsigned char note;
    unsigned char velocity;
};

extern std::mutex midiMutex;
extern std::queue<MidiEvent> midiQueue;
extern std::unique_ptr<RtMidiIn> midiIn;
extern bool midiInInitialized;
extern float midiCheckTimer; // timer for checking MIDI device connection
extern const float midiCheckInterval; // seconds between MIDI device checks

void attemptConnection();
void midiCallback(double deltatime, std::vector<unsigned char>* message, void* userData);