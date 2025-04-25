#include "similarity_calculator.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <numeric>
#include <unordered_map>


SimilarityCalculator::SimilarityCalculator(const std::vector<NoteEvent>& ref,
                                           const std::vector<NoteEvent>& perf)
    : ref_notes(ref), perf_notes(perf) {}
    
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

std::vector<MatchSegment> SimilarityCalculator::find_similar_segments(double similarity_threshold) {
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

double SimilarityCalculator::calculate_segment_similarity(
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