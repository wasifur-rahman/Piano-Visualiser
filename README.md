# PianoViz — starter skeleton

MIDI in -> FluidSynth audio out -> raylib rising blocks. No Ableton, no loopMIDI —
this one app owns the piano's MIDI port directly and produces its own sound.

## Setup (Windows, using vcpkg)

1. Install vcpkg if you don't have it: https://github.com/microsoft/vcpkg
2. Install dependencies:
   ```
   vcpkg install rtmidi raylib fluidsynth
   ```
3. Configure and build:
   ```
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake
   cmake --build build
   ```
4. Download a SoundFont. For a genuinely good free piano:
   - **Salamander Grand Piano** (SFZ format, converted SF2 versions exist online — search
     "Salamander Grand Piano SF2"). This is the same sample library many hobbyist and pro
     projects use because it sounds close to a real recorded piano, not a cheap synth patch.
   - Place the `.sf2` file at `assets/SalamanderGrandPiano.sf2` (or update `SOUNDFONT_PATH`
     in `main.cpp`).
5. Run `build/piano_viz.exe`.

## Why this stack gets you "Ableton-like" sound

FluidSynth doesn't synthesize piano tones algorithmically — it plays back real recorded
piano samples from the SoundFont, the same fundamental approach every serious DAW/sample
library uses. Quality comes almost entirely from the sample library (Salamander is a strong
free choice), not from FluidSynth itself. It also gives you, for free:
- Low-latency native audio output (WASAPI on Windows)
- Built-in reverb and chorus DSP (already wired up in `main.cpp`)
- Multi-timbral support (many instruments/channels at once)
- Per-channel program (instrument) switching

## Where your requested features fit in

**Customisation (blocks/background)**
- `colorForPitch()` and the `ClearBackground()` call are your hooks. Move these into a
  `Theme` struct with a few presets, and add a settings menu (raylib has simple
  immediate-mode UI, or use `raygui`, an official raylib companion library for buttons/
  sliders) to switch between them at runtime.

**Playback / instrument switching**
- `fluid_synth_program_select()` picks which instrument (preset) plays on a channel.
  Load a multi-instrument SoundFont (e.g. FluidR3_GM.sf2 for General MIDI variety, alongside
  Salamander for piano specifically) and let the user pick a preset index in a menu.
- `fluid_synth_sfload()` can load multiple SoundFonts simultaneously — assign different
  instruments to different MIDI channels if you want layering.

**Instrument settings (reverb, volume, tone, attack, etc.)**
- Reverb/chorus: already exposed via `fluid_synth_set_reverb()` / `fluid_synth_set_chorus()`
  — wire sliders to these params.
- Volume: `fluid_synth_set_gain()` for master, or per-channel via MIDI CC7 (volume) messages
  sent to `fluid_synth_cc()`.
- Attack/brightness/tone: these aren't simple FluidSynth API calls — they're properties
  baked into the SoundFont's envelope/filter settings per-instrument. To adjust them live
  you'd either (a) pick between multiple pre-made SoundFonts/presets with different
  characters, or (b) get into SoundFont generator parameters via
  `fluid_synth_set_gen()` (e.g. `GEN_ATTENUATION`, `GEN_FILTERFC` for brightness,
  `GEN_VOLENVATTACK` for attack time) — more advanced, but doable once the basics work.

**Recording**
- Simplest version: log every `MidiEvent` with a timestamp (use `GetTime()` from raylib)
  into a vector as you already receive them in the drain loop.
- Export as a real `.mid` file afterward — this needs a small MIDI file writer (Standard
  MIDI File format is a well-documented binary format; a minimal writer for note on/off
  events is a few hundred lines, or use a small library rather than hand-rolling the
  variable-length-quantity encoding).
- Alternative/simpler first step: record the *audio output* instead of MIDI, using
  FluidSynth's file renderer, but that's less flexible than having a MIDI file (you can't
  legally re-edit audio playback as easily as you can edit a MIDI recording, but you can
  read a MIDI file back into `music21` later for your sheet-music project).

## Known rough edges in this skeleton (intentional, for you to build out)

- MIDI port is hardcoded to index 0 — add a device-picker before opening.
- No sustain pedal (CC64) handling yet — worth adding since you mentioned it earlier;
  hook into `fluid_synth_cc()` for CC64 and adjust block-freezing logic in the drain loop.
- No error handling if the SoundFont fails to load - it'll run silently.
- Single hardcoded instrument/channel - fine for MVP, needs generalizing for instrument
  switching.
