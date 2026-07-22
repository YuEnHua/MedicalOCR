#include "document_classifier.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace medical_ocr {

// Keyword database for each document type.
// These are used when no template matches — a fallback classification.
static const std::unordered_map<DocumentType, std::vector<std::string>> kKeywordMap = {
    {DocumentType::UltrasoundReport, {
        "超声", "超声波", "B超", "彩超", "多普勒",
        "ultrasound", "sonography"
    }},
    {DocumentType::CTReport, {
        "CT", "计算机断层", "螺旋CT", "ct scan", "computed tomography"
    }},
    {DocumentType::MRIReport, {
        "磁共振", "MRI", "核磁", "MR", "magnetic resonance"
    }},
    {DocumentType::XRayReport, {
        "X线", "X光", "胸片", "DR", "CR", "数字摄影",
        "xray", "x-ray", "radiograph"
    }},
    {DocumentType::LaboratoryReport, {
        "检验报告", "化验", "检验", "生化", "血常规", "尿常规",
        "laboratory", "lab report"
    }}
};

// ============================================================================
// Classify
// ============================================================================

DocumentType DocumentClassifier::Classify(const std::string& all_text) {
    // Convert to lowercase for case-insensitive matching.
    std::string lower = all_text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Score each type by counting keyword matches.
    struct Score {
        DocumentType type;
        int count = 0;
    };
    std::vector<Score> scores;

    for (const auto& [type, keywords] : kKeywordMap) {
        int hits = 0;
        for (const auto& kw : keywords) {
            std::string kw_lower = kw;
            std::transform(kw_lower.begin(), kw_lower.end(), kw_lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (lower.find(kw_lower) != std::string::npos) {
                ++hits;
            }
        }
        if (hits > 0) {
            scores.push_back({type, hits});
        }
    }

    if (scores.empty()) return DocumentType::Unknown;

    // Return the type with the most keyword matches.
    std::sort(scores.begin(), scores.end(),
              [](const Score& a, const Score& b) { return a.count > b.count; });

    return scores[0].type;
}

// ============================================================================
// Get Keywords
// ============================================================================

const std::vector<std::string>& DocumentClassifier::GetKeywords(
    DocumentType type) {
    static const std::vector<std::string> kEmpty;
    auto it = kKeywordMap.find(type);
    return (it != kKeywordMap.end()) ? it->second : kEmpty;
}

}  // namespace medical_ocr
