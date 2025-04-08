#include "midi_io.h"
#include "dtw_core.h"
#include "tinyfiledialogs.h"
#include "gui.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

void run_alignment_gui() {
    const char* filters[] = {"*.mid"};

    const char* ref_path_cstr = tinyfd_openFileDialog(
        "Select Reference MIDI", "", 1, filters, "MIDI Files", 0);
    if (!ref_path_cstr) {
        std::cerr << "Error: Reference MIDI selection canceled\n";
        return;
    }
    std::string ref_path(ref_path_cstr); 

    const char* perf_path_cstr = tinyfd_openFileDialog(
        "Select Performance MIDI", "", 1, filters, "MIDI Files", 0);
    if (!perf_path_cstr) {
        std::cerr << "Error: Performance MIDI selection canceled\n";
        return;
    }
    std::string perf_path(perf_path_cstr); 

    try {

        auto ref_notes = MIDIIO::parse_midi(ref_path);
        auto perf_notes = MIDIIO::parse_midi(perf_path);

        std::cout << "Reference: " << ref_path << "\n";
        std::cout << "Performance: " << perf_path << "\n";
        std::cout << "===================================\n";

        double total_interval = 0.0;
        if (ref_notes.size() > 1) {
            for (size_t i = 1; i < ref_notes.size(); ++i) {
                total_interval += ref_notes[i].start - ref_notes[i - 1].start;
            }
            double avg_interval = total_interval / (ref_notes.size() - 1);
            double bpm = 60.0 / avg_interval;

            DTWAligner aligner(ref_notes, perf_notes, bpm);

            auto matches = aligner.align_notes();

            std::cout << "\n============= 音符对齐报告 =============\n";
            std::cout << std::left
                      << std::setw(6) << "#number"
                      << std::setw(10) << "Reference pitch"
                      << std::setw(12) << "Performance pitch"
                      << std::setw(12) << "Correction"
                      << "DTW filtering level\n";
            std::cout << "--------------------------------------------\n";

            for (const auto& m : matches) {
                std::cout << std::setw(6) << m.order
                          << std::setw(10) << m.reference.pitch
                          << std::setw(12) << m.performance.pitch
                          << std::setw(12) << std::fixed << std::setprecision(2) << m.time_correction
                          << m.match_round << "\n";
            }
            std::cout << "============================================\n";

            auto similar_segments = aligner.find_similar_segments(0.7);

            std::cout << "\n============= Similarity =============\n";
            std::cout << "Reference initial\tPerformance initial\tLength\tSimilarity\n";
            std::cout << "----------------------------------------\n";
            for (const auto& seg : similar_segments) {
                std::cout << seg.ref_start << "\t\t"
                          << seg.perf_start << "\t\t"
                          << seg.length << "\t"
                          << std::fixed << std::setprecision(1) 
                          << seg.similarity << "%\n";
            }
            std::cout << "========================================\n";
        } else {
            std::cerr << "Error: Insufficient number of notes.\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "\n[error] " << e.what() << "\n";
    }
}