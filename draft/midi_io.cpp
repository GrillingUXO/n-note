// midi_io.cpp
#include "midi_io.h"
#include "MidiFile.h"
#include <stdexcept>
#include <algorithm>  // 用于排序Tempo事件

using namespace smf;

namespace MIDIIO {
    std::vector<NoteEvent> parse_midi(const std::string& path) {
        MidiFile midi;
        if (!midi.read(path)) {
            throw std::runtime_error("Failed to read MIDI file: " + path);
        }
        midi.doTimeAnalysis();
        midi.linkNotePairs();

        // 1. 提取所有Tempo事件（时间点+BPM）
        std::vector<std::pair<double, double>> tempo_events; // <时间(秒), BPM>
        for (int track = 0; track < midi.getNumTracks(); ++track) {
            for (int event = 0; event < midi[track].size(); ++event) {
                auto& e = midi[track][event];
                if (e.isMeta() && e.getMetaType() == 0x51) { // Set Tempo事件
                    int micros_per_quarter = e.getTempoMicroseconds();
                    double bpm = 60000000.0 / micros_per_quarter;
                    tempo_events.emplace_back(e.seconds, bpm);
                }
            }
        }

        // 如果没有Tempo事件，添加默认120 BPM（MIDI标准）
        if (tempo_events.empty()) {
            tempo_events.emplace_back(0.0, 120.0);
        }

        std::sort(tempo_events.begin(), tempo_events.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        std::vector<NoteEvent> notes;
        for (int track = 0; track < midi.getNumTracks(); ++track) {
            for (int event = 0; event < midi[track].size(); ++event) {
                auto& e = midi[track][event];
                if (e.isNoteOn()) {
                    NoteEvent n;
                    n.start = e.seconds;
                    n.pitch = e.getKeyNumber();

                        for (int end_event = event + 1; end_event < midi[track].size(); ++end_event) {
                            auto& ee = midi[track][end_event];
                            if (ee.isNoteOff() && ee.getKeyNumber() == n.pitch) {
                                double duration_seconds = ee.seconds - n.start;
                                auto it = std::upper_bound(tempo_events.begin(), tempo_events.end(), n.start,
                                [](double val, const auto& elem) { return val < elem.first; });
                                if (it != tempo_events.begin()) --it;
                                double current_bpm = it->second;
                                n.note_value = duration_seconds * (current_bpm / 60.0);
                                break;
                            }
                        }
                        notes.push_back(n);
                    }
            }
        }

        if (notes.empty()) {
            throw std::runtime_error("No valid notes found in MIDI file");
        }
        return notes;
    }

    double extract_bpm(const std::string& path) {
        MidiFile midi;
        if (!midi.read(path)) return 120.0;

        for (int track = 0; track < midi.getNumTracks(); ++track) {
            for (int event = 0; event < midi[track].size(); ++event) {
                auto& e = midi[track][event];
                if (e.isMeta() && e.getMetaType() == 0x51) {
                    return 60000000.0 / e.getTempoMicroseconds();
                }
            }
        }
        return 120.0;
    }
}