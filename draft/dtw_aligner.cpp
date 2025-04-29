#include "dtw_aligner.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <iostream> 

DTWAligner::DTWAligner(
    const std::vector<NoteEvent>& ref,
    const std::vector<NoteEvent>& perf,
    double ref_bpm,          // 直接传入参考曲目的BPM
    bool use_interval_matching
) : ref_notes(ref),
    perf_notes(perf),
    bpm(ref_bpm),             // 初始化BPM
    use_interval(use_interval_matching),
    ref_mean(0.0)
{
    seconds_per_beat = 60.0 / bpm; 

    double total_duration = 0.0;
    for (const auto& note : ref_notes) {
        total_duration += note.note_value * seconds_per_beat;
    }

    if (!ref_notes.empty()) {
        ref_mean = total_duration / ref_notes.size();
    }

    std::cout << "[DTWAligner] Initialized with BPM: " << bpm 
              << ", Ref Mean Duration: " << ref_mean << "s\n";
}

std::pair<std::vector<std::vector<double>>, 
          std::vector<std::vector<std::pair<int, int>>>> 

DTWAligner::compute_dtw(
    const std::vector<std::vector<double>>& seq1, 
    const std::vector<std::vector<double>>& seq2) 
{
    const int n = seq1.size();
    const int m = seq2.size();
    std::vector<std::vector<double>> cost(n + 1, std::vector<double>(m + 1, std::numeric_limits<double>::infinity()));
    cost[0][0] = 0.0;

    std::vector<std::vector<std::pair<int, int>>> path(n + 1, std::vector<std::pair<int, int>>(m + 1));

    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            double diff = 0.0;
            for (size_t k = 0; k < seq1[i-1].size(); ++k) {
                diff += std::pow(seq1[i-1][k] - seq2[j-1][k], 2);
            }
            diff = std::sqrt(diff);

            std::vector<double> candidates {
                cost[i-1][j], 
                cost[i][j-1], 
                cost[i-1][j-1]
            };
            auto min_it = std::min_element(candidates.begin(), candidates.end());
            cost[i][j] = diff + *min_it;

            int dir = std::distance(candidates.begin(), min_it);
            switch (dir) {
                case 0: path[i][j] = {i-1, j}; break;
                case 1: path[i][j] = {i, j-1}; break;
                case 2: path[i][j] = {i-1, j-1}; break;
            }
        }
    }
    return {cost, path};
}

std::vector<std::vector<double>> DTWAligner::calculate_relative_metrics(
    const std::vector<NoteEvent>& notes) 
{
    std::vector<std::vector<double>> rel_notes;
    if (notes.empty()) return rel_notes;

    double first_start = notes[0].start;
    int prev_pitch = notes[0].pitch;


    rel_notes.push_back({
        0.0,
        notes[0].note_value,
        0.0
    });

    for (size_t i = 1; i < notes.size(); ++i) {
        const auto& n = notes[i];
        int interval = n.pitch - prev_pitch;
        prev_pitch = n.pitch;
        double rel_duration = n.note_value;  
        double rel_start = n.start - first_start;

        rel_notes.push_back({
            static_cast<double>(interval),
            rel_duration,
            rel_start
        });
    }
    return rel_notes;
}

std::vector<std::vector<double>> DTWAligner::get_context_features(
    const std::vector<std::vector<double>>& notes, 
    int index) 
{
    std::vector<std::vector<double>> context;
    for (int i = std::max(0, index - 1); i < std::min((int)notes.size(), index + 2); ++i) {
        context.push_back(notes[i]);
    }
    return context;
}

double DTWAligner::context_distance(
    const std::vector<std::vector<double>>& ctx1,
    const std::vector<std::vector<double>>& ctx2) 
{
    auto [cost_matrix, _] = compute_dtw(ctx1, ctx2);
    return cost_matrix.back().back();
}


void DTWAligner::add_match(
    std::vector<MatchResult>& matches, 
    int p_idx, 
    int r_idx,
    double score, 
    int round) 
{
    double time_correction = ref_notes[r_idx].note_value / perf_notes[p_idx].note_value;
    
    matches.push_back({
        p_idx,
        perf_notes[p_idx],
        ref_notes[r_idx],
        time_correction,
        score,
        (round == 1) ? "Round1" : "Round2"
    });
}


void DTWAligner::add_unmatched(
    std::vector<MatchResult>& matches, 
    int p_idx) 
{
    matches.push_back({
        p_idx,
        perf_notes[p_idx],
        NoteEvent{0, 0, 0},
        1.0,
        std::numeric_limits<double>::quiet_NaN(),
        "Unmatched"
    });
}

std::vector<MatchResult> DTWAligner::align_notes() {
    auto ref_rel = calculate_relative_metrics(ref_notes);
    auto perf_rel = calculate_relative_metrics(perf_notes);

    std::vector<MatchResult> matches;
    std::unordered_map<int, bool> matched_ref, matched_perf;
    for (size_t p_idx = 0; p_idx < perf_rel.size(); ++p_idx) {
        if (matched_perf[p_idx]) continue;

        double min_score = std::numeric_limits<double>::max();
        int best_ref_idx = -1;

        for (size_t r_idx = 0; r_idx < ref_rel.size(); ++r_idx) {
            if (matched_ref[r_idx]) continue;

            if (std::abs(ref_rel[r_idx][0] - perf_rel[p_idx][0]) > 0.0) continue;
            if (std::abs(ref_rel[r_idx][1] - perf_rel[p_idx][1]) > duration_tolerance_ratio * ref_mean) continue;
            if (std::abs(ref_rel[r_idx][2] - perf_rel[p_idx][2]) > position_tolerance) continue;

            auto ctx_ref = get_context_features(ref_rel, r_idx);
            auto ctx_perf = get_context_features(perf_rel, p_idx);
            double score = context_distance(ctx_ref, ctx_perf);

            if (score < min_score) {
                min_score = score;
                best_ref_idx = r_idx;
            }
        }

        if (best_ref_idx != -1) {
            add_match(matches, p_idx, best_ref_idx, min_score, 1);
            matched_ref[best_ref_idx] = true;
            matched_perf[p_idx] = true;
        }
    }

    for (size_t p_idx = 0; p_idx < perf_rel.size(); ++p_idx) {
        if (matched_perf[p_idx]) continue;

        double min_score = std::numeric_limits<double>::max();
        int best_ref_idx = -1;

        for (size_t r_idx = 0; r_idx < ref_rel.size(); ++r_idx) {
            if (matched_ref[r_idx]) continue;

            if (std::abs(ref_rel[r_idx][0] - perf_rel[p_idx][0]) > 1.0) continue;

            auto ctx_ref = get_context_features(ref_rel, r_idx);
            auto ctx_perf = get_context_features(perf_rel, p_idx);
            double score = context_distance(ctx_ref, ctx_perf);

            if (score < min_score) {
                min_score = score;
                best_ref_idx = r_idx;
            }
        }

        if (best_ref_idx != -1) {
            add_match(matches, p_idx, best_ref_idx, min_score, 2);
            matched_ref[best_ref_idx] = true;
            matched_perf[p_idx] = true;
        }
    }

    for (size_t p_idx = 0; p_idx < perf_notes.size(); ++p_idx) {
        if (!matched_perf[p_idx]) {
            add_unmatched(matches, p_idx);
        }
    }

    std::sort(matches.begin(), matches.end(), 
        [](const MatchResult& a, const MatchResult& b) {
            return a.order < b.order;
        });


    return matches;
}

