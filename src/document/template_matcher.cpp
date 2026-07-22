#include "template_matcher.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace medical_ocr {

// ============================================================================
// Concatenate Text
// ============================================================================

std::string TemplateMatcher::ConcatenateText(const OcrResult& ocr_result) {
    std::ostringstream oss;
    for (size_t i = 0; i < ocr_result.boxes.size(); ++i) {
        if (i > 0) oss << "\n";
        oss << ocr_result.boxes[i].text;
    }
    return oss.str();
}

// ============================================================================
// Match
// ============================================================================

TemplateMatchResult TemplateMatcher::Match(
    const OcrResult& ocr_result,
    const TemplateRepository& repository) const {

    TemplateMatchResult best;
    std::string all_text = ConcatenateText(ocr_result);
    best.all_text = all_text;

    for (const auto& tmpl : repository.GetAll()) {
        bool all_required = false;
        int hospital_hits = 0;
        int title_hits = 0;

        int score = ScoreTemplate(tmpl, all_text, all_required,
                                  hospital_hits, title_hits);

        if (score > best.score && all_required) {
            best.score = score;
            best.matched_template = tmpl;
            best.all_required_found = all_required;
            best.hospital_hits = hospital_hits;
            best.title_hits = title_hits;
        }
    }

    return best;
}

// ============================================================================
// Score Template
// ============================================================================

int TemplateMatcher::ScoreTemplate(const ReportTemplate& tmpl,
                                   const std::string& all_text,
                                   bool& all_required_found,
                                   int& hospital_hits,
                                   int& title_hits) {
    // Check required keywords: ALL must be present.
    all_required_found = true;
    for (const auto& kw : tmpl.required_keywords) {
        if (all_text.find(kw) == std::string::npos) {
            all_required_found = false;
            // We still continue scoring, but the match won't be selected
            // unless all required keywords are found.
        }
    }

    // Count keyword hits.
    hospital_hits = CountKeywordHits(tmpl.hospital_keywords, all_text);
    title_hits = CountKeywordHits(tmpl.title_keywords, all_text);
    int optional_hits = CountKeywordHits(tmpl.optional_keywords, all_text);

    // Score formula:
    //   hospital hits * 10  (hospital is most specific)
    //   + title hits * 8
    //   + required hits * 5
    //   + optional hits * 2
    int score = hospital_hits * 10 + title_hits * 8 + optional_hits * 2;

    // Count how many required keywords were found for scoring.
    int required_hits = 0;
    for (const auto& kw : tmpl.required_keywords) {
        if (all_text.find(kw) != std::string::npos) {
            ++required_hits;
        }
    }
    score += required_hits * 5;

    return score;
}

// ============================================================================
// Count Keyword Hits
// ============================================================================

int TemplateMatcher::CountKeywordHits(
    const std::vector<std::string>& keywords,
    const std::string& text) {
    int hits = 0;
    for (const auto& kw : keywords) {
        if (text.find(kw) != std::string::npos) {
            ++hits;
        }
    }
    return hits;
}

}  // namespace medical_ocr
