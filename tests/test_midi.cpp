// MIDI ambient tests: synthetic SMF parse, tiny synth render shape, and
// a real retail ambient track (MPQ-gated) proving audibility.
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

#include "audio/MidiFile.h"
#include "audio/MidiSynth.h"
#include "resource/mpq_storm.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace fs = std::filesystem;

namespace {

std::vector<uint8_t> SynthTestSong() {
    // Format 0, 1 track, 96 PPQ: tempo 120, program 64 (flute), quarter
    // note 60 at velocity 100, then end of track.
    std::vector<uint8_t> out = {
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96,
        'M', 'T', 'r', 'k', 0, 0, 0, 22,
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,
        0x00, 0xC0, 0x40,
        0x00, 0x90, 0x3C, 0x64,
        0x60, 0x80, 0x3C, 0x40,
        0x00, 0xFF, 0x2F, 0x00,
    };
    return out;
}

} // namespace

bool TestMidiParsesNotes() {
    auto bytes = SynthTestSong();
    MidiSong song;
    TEST_ASSERT(ParseMidiFile(bytes.data(), bytes.size(), song));
    TEST_ASSERT(song.valid);
    TEST_ASSERT(song.notes.size() == 2); // on + off
    TEST_ASSERT(!song.notes[0].off && song.notes[0].note == 60);
    TEST_ASSERT(song.notes[0].velocity == 100 && song.notes[0].channel == 0);
    TEST_ASSERT(song.notes[1].off);
    // 96 ticks at 120 BPM = 0.5 s.
    TEST_ASSERT(std::fabs(song.notes[1].timeSec - 0.5) < 0.001);
    TEST_ASSERT(std::fabs(song.durationSec - 0.5) < 0.001);
    TEST_ASSERT(song.programs.size() == 1 && song.programs[0].program == 64);
    // Garbage fails clean, never throws.
    MidiSong bad;
    TEST_ASSERT(!ParseMidiFile((const uint8_t*)"NOPE", 4, bad));
    TEST_ASSERT(!ParseMidiFile(nullptr, 0, bad));
    std::vector<uint8_t> truncated = bytes;
    truncated.resize(20);
    TEST_ASSERT(!ParseMidiFile(truncated.data(), truncated.size(), bad));
    return true;
}

bool TestMidiRendersAudibleLoop() {
    auto bytes = SynthTestSong();
    MidiSong song;
    TEST_ASSERT(ParseMidiFile(bytes.data(), bytes.size(), song));
    MidiRenderOptions opt;
    opt.releaseTailSec = 0.2;
    MidiPcm pcm = RenderMidiSong(song, opt);
    TEST_ASSERT(pcm.sampleRate == 22050);
    // Note 0.2f: releaseTailSec is float; match the callee's rounding.
    TEST_ASSERT(pcm.samples.size() == (size_t)((0.5 + 0.2f) * 22050) * 2);
    float peak = 0.0f;
    double energy = 0.0;
    for (float s : pcm.samples) {
        peak = std::max(peak, std::fabs(s));
        energy += s * s;
    }
    TEST_ASSERT(peak > 0.05f && peak <= 1.0f);
    TEST_ASSERT(energy / pcm.samples.size() > 1e-6);
    // Loop ramps: first and last frames near silence (no loop click).
    TEST_ASSERT(std::fabs(pcm.samples[0]) < 0.01f);
    TEST_ASSERT(std::fabs(pcm.samples[pcm.samples.size() - 1]) < 0.05f);
    // Empty song renders empty.
    MidiSong empty;
    MidiPcm silent = RenderMidiSong(empty, opt);
    TEST_ASSERT(silent.samples.empty());
    return true;
}

bool TestMidiRetailAmbientRenders() {
    const fs::path mpqPath = fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3.mpq";
    if (!fs::exists(mpqPath)) return true;
    MpqManager mpq;
    if (!mpq.Open(mpqPath.string())) return true;
    std::vector<uint8_t> bytes;
    if (!mpq.ReadFile("Sound\\Ambient\\LordaeronSummer\\LordaeronSummerDay.mid", bytes)) {
        return true; // path layout differs; nothing to prove
    }
    TEST_ASSERT(bytes.size() > 1000);
    MidiSong song;
    TEST_ASSERT(ParseMidiFile(bytes.data(), bytes.size(), song));
    TEST_ASSERT(song.valid && !song.notes.empty());
    TEST_ASSERT(song.durationSec > 5.0);
    MidiRenderOptions opt;
    opt.releaseTailSec = 0.5;
    MidiPcm pcm = RenderMidiSong(song, opt);
    float peak = 0.0f;
    for (float s : pcm.samples) peak = std::max(peak, std::fabs(s));
    std::cout << "Retail ambient: notes=" << song.notes.size()
              << " duration=" << song.durationSec << "s peak=" << peak << std::endl;
    TEST_ASSERT(peak > 0.01f && peak <= 1.0f);
    return true;
}
