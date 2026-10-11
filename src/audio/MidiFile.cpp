#include "audio/MidiFile.h"

#include <algorithm>

namespace {

struct Reader {
    const uint8_t* p;
    size_t left;
    bool ok = true;
    uint8_t byte() {
        if (left == 0) { ok = false; return 0; }
        --left;
        return *p++;
    }
    uint32_t be32() {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) v = (v << 8) | byte();
        return v;
    }
    uint16_t be16() {
        uint16_t v = (uint16_t)byte() << 8;
        v |= byte();
        return v;
    }
    uint32_t varlen() {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) {
            uint8_t b = byte();
            v = (v << 7) | (b & 0x7F);
            if (!(b & 0x80)) return v;
        }
        ok = false;
        return v;
    }
    void skip(size_t n) {
        if (n > left) { ok = false; return; }
        p += n;
        left -= n;
    }
};

struct RawEvent {
    uint32_t tick = 0;
    uint8_t kind = 0; // 0=note,1=program,2=control,3=tempo
    uint8_t ch = 0;
    uint8_t a = 0;
    uint8_t b = 0;
    uint32_t tempo = 500000; // usec per quarter (kind 3)
};

} // namespace

bool ParseMidiFile(const uint8_t* data, size_t size, MidiSong& out) {
    out = MidiSong{};
    if (!data || size < 14) return false;
    Reader r{data, size};
    if (r.byte() != 'M' || r.byte() != 'T' || r.byte() != 'h' || r.byte() != 'd') return false;
    if (r.be32() != 6) return false;
    const uint16_t format = r.be16();
    if (format > 1) return false; // type 2 (async patterns) unsupported
    const uint16_t ntracks = r.be16();
    const uint16_t division = r.be16();
    if (!r.ok || ntracks == 0) return false;
    if (division & 0x8000) return false; // SMPTE timing unsupported
    const uint16_t ppq = division ? division : 480;

    std::vector<RawEvent> raw;
    for (uint16_t t = 0; t < ntracks && r.ok; ++t) {
        if (r.byte() != 'M' || r.byte() != 'T' || r.byte() != 'r' || r.byte() != 'k') {
            return false;
        }
        const uint32_t trackLen = r.be32();
        if (!r.ok || trackLen > r.left) return false;
        Reader tr{r.p, trackLen};
        r.skip(trackLen);
        uint32_t tick = 0;
        uint8_t running = 0;
        while (tr.left > 0 && tr.ok) {
            tick += tr.varlen();
            if (!tr.ok) break;
            uint8_t status = tr.byte();
            uint8_t data1 = 0;
            bool haveData1 = false;
            if (status < 0x80) {
                // Running status: the byte is the first data byte.
                if (running < 0x80 || running >= 0xF0) { tr.ok = false; break; }
                data1 = status;
                haveData1 = true;
                status = running;
            }
            if (status == 0xFF) {
                const uint8_t meta = tr.byte();
                const uint32_t len = tr.varlen();
                if (meta == 0x51 && len == 3) { // tempo
                    RawEvent e;
                    e.tick = tick;
                    e.kind = 3;
                    e.tempo = ((uint32_t)tr.byte() << 16) | ((uint32_t)tr.byte() << 8) | tr.byte();
                    raw.push_back(e);
                } else {
                    tr.skip(len);
                }
                running = 0;
            } else if (status == 0xF0 || status == 0xF7) {
                const uint32_t len = tr.varlen();
                tr.skip(len);
                running = 0;
            } else if (status >= 0x80 && status < 0xF0) {
                running = status;
                const uint8_t hi = status & 0xF0;
                const uint8_t ch = status & 0x0F;
                auto dataByte = [&]() -> uint8_t {
                    if (haveData1) {
                        haveData1 = false;
                        return data1;
                    }
                    return tr.byte();
                };
                if (hi == 0xC0 || hi == 0xD0) {
                    const uint8_t prog = dataByte();
                    if (hi == 0xC0) {
                        RawEvent e;
                        e.tick = tick;
                        e.kind = 1;
                        e.ch = ch;
                        e.a = prog;
                        raw.push_back(e);
                    }
                } else {
                    const uint8_t d1 = dataByte();
                    const uint8_t d2 = tr.byte();
                    if (hi == 0x90) {
                        RawEvent e;
                        e.tick = tick;
                        e.kind = 0;
                        e.ch = ch;
                        e.a = d1;
                        e.b = d2;
                        raw.push_back(e);
                    } else if (hi == 0x80) {
                        RawEvent e;
                        e.tick = tick;
                        e.kind = 0;
                        e.ch = ch;
                        e.a = d1;
                        e.b = 0;
                        raw.push_back(e);
                    } else if (hi == 0xB0) {
                        if (d1 == 7 || d1 == 10 || d1 == 11 || d1 == 64) {
                            RawEvent e;
                            e.tick = tick;
                            e.kind = 2;
                            e.ch = ch;
                            e.a = d1;
                            e.b = d2 > 127 ? 127 : d2;
                            raw.push_back(e);
                        }
                    }
                    // Pitch bend (0xE0), aftertouch (0xA0), pressure: skipped.
                }
            } else {
                tr.ok = false; // unknown system message
            }
        }
        if (!tr.ok) return false;
    }
    if (!r.ok || raw.empty()) return false;

    // Ticks -> seconds through the merged tempo map (default 120 BPM).
    std::vector<std::pair<uint32_t, uint32_t>> tempoMap; // tick -> usec/quarter
    tempoMap.emplace_back(0, 500000);
    for (const auto& e : raw) {
        if (e.kind == 3) tempoMap.emplace_back(e.tick, e.tempo ? e.tempo : 500000);
    }
    std::sort(tempoMap.begin(), tempoMap.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    auto tickToSec = [&](uint32_t tick) {
        double sec = 0.0;
        uint32_t lastTick = 0;
        uint32_t usec = 500000;
        for (const auto& [tt, tu] : tempoMap) {
            if (tt > tick) break;
            sec += (double)(tt - lastTick) * usec / 1e6 / ppq;
            lastTick = tt;
            usec = tu;
        }
        sec += (double)(tick - lastTick) * usec / 1e6 / ppq;
        return sec;
    };

    for (const auto& e : raw) {
        const double t = tickToSec(e.tick);
        if (e.kind == 0) {
            MidiNoteEvent n;
            n.timeSec = t;
            n.channel = e.ch;
            n.note = e.a > 127 ? 127 : e.a;
            n.velocity = e.b > 127 ? 127 : e.b;
            n.off = (n.velocity == 0);
            out.notes.push_back(n);
        } else if (e.kind == 1) {
            MidiProgramEvent p;
            p.timeSec = t;
            p.channel = e.ch;
            p.program = e.a > 127 ? 127 : e.a;
            out.programs.push_back(p);
        } else if (e.kind == 2) {
            MidiControlEvent c;
            c.timeSec = t;
            c.channel = e.ch;
            c.control = e.a;
            c.value = e.b;
            out.controls.push_back(c);
        }
    }
    if (out.notes.empty() && out.programs.empty() && out.controls.empty()) return false;
    double last = 0.0;
    for (const auto& n : out.notes) last = std::max(last, n.timeSec);
    out.durationSec = last;
    out.valid = true;
    return true;
}
