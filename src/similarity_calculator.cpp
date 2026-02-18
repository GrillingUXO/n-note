#include "similarity_calculator.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <iostream>

SimilarityCalculator::SimilarityCalculator(
    const std::vector<NoteEvent>& ref,
    const std::vector<NoteEvent>& perf
) : ref_notes(ref), perf_notes(perf) {}


bool SimilarityCalculator::was_fallback_used() const {
    return fallback_used;
}

void SimilarityCalculator::compute_intervals() {
    ref_intervals.clear();
    for (size_t i = 1; i < ref_notes.size(); ++i) {
        ref_intervals.push_back(ref_notes[i].pitch - ref_notes[i-1].pitch);
    }
    perf_intervals.clear();
    for (size_t i = 1; i < perf_notes.size(); ++i) {
        perf_intervals.push_back(perf_notes[i].pitch - perf_notes[i-1].pitch);
    }
}


void SimilarityCalculator::perform_fallback_check(
    std::vector<MatchSegment>& candidates,
    double threshold,
    bool use_musical_time 
) {
    fallback_used = true;
    if (ref_notes.size() < 2 || perf_notes.size() < 2) {
        return;
    }

    constexpr double rhythm_tolerance = 0.15;
    constexpr double absolute_epsilon = 0.01;

    for (size_t perf_start = 0; perf_start + 1 < perf_notes.size(); ++perf_start) {
        size_t ref_idx = 0;
        size_t perf_idx = perf_start;
        int matched_pairs = 0;

        while (ref_idx + 1 < ref_notes.size() && perf_idx + 1 < perf_notes.size()) {
            int ref_interval = ref_notes[ref_idx + 1].pitch - ref_notes[ref_idx].pitch;
            
            double ref_duration;
            if (use_musical_time) {
                ref_duration = ref_notes[ref_idx].note_value; 
            } else {

                ref_duration = ref_notes[ref_idx].note_value * (60.0 / ref_notes[ref_idx].bpm) * 4;
            }

            bool interval_matched = false;

            for (size_t search_idx = perf_idx + 1; search_idx < perf_notes.size(); ++search_idx) {
                int perf_interval = perf_notes[search_idx].pitch - perf_notes[perf_idx].pitch;

                double accumulated_duration = 0.0;
                if (use_musical_time) {
                    for (size_t k = perf_idx; k < search_idx; ++k) {
                        accumulated_duration += perf_notes[k].note_value;
                    }
                } else {
                    for (size_t k = perf_idx; k < search_idx; ++k) {
                        accumulated_duration += perf_notes[k].note_value * (60.0 / perf_notes[k].bpm) * 4;
                    }
                }

                std::cout << "[Fallback " << (use_musical_time ? "Musical" : "Absolute") 
                          << "] Trying ref[" << ref_idx << "] (interval=" << ref_interval 
                          << ", dur=" << ref_duration << ") with perf[" << perf_idx 
                          << "->" << search_idx << "] (acc_dur=" << accumulated_duration << ")\n";

                if (perf_interval == ref_interval &&
                    accumulated_duration + absolute_epsilon >= (1.0 - rhythm_tolerance) * ref_duration &&
                    accumulated_duration - absolute_epsilon <= (1.0 + rhythm_tolerance) * ref_duration) 
                {
                    perf_idx = search_idx;
                    ++ref_idx;
                    ++matched_pairs;
                    interval_matched = true;
                    break;
                }

                if (accumulated_duration > (1.0 + rhythm_tolerance) * ref_duration + absolute_epsilon) {
                    break;
                }
            }

            if (!interval_matched) {
                ++perf_idx;
            }
        }

        if (matched_pairs >= 2) {
            int matched_length = static_cast<int>(perf_idx - perf_start + 1);
            double similarity = std::min(matched_pairs * 50.0, 100.0);
            candidates.push_back({
                0,
                static_cast<int>(perf_start),
                matched_length,
                similarity,
                0.0
            });
        }
    }
}


std::vector<MatchSegment> SimilarityCalculator::find_similar_segments(double similarity_threshold) {
    fallback_used = false;
    compute_intervals();
    const int n = ref_intervals.size();
    const int m = perf_intervals.size();
    std::vector<MatchSegment> candidates;

    if (n > 0 && m > 0) {
        std::vector<std::vector<int>> dp(n, std::vector<int>(m, 0));
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < m; ++j) {
                if (ref_intervals[i] == perf_intervals[j]) {
                    dp[i][j] = (i > 0 && j > 0) ? dp[i-1][j-1] + 1 : 1;
                    if (dp[i][j] >= 4) {
                        int length = dp[i][j] + 1;
                        int ref_start = i - dp[i][j] + 1;
                        int perf_start = j - dp[i][j] + 1;
                        if (ref_start >=0 && perf_start >=0 &&
                            ref_start + length <= static_cast<int>(ref_notes.size()) &&
                            perf_start + length <= static_cast<int>(perf_notes.size())) 
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
                                    static_cast<int>(ref_start), 
                                    static_cast<int>(perf_start), 
                                    static_cast<int>(length), 
                                    sim,
                                    0.0
                                });
                            }
                        }
                    }
                }
            }
        }
    }

    bool has_high_similarity = false;
    for (const auto& seg : candidates) {
        if (seg.similarity >= 50.0) { 
            has_high_similarity = true;
            break;
        }
    }

    if (!has_high_similarity) {
        perform_fallback_check(candidates, similarity_threshold, true); 

        if (candidates.empty()) {
            perform_fallback_check(candidates, similarity_threshold, false);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const MatchSegment& a, const MatchSegment& b) {
        if (a.length != b.length) return a.length > b.length;
        return a.similarity > b.similarity;
    });

    std::vector<bool> ref_used(ref_notes.size(), false);
    std::vector<bool> perf_used(perf_notes.size(), false);
    std::vector<MatchSegment> results;

    for (const auto& seg : candidates) {
        bool overlap = false;
        for (size_t k = 0; k < seg.length; ++k) {
            size_t r = seg.ref_start + k;
            size_t p = seg.perf_start + k;
            if (r >= ref_used.size() || p >= perf_used.size() || ref_used[r] || perf_used[p]) {
                overlap = true;
                break;
            }
        }
        if (!overlap) {
            results.push_back(seg);
            for (size_t k = 0; k < seg.length; ++k) {
                size_t r = seg.ref_start + k;
                size_t p = seg.perf_start + k;
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


double SimilarityCalculator::calculate_segment_similarity(
    const std::vector<NoteEvent>& ref_seg,
    const std::vector<NoteEvent>& perf_seg) 
{
    const double base_score_per_note = 5.0;
    const double rhythm_match_bonus = 2.0;
    const double rhythm_tolerance = 0.15;
    int rhythm_matches = 0;

    for (size_t i = 0; i < ref_seg.size(); ++i) {
        double ref_duration = ref_seg[i].note_value;
        double perf_duration = perf_seg[i].note_value;
        double duration_diff = std::abs(ref_duration - perf_duration);
        double allowed_diff = ref_duration * rhythm_tolerance;
        
        if (duration_diff <= allowed_diff) {
            rhythm_matches++;
        }
    }

    double length_score = base_score_per_note * ref_seg.size();
    double rhythm_score = rhythm_match_bonus * rhythm_matches;
    double total_score = length_score + rhythm_score;
    return std::min(total_score, 100.0);
}
