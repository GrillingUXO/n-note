#include "midi_io.h"
#include "tinyfiledialogs.h"
#include "similarity_calculator.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm> 
#include <cmath>
#include "MidiFile.h" 


std::string select_midi_file(const std::string& dialog_title);
std::vector<NoteEvent> parse_midi_with_retry(const std::string& path);
void generate_similarity_report(const std::vector<MatchSegment>& segments);
void run_alignment_process();


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

        std::cout << "Note pitch " << note.pitch
                  << " | Start time: " << note.start << "s"
                  << " | Note Value: " << note.note_value
                  << " | BPM: " << note.bpm
                  << " | Duration in quarters: " << duration_in_quarters
                  << " | Calculated denominator: " << denominator
                  << std::endl;

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

        auto ref_denominators = calculate_denominators(ref_path, ref_notes);
        auto perf_denominators = calculate_denominators(perf_path, perf_notes);

        std::cout << "\n=== File Info =============================="
                  << "\nReference:  " << ref_path
                  << "\nPerformance: " << perf_path
                  << "\nNotes Count: " << ref_notes.size() << " vs " 
                  << perf_notes.size() << "\n"
                  << "===========================================\n";

        std::cout << "\n=== Reference MIDI Notes (Channel/Pitch/NoteValue) ===" << std::endl;
        for (size_t i = 0; i < ref_notes.size(); ++i) {
            std::cout << "Channel " << ref_notes[i].channel << ": " 
                      << ref_notes[i].pitch << "/" << ref_notes[i].note_value 
                      << std::endl;
        }
        std::cout << "\n=== Performance MIDI Notes (Channel/Pitch/NoteValue) ===" << std::endl;
        for (size_t i = 0; i < perf_notes.size(); ++i) {
            std::cout << "Channel " << perf_notes[i].channel << ": " 
                      << perf_notes[i].pitch << "/" << perf_notes[i].note_value 
                      << std::endl;
        }

        SimilarityCalculator calculator(ref_notes, perf_notes);
        std::vector<MatchSegment> segments = calculator.find_similar_segments(70.0);
        bool fallback_triggered = calculator.was_fallback_used();
        generate_similarity_report(segments, fallback_triggered);

    } catch (const std::exception& e) {
        std::cerr << "\n[Fatal Error] " << e.what() << "\n";
    }
}
