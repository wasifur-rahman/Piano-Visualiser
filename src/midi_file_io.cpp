#include "midi_file_io.h"
#include <MidiFile.h>
#include <cmath>

namespace midi_file_io {

// Reading: merge all tracks, convert ticks -> seconds, drop meta/sysex
bool loadMidFile(const std::string& path, std::vector<TimedMidiEvent>& outEvents) {
    smf::MidiFile midifile;
    if (!midifile.read(path)) return false;
    if (!midifile.status()) return false;

    midifile.doTimeAnalysis();  // populates .seconds on every event using any tempo meta-messages
    midifile.joinTracks();      // merge all tracks into track 0, in chronological order

    outEvents.clear();
    for (int i = 0; i < midifile[0].getEventCount(); ++i) {
        auto& ev = midifile[0][i];
        if (ev.size() < 1 || ev[0] >= 0xF0) continue; // skip meta/sysex - not a channel message
        outEvents.push_back(TimedMidiEvent{
            ev.seconds,
            ev[0],
            ev.size() > 1 ? ev[1] : (unsigned char)0,
            ev.size() > 2 ? ev[2] : (unsigned char)0
        });
    }
    return true;
}

// Writing: your recordings are absolute seconds, so pick a fixed tempo and
// convert to ticks. 480 ticks/quarter and 120bpm are just conventional defaults.
bool saveMidFile(const std::string& path, const std::vector<TimedMidiEvent>& events) {
    constexpr int ticksPerQuarter = 480;
    constexpr double bpm = 120.0;
    constexpr double ticksPerSecond = ticksPerQuarter * (bpm / 60.0);

    smf::MidiFile midifile;
    midifile.setTicksPerQuarterNote(ticksPerQuarter);
    midifile.addTempo(0, 0, bpm);

    for (const auto& ev : events) {
        int tick = (int)std::round(ev.timestamp * ticksPerSecond);
        std::vector<smf::uchar> message = { ev.status, ev.note, ev.velocity };
        midifile.addEvent(0, tick, message);
    }

    midifile.sortTracks();
    return midifile.write(path);
}

}