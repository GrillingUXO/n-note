#include "midi_io.h"
#include "MidiFile.h"
#include <stdexcept>

using namespace smf;

namespace MIDIIO {
    std::vector<NoteEvent> parse_midi(const std::string& path) {
        MidiFile midi;
        if (!midi.read(path)) {
            throw std::runtime_error("Failed to read MIDI file: " + path);
        }
        midi.doTimeAnalysis();
        
        std::vector<NoteEvent> notes;
        for (int track = 0; track < midi.getNumTracks(); ++track) {
            for (int event = 0; event < midi[track].size(); ++event) {
                auto& e = midi[track][event];
                if (e.isNoteOn()) {
                    NoteEvent n;
                    n.start = e.seconds;
                    n.pitch = e.getKeyNumber();
                    
                    // 查找对应的NoteOff事件
                    for (int end_event = event+1; end_event < midi[track].size(); ++end_event) {
                        auto& ee = midi[track][end_event];
                        if (ee.isNoteOff() && ee.getKeyNumber() == n.pitch) {
                            n.duration = ee.seconds - n.start;
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