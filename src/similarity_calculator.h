#pragma once
#include <vector>
#include "common_defs.h" 

struct MatchSegment {
    int ref_start;
    int perf_start;
    int length;
    double similarity;
    double dtw_score;
};

class SimilarityCalculator {
public:
    SimilarityCalculator(
        const std::vector<NoteEvent>& ref,
        const std::vector<NoteEvent>& perf
    );
    
    std::vector<MatchSegment> find_similar_segments(double similarity_threshold);
    bool was_fallback_used() const;

private:
    std::vector<NoteEvent> ref_notes;
    std::vector<NoteEvent> perf_notes;
    std::vector<int> ref_intervals;
    std::vector<int> perf_intervals;
    bool fallback_used = false;

    void compute_intervals();
    void perform_fallback_check(std::vector<MatchSegment>& candidates, double threshold, bool use_musical_time);
    double calculate_segment_similarity(const std::vector<NoteEvent>& ref_seg,
                                       const std::vector<NoteEvent>& perf_seg);
};