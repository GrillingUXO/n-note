#pragma once
#include <vector>
#include <string>
#include <map>
#include <functional>

struct NoteEvent {
    double start;
    int pitch;
    double duration;
};

struct MatchResult {
    int order;
    NoteEvent performance;
    NoteEvent reference;
    double time_correction;
    double dtw_score;
    std::string match_round;

    MatchResult(int o, const NoteEvent& p, const NoteEvent& r,
                double tc, double score, const std::string& round)
        : order(o), performance(p), reference(r),
          time_correction(tc), dtw_score(score), match_round(round) {}
};

struct MatchSegment {
    int ref_start;
    int perf_start;
    int length;
    double similarity;
    double dtw_score;
};

class DTWAligner {
public:
    DTWAligner(const std::vector<NoteEvent>& ref,
               const std::vector<NoteEvent>& perf,
               double tempo,
               bool use_interval_matching = false);

    std::vector<MatchResult> align_notes();
    std::vector<MatchSegment> find_similar_segments(double similarity_threshold);

private:

    std::vector<NoteEvent> ref_notes;
    std::vector<NoteEvent> perf_notes;
    double bpm;
    double seconds_per_beat;
    bool use_interval;
    double ref_mean;
    std::vector<int> ref_intervals; 
    std::vector<int> perf_intervals;

    const double duration_tolerance_ratio = 0.3;
    const double position_tolerance = 0.5;
    const int interval_tolerance = 1;

    std::pair<std::vector<std::vector<double>>, 
              std::vector<std::vector<std::pair<int, int>>>> 
    compute_dtw(const std::vector<std::vector<double>>& seq1, 
               const std::vector<std::vector<double>>& seq2);

    std::vector<std::vector<double>> calculate_relative_metrics(const std::vector<NoteEvent>& notes);
    std::vector<std::vector<double>> get_context_features(const std::vector<std::vector<double>>& notes, int index);
    double context_distance(const std::vector<std::vector<double>>& ctx1, const std::vector<std::vector<double>>& ctx2);
    void add_match(std::vector<MatchResult>& matches, int p_idx, int r_idx, double score, int round);
    void add_unmatched(std::vector<MatchResult>& matches, int p_idx);
    double calculate_segment_similarity(const std::vector<NoteEvent>& ref_seg, const std::vector<NoteEvent>& perf_seg);
    void compute_intervals();
};