#ifndef MEDICAL_OCR_TEMPLATE_REPOSITORY_H_
#define MEDICAL_OCR_TEMPLATE_REPOSITORY_H_

#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace medical_ocr {

/**
 * Represents a single field extraction rule from a template.
 */
struct FieldRule {
    std::string field_name;
    std::vector<std::string> anchors;
    std::string search_direction;  // "right" | "below"
    int same_line_tolerance = 20;
    int max_distance = 250;
    std::vector<int> region;       // [x1, y1, x2, y2] in 0–1000 normalized coords
    std::string data_type;         // "string" | "date" | "age" | "gender" | "id_number" | "text"
    std::string extraction_type;   // "anchor_right" | "anchor_below" | "paragraph_between_anchors"
    std::vector<std::string> start_anchors;
    std::vector<std::string> end_anchors;
};

/**
 * Represents a loaded hospital report template.
 */
struct ReportTemplate {
    std::string template_id;
    std::string document_type;  // "ultrasound_report", "ct_report", etc.
    std::string description;
    std::vector<std::string> hospital_keywords;
    std::vector<std::string> title_keywords;
    std::vector<std::string> required_keywords;
    std::vector<std::string> optional_keywords;
    std::vector<FieldRule> fields;
};

/**
 * Loads and caches hospital report templates from JSON files.
 *
 * Scans the configured templates_directory for *.json files,
 * parses them, and provides access to the loaded templates.
 */
class TemplateRepository {
public:
    /**
     * Load all JSON templates from a directory.
     *
     * @param directory_path  Path to the templates directory.
     * @return                Number of templates loaded.
     */
    int LoadFromDirectory(const std::string& directory_path);

    /**
     * Load a single template from a JSON file.
     *
     * @param file_path  Path to the JSON template file.
     * @return           true on success.
     */
    bool LoadFromFile(const std::string& file_path);

    /**
     * Get all loaded templates.
     */
    const std::vector<ReportTemplate>& GetAll() const { return templates_; }

    /**
     * Get the number of loaded templates.
     */
    size_t Count() const { return templates_.size(); }

    /**
     * Parse a single template from JSON.
     */
    static ReportTemplate ParseTemplate(const nlohmann::json& j);

    /**
     * Parse a single field rule from JSON.
     */
    static FieldRule ParseFieldRule(const std::string& field_name,
                                    const nlohmann::json& j);

private:
    std::vector<ReportTemplate> templates_;
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_TEMPLATE_REPOSITORY_H_
