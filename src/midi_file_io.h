#pragma once
#include "recording.h" // for the shared TimedMidiEvent struct
#include <string>
#include <vector>

// Conversion layer between our in-memory event format (TimedMidiEvent) and
// real Standard MIDI Files (.mid), using the midifile library (smf::MidiFile).
//
// This is deliberately just a converter, not a replacement for the .pvrc
// format: recording.cpp/playback.cpp keep reading/writing .pvrc for normal
// use. Reach for this when you want to:
//   - export a recording as a .mid so it opens in a DAW, notation software,
//     another visualiser, etc.
//   - import someone else's .mid file and play it back through this app.
namespace midi_file_io {

    // Reads a .mid file, merges all tracks into chronological order, and
    // converts tick-based timing into absolute seconds using the file's
    // tempo map. Non-channel messages (meta events, sysex) are dropped -
    // this app only cares about note on/off and control-change messages.
    // Returns false if the file can't be opened or isn't a valid MIDI file.
    bool loadMidFile(const std::string& path, std::vector<TimedMidiEvent>& outEvents);

    // Writes events out as a Standard MIDI File. Our recordings only carry
    // absolute seconds (no tempo info), so this picks a fixed 120bpm/480
    // ticks-per-quarter grid and converts seconds -> ticks against that.
    // The exported file will always report 120bpm, but the note timing
    // (in real seconds) is preserved exactly.
    bool saveMidFile(const std::string& path, const std::vector<TimedMidiEvent>& events);

}