#include "midi_io.h"
#include "tinyfiledialogs.h"
#include "dtw_aligner.h"
#include "similarity_calculator.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#define USE_DTW_ALIGNER 0


std::string select_midi_file(const std::string& dialog_title);
std::vector<NoteEvent> parse_midi_with_retry(const std::string& path);
void generate_alignment_report(const std::vector<MatchResult>& matches);
void generate_similarity_report(const std::vector<MatchSegment>& segments);
void run_alignment_process();


int main() {
    run_alignment_process();
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


void generate_alignment_report(const std::vector<MatchResult>& matches) {
    std::cout << "\n============= Note Alignment Report =============\n";
    std::cout << std::left
              << std::setw(6) << "#" 
              << std::setw(12) << "Ref Pitch"
              << std::setw(14) << "Perf Pitch"
              << std::setw(10) << "Ratio"
              << "Match Stage\n";
    std::cout << "-------------------------------------------------\n";
    
    for (const auto& m : matches) {
        std::cout << std::setw(6) << m.order
                  << std::setw(12) << m.reference.pitch
                  << std::setw(14) << m.performance.pitch
                  << std::setw(10) << std::fixed << std::setprecision(2) << m.time_correction
                  << m.match_round << "\n";
    }
    std::cout << "=================================================\n";
}


void generate_similarity_report(const std::vector<MatchSegment>& segments) {
    constexpr int min_segment_length = 3;  // ***
    
    std::cout << "\n============= Similarity Analysis =============\n";
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


void run_alignment_process() {
    try {
        auto ref_path = select_midi_file("Select Reference MIDI");
        auto perf_path = select_midi_file("Select Performance MIDI");

        auto ref_notes = parse_midi_with_retry(ref_path);
        auto perf_notes = parse_midi_with_retry(perf_path);
        
        std::cout << "\n=== File Info =============================="
                  << "\nReference:  " << ref_path
                  << "\nPerformance: " << perf_path
                  << "\nNotes Count: " << ref_notes.size() << " vs " 
                  << perf_notes.size() << "\n"
                  << "===========================================\n";


#if USE_DTW_ALIGNER
        if (ref_notes.size() > 1) {
            double avg_interval = (ref_notes.back().start - ref_notes.front().start) 
                                / (ref_notes.size() - 1);
            DTWAligner aligner(ref_notes, perf_notes, 60.0 / avg_interval);
            
            auto matches = aligner.align_notes();
            generate_alignment_report(matches);
        } else {
            std::cerr << "Reference needs at least 2 notes for alignment\n";
        }
#else
        std::cout << "\n DTW alignment disabled\n";
#endif

        SimilarityCalculator calculator(ref_notes, perf_notes);
        generate_similarity_report(calculator.find_similar_segments(0.7));
        
    } catch (const std::exception& e) {
        std::cerr << "\n[Fatal Error] " << e.what() << "\n";
    }
}