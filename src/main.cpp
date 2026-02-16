#include "midi_io.h"
#include "tinyfiledialogs.h"
#include "similarity_calculator.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm> 
#include <cmath>
#include <filesystem> 
#include "MidiFile.h" 

namespace fs = std::filesystem;

std::string select_midi_file(const std::string& dialog_title);
std::vector<NoteEvent> parse_midi_with_retry(const std::string& path);
void generate_similarity_report(const std::vector<MatchSegment>& segments, bool fallback_triggered);
void run_alignment_process();
void save_segment_to_midi(const std::vector<NoteEvent>& notes, const std::string& filename);

int main() {
    run_alignment_process();
    std::cout << "\npress enter to exit...";
    std::cin.get();  
    return 0;
}

std::string select_midi_file(const std::string& dialog_title) {
    const char* filters[] = {"*.mid"};
    const char* path = tinyfd_openFileDialog(
        dialog_title.c_str(), "", 1, filters, "MIDI Files", 0);
    
    if (!path) {
        throw std::runtime_error(dialog_title + " selection canceled");
    }
    return std::string(path);
}

std::vector<NoteEvent> parse_midi_with_retry(const std::string& path) {
    const int max_retries = 2;
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        try {
            return MIDIIO::parse_midi(path);
        } catch (const std::exception& e) {
            std::cerr << "\n[Retry " << (attempt+1) << "/" << max_retries << "] "
                      << e.what() << "\n";
            if (attempt == max_retries-1) throw;
        }
    }
    return {}; 
}

void generate_similarity_report(
    const std::vector<MatchSegment>& segments, 
    bool fallback_triggered 
) {
    constexpr int min_segment_length = 3;
    
    std::cout << "\n============= Similarity Analysis =============\n";
    std::cout << "[Fallback Triggered] " << (fallback_triggered ? "Yes" : "No") << "\n";
    std::cout << "Ref Start\tPerf Start\tLength\tSimilarity\n";
    std::cout << "-----------------------------------------------\n";
    
    for (const auto& seg : segments) {
        if (seg.length >= min_segment_length) {
            std::cout << seg.ref_start << "\t\t"
                      << seg.perf_start << "\t\t"
                      << seg.length << "\t"
                      << std::fixed << std::setprecision(1)
                      << seg.similarity << "%\n";
        }
    }
    std::cout << "===============================================\n";
}


void save_segment_to_midi(const std::vector<NoteEvent>& notes, const std::string& filename) {
    if (notes.empty()) return;

    smf::MidiFile midifile;
    midifile.setTicksPerQuarterNote(480);
    int track = 0;
    midifile.addTrack(1);

    midifile.addTempo(track, 0, notes[0].bpm);

    double start_offset = notes[0].start;

    for (const auto& note : notes) {
        double relative_start_sec = note.start - start_offset;
        
        int start_tick = static_cast<int>(relative_start_sec * (note.bpm / 60.0) * 480.0);
        int duration_tick = static_cast<int>(note.note_value * 480.0);

        midifile.addNoteOn(track, start_tick, note.channel, note.pitch, 90);
        midifile.addNoteOff(track, start_tick + duration_tick, note.channel, note.pitch);
    }

    midifile.sortTracks();
    if (midifile.write(filename)) {
        std::cout << "[Export] Saved: " << filename << std::endl;
    } else {
        std::cerr << "[Export Error] Failed to write: " << filename << std::endl;
    }
}

std::vector<double> calculate_denominators(
    const std::string& path, 
    const std::vector<NoteEvent>& notes
) {
    smf::MidiFile midi;
    if (!midi.read(path)) {
        throw std::runtime_error("Failed to read MIDI file: " + path);
    }
    midi.doTimeAnalysis();
    midi.linkNotePairs();

    std::vector<double> denominators;
    for (const auto& note : notes) {
        double quarter_seconds = 60.0 / note.bpm;
        double duration_in_seconds = note.note_value * (60.0 / note.bpm);
        double duration_in_quarters = duration_in_seconds / quarter_seconds;
        double denominator = 4.0 / duration_in_quarters;

        denominators.push_back(denominator);
    }
    return denominators;
}

void run_alignment_process() {
    try {
        auto ref_path = select_midi_file("Select Reference MIDI");
        auto perf_path = select_midi_file("Select Performance MIDI");

        auto ref_notes = parse_midi_with_retry(ref_path);
        auto perf_notes = parse_midi_with_retry(perf_path);

        std::string ref_base = fs::path(ref_path).stem().string();
        std::string perf_base = fs::path(perf_path).stem().string();

        std::cout << "\n=== File Info =============================="
                  << "\nReference:  " << ref_path
                  << "\nPerformance: " << perf_path
                  << "\nNotes Count: " << ref_notes.size() << " vs " 
                  << perf_notes.size() << "\n"
                  << "===========================================\n";

        SimilarityCalculator calculator(ref_notes, perf_notes);
        std::vector<MatchSegment> segments = calculator.find_similar_segments(70.0);
        bool fallback_triggered = calculator.was_fallback_used();
        
        generate_similarity_report(segments, fallback_triggered);

        std::cout << "\n=== Exporting Matches ======================\n";
        int seg_index = 1;
        for (const auto& seg : segments) {
            if (seg.length >= 3) {
                // 提取参考片段子集
                std::vector<NoteEvent> ref_sub(
                    ref_notes.begin() + seg.ref_start,
                    ref_notes.begin() + seg.ref_start + seg.length
                );
                // 提取演奏片段子集
                std::vector<NoteEvent> perf_sub(
                    perf_notes.begin() + seg.perf_start,
                    perf_notes.begin() + seg.perf_start + seg.length
                );

                std::string ref_out_name = ref_base + "_seg" + std::to_string(seg_index) + "_ref.mid";
                std::string perf_out_name = perf_base + "_seg" + std::to_string(seg_index) + "_perf.mid";

                save_segment_to_midi(ref_sub, ref_out_name);
                save_segment_to_midi(perf_sub, perf_out_name);
                
                seg_index++;
            }
        }
        std::cout << "===========================================\n";

    } catch (const std::exception& e) {
        std::cerr << "\n[Fatal Error] " << e.what() << "\n";
    }
}
