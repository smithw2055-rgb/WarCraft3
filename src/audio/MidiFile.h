#pragma once
// Minimal Standard MIDI File reader for ambient playback.
//
// Covers what Blizzard's ambient tracks use: type 0/1, PPQ division,
// running status, note on/off (vel 0 = off), program change, CC 7/10/11/64,
// tempo map, end-of-track. Skips sysex, pitch bend, aftertouch and SMPTE
// division (rejected at load). Never throws; malformed files fail clean.
#include <cstdint>
#include <string>
#include <vector>

struct MidiNoteEvent {
    double timeSec = 0.0; // tempo-mapped
    uint8_t channel = 0;  // 0..15
    uint8_t note = 60;
    uint8_t velocity = 0; // 0 = note off
    bool off = false;
};

struct MidiProgramEvent {
    double timeSec = 0.0;
    uint8_t channel = 0;
    uint8_t program = 0; // GM 0..127
};

struct MidiControlEvent {
    double timeSec = 0.0;
    uint8_t channel = 0;
    uint8_t control = 0; // 7 volume, 10 pan, 11 expression, 64 sustain
    uint8_t value = 0;
};

struct MidiSong {
    bool valid = false;
    double durationSec = 0.0; // last event time (synth adds release tail)
    std::vector<MidiNoteEvent> notes;
    std::vector<MidiProgramEvent> programs;
    std::vector<MidiControlEvent> controls;
};

// Parse an in-memory .mid file. False on bad magic, SMPTE division,
// truncation, or empty event stream (header-only files).
bool ParseMidiFile(const uint8_t* data, size_t size, MidiSong& out);
