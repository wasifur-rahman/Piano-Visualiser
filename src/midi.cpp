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
    midiInInitialized = false;
    midiIn.reset();

    try {
        auto newMidiIn = std::make_unique<RtMidiIn>();
        const unsigned int portCount = newMidiIn->getPortCount();

        if (portCount == 0) {
            std::cout << "[MIDI] No input ports detected." << std::endl;
            return;
        }

        for (unsigned int portIndex = 0; portIndex < portCount; ++portIndex) {
            try {
                newMidiIn->openPort(portIndex);
                newMidiIn->setCallback(&midiCallback);
                newMidiIn->ignoreTypes(false, false, false);

                midiIn = std::move(newMidiIn);
                midiInInitialized = true;
                std::cout << "[MIDI] Connected to input port " << portIndex << std::endl;
                return;
            }
            catch (const RtMidiError& e) {
                std::cerr << "[MIDI] Failed to open port " << portIndex << ": " << e.getMessage() << std::endl;
            }
        }

        std::cout << "[MIDI] No input ports could be opened." << std::endl;
    }
    catch (const RtMidiError& e) {
        std::cerr << "[MIDI] initialization error: " << e.getMessage() << std::endl;
    }
}