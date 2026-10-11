#pragma once
// Tiny General-MIDI renderer for ambient tracks (NOT a SoundFont).
//
// Blizzard's 27 ambient .mid files are sparse atmosphere (wind, birds,
// drones), not dense music. This renders them with small per-family
// recipes (additive harmonics + ADSR + vibrato, noise drums on ch 10)
// at 22050 Hz stereo: honest background texture, not authentic timbres.
// The reference engine plays no MIDI at all, so anything audible here
// is a strict improvement; a DLS-bank synth would be the real fix.
#include <cstdint>
#include <vector>

struct MidiSong;

// Rendered PCM: sampleRate Hz, interleaved stereo float [-1, 1].
struct MidiPcm {
    int sampleRate = 22050;
    std::vector<float> samples; // LRLR...
};

struct MidiRenderOptions {
    int sampleRate = 22050;
    float masterGain = 0.8f;
    float releaseTailSec = 2.0f; // ring-out after the last event
    double maxDurationSec = 300.0;
};

// Render the whole song (loops seamlessly-ish: 30 ms raised-cosine ramps
// at both ends so the hard loop cut does not click). Empty on bad input.
MidiPcm RenderMidiSong(const MidiSong& song, const MidiRenderOptions& opt = {});
