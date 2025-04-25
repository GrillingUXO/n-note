#include "dtw_core.h"
#include <cmath>
#include <algorithm>
#include <iostream> 
#include <limits>
#include <map>
#include <numeric>
#include <functional>
#include <unordered_map>


DTWAligner::DTWAligner(const std::vector<NoteEvent>& ref, 
    const std::vector<NoteEvent>& perf,
    double tempo,
    bool use_interval_matching)
: ref_notes(ref), 
  perf_notes(perf), 
  bpm(tempo), 
  use_interval(use_interval_matching),
  ref_mean(0.0)
{
    seconds_per_beat = 60.0 / bpm;
    double total_duration = 0.0;
    for (const auto& note : ref_notes) {
        total_duration += note.duration;
    }
    if (!ref_notes.empty()) {
        ref_mean = total_duration / ref_notes.size();
    }
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
    double cumulative_duration = 0.0;

    rel_notes.push_back({
        0.0,
        notes[0].duration / seconds_per_beat,
        0.0
    });
    cumulative_duration += notes[0].duration / seconds_per_beat;

    for (size_t i = 1; i < notes.size(); ++i) {
        const auto& n = notes[i];
        int interval = n.pitch - prev_pitch;
        prev_pitch = n.pitch;
        double rel_duration = n.duration / seconds_per_beat;
        double rel_start = n.start - first_start;

        rel_notes.push_back({
            static_cast<double>(interval),
            rel_duration,
            rel_start
        });
        cumulative_duration += rel_duration;
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
    matches.push_back({
        p_idx,
        perf_notes[p_idx],
        ref_notes[r_idx],
        ref_notes[r_idx].duration / perf_notes[p_idx].duration,
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


void DTWAligner::compute_intervals() {
    ref_intervals.clear();
    for (size_t i = 1; i < ref_notes.size(); ++i) {
        ref_intervals.push_back(ref_notes[i].pitch - ref_notes[i-1].pitch);
    }
    perf_intervals.clear();
    for (size_t i = 1; i < perf_notes.size(); ++i) {
        perf_intervals.push_back(perf_notes[i].pitch - perf_notes[i-1].pitch);
    }
}

std::vector<MatchSegment> DTWAligner::find_similar_segments(double similarity_threshold) {
    compute_intervals(); 
    const int n = ref_intervals.size();
    const int m = perf_intervals.size();
    std::vector<MatchSegment> candidates;
    std::vector<std::vector<int>> dp(n, std::vector<int>(m, 0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            if (ref_intervals[i] == perf_intervals[j]) {
                dp[i][j] = (i > 0 && j > 0) ? dp[i-1][j-1] + 1 : 1;
                if (dp[i][j] >= 2) { // limit
                    int length = dp[i][j] + 1;
                    int ref_start = i - dp[i][j] + 1;
                    int perf_start = j - dp[i][j] + 1;
                    if (ref_start >=0 && perf_start >=0 &&
                        ref_start + length <= ref_notes.size() &&
                        perf_start + length <= perf_notes.size()) 
                    {
                        std::vector<NoteEvent> ref_seg(
                            ref_notes.begin() + ref_start,
                            ref_notes.begin() + ref_start + length
                        );
                        std::vector<NoteEvent> perf_seg(
                            perf_notes.begin() + perf_start,
                            perf_notes.begin() + perf_start + length
                        );
                        double sim = calculate_segment_similarity(ref_seg, perf_seg);
                        if (sim >= similarity_threshold) {
                            candidates.push_back({
                                ref_start,
                                perf_start,
                                length,
                                sim,
                                0.0
                            });
                        }
                    }
                }
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const MatchSegment& a, const MatchSegment& b) {
        return a.length > b.length; 
    });

    std::vector<bool> ref_used(ref_notes.size(), false);
    std::vector<bool> perf_used(perf_notes.size(), false);
    std::vector<MatchSegment> results;

    for (const auto& seg : candidates) {
        bool overlap = false;
        for (int k = 0; k < seg.length; ++k) {
            int r = seg.ref_start + k;
            int p = seg.perf_start + k;
            if (r >= ref_used.size() || p >= perf_used.size() || ref_used[r] || perf_used[p]) {
                overlap = true;
                break;
            }
        }
        if (!overlap) {
            results.push_back(seg);
            for (int k = 0; k < seg.length; ++k) {
                int r = seg.ref_start + k;
                int p = seg.perf_start + k;
                if (r < ref_used.size()) ref_used[r] = true;
                if (p < perf_used.size()) perf_used[p] = true;
            }
        }
    }

    std::sort(results.begin(), results.end(), [](const MatchSegment& a, const MatchSegment& b) {
        return a.similarity > b.similarity;
    });

    return results;
}

double DTWAligner::calculate_segment_similarity(
    const std::vector<NoteEvent>& ref_seg,
    const std::vector<NoteEvent>& perf_seg) 
{
    const double base_score_per_note = 10.0;
    const double rhythm_match_bonus = 5.0;
    const double rhythm_tolerance = 0.15; 
    int rhythm_matches = 0;


    for (size_t i = 1; i < ref_seg.size(); ++i) {
        double ref_ratio = ref_seg[i].duration / ref_seg[i-1].duration;
        double perf_ratio = perf_seg[i].duration / perf_seg[i-1].duration;
        if (std::abs(ref_ratio - perf_ratio) < rhythm_tolerance) {
            rhythm_matches++;
        }
    }


    double length_score = base_score_per_note * ref_seg.size();
    double rhythm_score = rhythm_match_bonus * rhythm_matches;
    double total_score = length_score + rhythm_score;
    return std::min(total_score, 100.0);
}