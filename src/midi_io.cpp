
#include "midi_io.h"
#include "MidiFile.h"
#include <stdexcept>
#include <algorithm> 


using namespace smf;

namespace MIDIIO {
    std::vector<NoteEvent> parse_midi(const std::string& path) {
        MidiFile midi;
        if (!midi.read(path)) {
            throw std::runtime_error("Failed to read MIDI file: " + path);
        }
        midi.doTimeAnalysis();
        midi.linkNotePairs();

        std::vector<std::pair<double, double>> tempo_events; 
        for (int track = 0; track < midi.getNumTracks(); ++track) {
            for (int event = 0; event < midi[track].size(); ++event) {
                auto& e = midi[track][event];
                if (e.isMeta() && e.getMetaType() == 0x51) { 
                    int micros_per_quarter = e.getTempoMicroseconds();
                    double bpm = 60000000.0 / micros_per_quarter;
                    tempo_events.emplace_back(e.seconds, bpm);
                }
            }
        }
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

                    auto it = std::upper_bound(
                        tempo_events.begin(), 
                        tempo_events.end(), 
                        n.start,
                        [](double val, const auto& elem) { 
                            return val < elem.first; 
                        }
                    );
                    if (it != tempo_events.begin()) --it;
                    n.bpm = it->second;

                    for (int end_event = event + 1; end_event < midi[track].size(); ++end_event) {
                        auto& ee = midi[track][end_event];
                        if (ee.isNoteOff() && ee.getKeyNumber() == n.pitch) {
                            double duration_seconds = ee.seconds - n.start;
                            n.note_value = duration_seconds * (n.bpm / 60.0);
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
}