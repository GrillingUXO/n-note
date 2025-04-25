#pragma once
#include <vector>
#include <string>
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

class DTWAligner {
public:
    DTWAligner(const std::vector<NoteEvent>& ref,
               const std::vector<NoteEvent>& perf,
               double tempo,
               bool use_interval_matching = false);

    std::vector<MatchResult> align_notes();

private:
    std::vector<NoteEvent> ref_notes;
    std::vector<NoteEvent> perf_notes;
    double bpm;
    double seconds_per_beat;
    bool use_interval;
    double ref_mean;

    const double duration_tolerance_ratio = 0.3;
    const double position_tolerance = 0.5;

    std::pair<std::vector<std::vector<double>>, 
              std::vector<std::vector<std::pair<int, int>>>> 
    compute_dtw(const std::vector<std::vector<double>>& seq1, 
               const std::vector<std::vector<double>>& seq2);

    std::vector<std::vector<double>> calculate_relative_metrics(const std::vector<NoteEvent>& notes);
    std::vector<std::vector<double>> get_context_features(const std::vector<std::vector<double>>& notes, int index);
    double context_distance(const std::vector<std::vector<double>>& ctx1, const std::vector<std::vector<double>>& ctx2);
    void add_match(std::vector<MatchResult>& matches, int p_idx, int r_idx, double score, int round);
    void add_unmatched(std::vector<MatchResult>& matches, int p_idx);
};