#include "midi.h"
#include <iostream>

std::mutex midiMutex;
std::queue<MidiEvent> midiQueue;
std::unique_ptr<RtMidiIn> midiIn;
bool midiInInitialized = false;
float midiCheckTimer = 0.0f;
const float midiCheckInterval = 0.5f;

void midiCallback(double /*deltatime*/, std::vector<unsigned char>* message, void* /*userData*/) {
    if (!message || message->size() < 3) return;
    MidiEvent ev{ (*message)[0], (*message)[1], (*message)[2] };
    std::lock_guard<std::mutex> lock(midiMutex);
    midiQueue.push(ev);
}

void attemptConnection() {
    try {
        midiIn = std::make_unique<RtMidiIn>();
        unsigned int nPorts = midiIn->getPortCount();
        if (nPorts > 0) {
            midiIn->openPort(0);
            midiIn->setCallback(&midiCallback);
            midiIn->ignoreTypes(false, false, false);
            midiInInitialized = true;
        }
        else {
            midiInInitialized = false;
        }
    }
    catch (RtMidiError& e) {
        std::cerr << "MIDI initialization error: " << e.getMessage() << std::endl;
        midiInInitialized = false;
    }
}