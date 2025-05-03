#include "midi_io.h"
#include "MidiFile.h"
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <map>
#include <iostream>

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

        std::map<int, std::vector<MidiEvent*>> channel_notes;
        for (int track = 0; track < midi.getNumTracks(); ++track) {
            for (int event = 0; event < midi[track].size(); ++event) {
                auto& e = midi[track][event];
                if (e.isNoteOn()) {
                    int raw_status = e[0]; 
                    int channel = raw_status & 0x0F; 
                    channel_notes[channel].push_back(&e);
                }
            }
        }

        std::vector<NoteEvent> notes;
        double channel_time_offset = 0.0;
        constexpr double REST_DURATION = 10.0;

        for (auto& [channel_num, events] : channel_notes) {
            std::cout << "\n=== channel " << channel_num;

            std::sort(events.begin(), events.end(),
                [](const MidiEvent* a, const MidiEvent* b) {
                    return a->seconds < b->seconds;
                });

            double last_note_end = 0.0;
            for (auto e_ptr : events) {
                auto& e = *e_ptr;
                NoteEvent n;

                n.channel = channel_num;

                n.start = e.seconds + channel_time_offset;
                n.pitch = e.getKeyNumber();

                auto it = std::upper_bound(
                    tempo_events.begin(), tempo_events.end(),
                    e.seconds,  
                    [](double val, const auto& elem) { return val < elem.first; });
                if (it != tempo_events.begin()) --it;
                n.bpm = it->second;

                auto linked = e.getLinkedEvent();
                if (linked) {
                    double original_duration = linked->seconds - e.seconds;
                    n.note_value = original_duration * (n.bpm / 60.0);
                    last_note_end = linked->seconds + channel_time_offset;
                }

                notes.push_back(n);
            }
            if (!events.empty()) {
                channel_time_offset = last_note_end + REST_DURATION;
            }
        }

        if (notes.empty()) {
            throw std::runtime_error("No valid notes found in MIDI file");
        }
        return notes;
    }
}