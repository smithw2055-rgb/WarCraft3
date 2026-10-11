#include "audio/MidiSynth.h"

#include <algorithm>
#include <cmath>

#include "audio/MidiFile.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

inline double NoteToFreq(int note) {
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

// Deterministic noise (LCG): synth stays bit-stable, no <random> in audio.
struct NoiseGen {
    uint32_t s = 0x12345678u;
    float next() {
        s = s * 1664525u + 1013904223u;
        return (float)((int32_t)(s >> 9) & 0x7FFFFF) / (float)0x400000 - 1.0f;
    }
};

enum class Timbre : uint8_t {
    PIANO,   // 0-7, 104-111: plucky, fast decay
    BELL,    // 8-15 chromatic perc: inharmonic, long ring
    ORGAN,   // 16-23: hollow, full sustain
    PLUCK,   // 24-31 guitars, 96-103 synth fx: bright, medium decay
    BASS,    // 32-39: sine + octave, short
    STRINGS, // 40-55: slow attack, warm saw stack
    BRASS,   // 56-63: medium attack, bright
    FLUTE,   // 64-79 reed/pipe: sine + vibrato
    SAW,     // 80-87 synth lead
    PAD,     // 88-95 slow evolving detuned pair
    SFX,     // 112-127: breathy noise + tone
};

Timbre ProgramToTimbre(int program) {
    if (program <= 7) return Timbre::PIANO;
    if (program <= 15) return Timbre::BELL;
    if (program <= 23) return Timbre::ORGAN;
    if (program <= 31) return Timbre::PLUCK;
    if (program <= 39) return Timbre::BASS;
    if (program <= 55) return Timbre::STRINGS;
    if (program <= 63) return Timbre::BRASS;
    if (program <= 79) return Timbre::FLUTE;
    if (program <= 87) return Timbre::SAW;
    if (program <= 95) return Timbre::PAD;
    if (program <= 111) return Timbre::PLUCK;
    return Timbre::SFX;
}

struct Voice {
    bool active = false;
    bool released = false;
    bool pedaled = false; // sustain held past note-off
    int channel = 0;
    int note = 60;
    double freq = 440.0;
    double phase = 0.0;
    double phase2 = 0.0; // detune / second partial phase
    float vel = 1.0f;
    Timbre timbre = Timbre::PIANO;
    bool drum = false;
    int drumKind = 0; // 0 kick, 1 snare, 2 hat, 3 tom, 4 cymbal, 5 other
    double age = 0.0; // seconds since note-on
    double releaseAge = 0.0;
    float releaseLevel = 0.0f;
    float panL = 0.7071f, panR = 0.7071f;
    float chanGain = 1.0f;
    NoiseGen noise;
};

struct Envelope {
    float attack = 0.01f;
    float decay = 0.1f;
    float sustain = 0.8f;
    float release = 0.15f;
};

Envelope TimbreEnvelope(Timbre t) {
    switch (t) {
        case Timbre::PIANO: return {0.005f, 0.6f, 0.25f, 0.25f};
        case Timbre::BELL: return {0.005f, 1.5f, 0.35f, 1.2f};
        case Timbre::ORGAN: return {0.02f, 0.05f, 1.0f, 0.08f};
        case Timbre::PLUCK: return {0.003f, 0.25f, 0.4f, 0.12f};
        case Timbre::BASS: return {0.008f, 0.12f, 0.9f, 0.1f};
        case Timbre::STRINGS: return {0.35f, 0.3f, 0.9f, 0.4f};
        case Timbre::BRASS: return {0.09f, 0.15f, 0.85f, 0.15f};
        case Timbre::FLUTE: return {0.06f, 0.1f, 0.9f, 0.12f};
        case Timbre::SAW: return {0.02f, 0.1f, 0.8f, 0.15f};
        case Timbre::PAD: return {0.8f, 0.8f, 0.9f, 0.8f};
        case Timbre::SFX: return {0.05f, 0.4f, 0.5f, 0.4f};
    }
    return {0.01f, 0.1f, 0.8f, 0.2f};
}

// Begin release, capturing the live level first (computing with
// released=false; capturing after flagging would read zero).
float EnvelopeLevel(const Envelope& e, const Voice& v);
void StartRelease(Voice& v) {
    if (v.released) return;
    if (!v.drum) {
        Voice probe = v;
        probe.released = false;
        v.releaseLevel = EnvelopeLevel(TimbreEnvelope(v.timbre), probe);
    } else {
        v.releaseLevel = 1.0f;
    }
    v.released = true;
    v.releaseAge = 0.0;
}

float EnvelopeLevel(const Envelope& e, const Voice& v) {
    if (v.released) {
        if (e.release <= 0.0f) return 0.0f;
        float k = 1.0f - (float)(v.releaseAge / e.release);
        if (k <= 0.0f) return 0.0f;
        return v.releaseLevel * k * k;
    }
    if (v.age < e.attack) {
        return e.attack <= 0.0f ? 1.0f : (float)(v.age / e.attack);
    }
    const double d = v.age - e.attack;
    if (d < e.decay) {
        const float k = (float)(d / e.decay);
        return 1.0f - (1.0f - e.sustain) * k;
    }
    return e.sustain;
}

// Additive tone: sine fundamental + harmonics by timbre. Returns [-1, 1].
float ToneSample(const Voice& v, double freqMul = 1.0) {
    const double p = v.phase * freqMul;
    const double s1 = std::sin(2.0 * kPi * p);
    switch (v.timbre) {
        case Timbre::PIANO:
            return (float)(0.65 * s1 + 0.22 * std::sin(2.0 * kPi * p * 2.0) +
                            0.09 * std::sin(2.0 * kPi * p * 3.0) +
                            0.04 * std::sin(2.0 * kPi * p * 4.0));
        case Timbre::BELL:
            return (float)(0.6 * s1 + 0.25 * std::sin(2.0 * kPi * p * 2.76) +
                            0.15 * std::sin(2.0 * kPi * p * 5.4));
        case Timbre::ORGAN:
            return (float)(0.7 * s1 + 0.2 * std::sin(2.0 * kPi * p * 2.0) +
                            0.1 * std::sin(2.0 * kPi * p * 3.0));
        case Timbre::PLUCK:
            return (float)(0.55 * s1 + 0.25 * std::sin(2.0 * kPi * p * 2.0) +
                            0.12 * std::sin(2.0 * kPi * p * 3.0) +
                            0.08 * std::sin(2.0 * kPi * p * 4.0));
        case Timbre::BASS:
            return (float)(0.75 * s1 + 0.25 * std::sin(2.0 * kPi * p * 2.0));
        case Timbre::STRINGS:
        case Timbre::BRASS: {
            float s = 0.0f;
            float amp = 0.55f;
            for (int h = 1; h <= 8; ++h) {
                s += amp * (float)std::sin(2.0 * kPi * p * h);
                amp *= (v.timbre == Timbre::STRINGS ? 0.72f : 0.82f);
            }
            return s;
        }
        case Timbre::FLUTE: {
            const double vib = 1.0 + 0.004 * std::sin(2.0 * kPi * v.phase2 * 5.5);
            (void)vib;
            return (float)(0.85 * s1 + 0.15 * std::sin(2.0 * kPi * p * 2.0));
        }
        case Timbre::SAW: {
            float s = 0.0f;
            float amp = 0.5f;
            for (int h = 1; h <= 6; ++h) {
                s += amp * (float)std::sin(2.0 * kPi * p * h);
                amp *= 0.85f;
            }
            return s;
        }
        case Timbre::PAD: {
            const double det = 1.003;
            return (float)(0.4 * s1 + 0.3 * std::sin(2.0 * kPi * v.phase2) +
                            0.15 * std::sin(2.0 * kPi * p * 2.0) +
                            0.15 * std::sin(2.0 * kPi * v.phase2 * det * 2.0));
        }
        case Timbre::SFX:
            return (float)(0.5 * s1 + 0.3 * std::sin(2.0 * kPi * p * 1.5) +
                            0.2 * std::sin(2.0 * kPi * p * 2.02));
    }
    return (float)s1;
}

} // namespace

MidiPcm RenderMidiSong(const MidiSong& song, const MidiRenderOptions& opt) {
    MidiPcm pcm;
    pcm.sampleRate = opt.sampleRate > 0 ? opt.sampleRate : 22050;
    if (!song.valid || song.notes.empty()) return pcm;

    const double duration =
        std::min(song.durationSec + opt.releaseTailSec, opt.maxDurationSec);
    if (duration <= 0.0) return pcm;
    const size_t frames = (size_t)(duration * pcm.sampleRate);
    if (frames == 0) return pcm;
    pcm.samples.assign(frames * 2, 0.0f);

    struct Timed {
        double t;
        int order; // programs/controls before notes at same tick
        size_t index;
        int kind; // 0 program, 1 control, 2 note
    };
    std::vector<Timed> timeline;
    timeline.reserve(song.programs.size() + song.controls.size() + song.notes.size());
    for (size_t i = 0; i < song.programs.size(); ++i)
        timeline.push_back({song.programs[i].timeSec, 0, i, 0});
    for (size_t i = 0; i < song.controls.size(); ++i)
        timeline.push_back({song.controls[i].timeSec, 1, i, 1});
    for (size_t i = 0; i < song.notes.size(); ++i)
        timeline.push_back({song.notes[i].timeSec, 2, i, 2});
    std::sort(timeline.begin(), timeline.end(), [](const Timed& a, const Timed& b) {
        if (a.t != b.t) return a.t < b.t;
        return a.order < b.order;
    });

    int program[16] = {};
    float volume[16];
    float expression[16];
    float pan[16];
    bool pedal[16] = {};
    for (int c = 0; c < 16; ++c) {
        volume[c] = 100.0f / 127.0f;
        expression[c] = 1.0f;
        pan[c] = 0.5f;
    }

    constexpr int kMaxVoices = 64;
    Voice voices[kMaxVoices];
    size_t cursor = 0;
    const double dt = 1.0 / pcm.sampleRate;

    for (size_t f = 0; f < frames; ++f) {
        const double now = f * dt;
        while (cursor < timeline.size() && timeline[cursor].t <= now) {
            const Timed& te = timeline[cursor++];
            if (te.kind == 0) {
                const auto& p = song.programs[te.index];
                if (p.channel < 16) program[p.channel] = p.program;
            } else if (te.kind == 1) {
                const auto& c = song.controls[te.index];
                if (c.channel >= 16) continue;
                if (c.control == 7) volume[c.channel] = c.value / 127.0f;
                else if (c.control == 11) expression[c.channel] = c.value / 127.0f;
                else if (c.control == 10) pan[c.channel] = c.value / 127.0f;
                else if (c.control == 64) {
                    const bool down = c.value >= 64;
                    pedal[c.channel] = down;
                    if (!down) {
                        for (auto& v : voices) {
                            if (v.active && v.channel == c.channel && v.pedaled) {
                                v.pedaled = false;
                                StartRelease(v);
                            }
                        }
                    }
                }
            } else {
                const auto& n = song.notes[te.index];
                if (n.channel >= 16) continue;
                if (n.off || n.velocity == 0) {
                    for (auto& v : voices) {
                        if (!v.active || v.channel != n.channel || v.note != n.note) continue;
                        if (pedal[n.channel] && !v.drum) {
                            v.pedaled = true;
                        } else {
                            StartRelease(v);
                        }
                    }
                } else {
                    // Retrigger: release the old twin first (no clicks).
                    for (auto& v : voices) {
                        if (v.active && v.channel == n.channel && v.note == n.note && !v.released) {
                            StartRelease(v);
                        }
                    }
                    Voice* slot = nullptr;
                    for (auto& v : voices) {
                        if (!v.active) {
                            slot = &v;
                            break;
                        }
                    }
                    if (!slot) {
                        // Steal the oldest released voice, else the oldest.
                        for (auto& v : voices) {
                            if (v.released && (!slot || v.age > slot->age)) slot = &v;
                        }
                        if (!slot) {
                            slot = &voices[0];
                            for (auto& v : voices) {
                                if (v.age > slot->age) slot = &v;
                            }
                        }
                    }
                    Voice& v = *slot;
                    v = Voice{};
                    v.active = true;
                    v.channel = n.channel;
                    v.note = n.note;
                    v.vel = n.velocity / 127.0f;
                    v.drum = (n.channel == 9);
                    if (v.drum) {
                        if (n.note == 35 || n.note == 36) v.drumKind = 0;
                        else if (n.note == 38 || n.note == 40) v.drumKind = 1;
                        else if (n.note == 42 || n.note == 44 || n.note == 46) v.drumKind = 2;
                        else if (n.note >= 41 && n.note <= 47) v.drumKind = 3;
                        else if (n.note == 49 || n.note == 51 || n.note == 52 || n.note == 55 ||
                                 n.note == 57) v.drumKind = 4;
                        else v.drumKind = 5;
                        v.freq = NoteToFreq(n.note);
                    } else {
                        v.timbre = ProgramToTimbre(program[n.channel]);
                        v.freq = NoteToFreq(n.note);
                    }
                    const float g = volume[n.channel] * expression[n.channel];
                    v.chanGain = g;
                    const float a = pan[n.channel] * (float)kPi * 0.5f;
                    v.panL = std::cos(a);
                    v.panR = std::sin(a);
                    v.noise.s = 0x12345678u + (uint32_t)n.note * 0x9E3779B9u + (uint32_t)f;
                }
            }
        }

        float left = 0.0f, right = 0.0f;
        for (auto& v : voices) {
            if (!v.active) continue;
            v.age += dt;
            if (v.released) v.releaseAge += dt;
            float sample = 0.0f;
            if (v.drum) {
                const double t = v.age;
                switch (v.drumKind) {
                    case 0: { // kick: sine drop 120 -> 40 Hz
                        const double f = 40.0 + 80.0 * std::exp(-t * 25.0);
                        v.phase += f * dt;
                        sample = (float)(std::sin(2.0 * kPi * v.phase) * std::exp(-t * 9.0));
                        break;
                    }
                    case 1: // snare: tone + noise
                        v.phase += 180.0 * dt;
                        sample = (float)((0.5 * std::sin(2.0 * kPi * v.phase) + 0.5 * v.noise.next()) *
                                         std::exp(-t * 14.0));
                        break;
                    case 2: // hat: short high noise
                        sample = v.noise.next() * (float)std::exp(-t * 90.0) * 0.6f;
                        break;
                    case 3: { // tom: sine drop
                        const double f = 90.0 + 60.0 * std::exp(-t * 18.0);
                        v.phase += f * dt;
                        sample = (float)(std::sin(2.0 * kPi * v.phase) * std::exp(-t * 10.0));
                        break;
                    }
                    case 4: // cymbal: long bright noise
                        sample = v.noise.next() * (float)std::exp(-t * 3.0) * 0.4f;
                        break;
                    default:
                        sample = v.noise.next() * (float)std::exp(-t * 20.0) * 0.4f;
                        break;
                }
                if (t > 4.0) {
                    v.active = false;
                    continue;
                }
            } else {
                const Envelope env = TimbreEnvelope(v.timbre);
                const float e = EnvelopeLevel(env, v);
                if (e <= 0.0f) {
                    if (v.released) v.active = false;
                    continue;
                }
                double vibHz = 0.0;
                if (v.timbre == Timbre::FLUTE || v.timbre == Timbre::STRINGS) {
                    v.phase2 += dt;
                    vibHz = 5.5;
                    const double vibDepth = v.timbre == Timbre::FLUTE ? 0.004 : 0.002;
                    v.freq = NoteToFreq(v.note) * (1.0 + vibDepth * std::sin(2.0 * kPi * v.phase2 * vibHz));
                } else if (v.timbre == Timbre::PAD) {
                    v.phase2 += v.freq * 1.003 * dt;
                }
                v.phase += v.freq * dt;
                if (v.phase >= 1.0) v.phase -= std::floor(v.phase);
                if (v.phase2 >= 1.0 && v.timbre != Timbre::FLUTE && v.timbre != Timbre::STRINGS)
                    v.phase2 -= std::floor(v.phase2);
                sample = ToneSample(v) * e;
                // Bells/pads ring: hard-cap runaway sustains.
                if (v.age > 12.0 && !v.released) {
                    v.released = true;
                    v.releaseAge = 0.0;
                    v.releaseLevel = e;
                }
            }
            sample *= v.vel * v.chanGain;
            left += sample * v.panL;
            right += sample * v.panR;
        }
        pcm.samples[f * 2] = left * opt.masterGain;
        pcm.samples[f * 2 + 1] = right * opt.masterGain;
    }

    // Peak-normalize to 0.7 and apply 30 ms loop ramps (no loop click).
    float peak = 0.0f;
    for (float s : pcm.samples) peak = std::max(peak, std::fabs(s));
    if (peak > 0.0f) {
        const float g = 0.7f / peak;
        for (float& s : pcm.samples) s *= g;
    }
    const size_t ramp = std::min(frames, (size_t)(pcm.sampleRate * 0.03));
    for (size_t i = 0; i < ramp; ++i) {
        const float k = 0.5f - 0.5f * std::cos((float)kPi * i / (float)ramp);
        pcm.samples[i * 2] *= k;
        pcm.samples[i * 2 + 1] *= k;
        pcm.samples[(frames - 1 - i) * 2] *= k;
        pcm.samples[(frames - 1 - i) * 2 + 1] *= k;
    }
    return pcm;
}
