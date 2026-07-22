#include "template_repository.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace medical_ocr {

// ============================================================================
// Load From Directory
// ============================================================================

int TemplateRepository::LoadFromDirectory(const std::string& directory_path) {
    templates_.clear();

#ifdef _WIN32
    // Use Windows FindFirstFile/FindNextFile for Unicode support.
    int len = MultiByteToWideChar(CP_UTF8, 0, directory_path.c_str(), -1,
                                  nullptr, 0);
    if (len <= 0) return 0;
    std::wstring wdir(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, directory_path.c_str(), -1, &wdir[0], len);

    // Build search pattern: directory\*.json.
    std::wstring pattern = wdir;
    if (!pattern.empty() && pattern.back() != L'\\' && pattern.back() != L'/') {
        pattern += L'\\';
    }
    pattern += L"*.json";

    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    int count = 0;
    do {
        // Skip directories.
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        std::wstring wfile = wdir;
        if (!wfile.empty() && wfile.back() != L'\\' && wfile.back() != L'/') {
            wfile += L'\\';
        }
        wfile += find_data.cFileName;

        // Convert wide path back to UTF-8.
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wfile.c_str(), -1,
                                           nullptr, 0, nullptr, nullptr);
        std::string utf8_path(utf8_len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wfile.c_str(), -1, &utf8_path[0],
                            utf8_len, nullptr, nullptr);
        // Remove trailing null.
        if (!utf8_path.empty() && utf8_path.back() == '\0') {
            utf8_path.pop_back();
        }

        if (LoadFromFile(utf8_path)) {
            ++count;
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);
    return count;
#else
    // Non-Windows: use standard filesystem iteration.
    // TODO: Use std::filesystem when available.
    return 0;
#endif
}

// ============================================================================
// Load From File
// ============================================================================

bool TemplateRepository::LoadFromFile(const std::string& file_path) {
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) return false;

    try {
        auto j = nlohmann::json::parse(ifs);
        auto tmpl = ParseTemplate(j);
        if (!tmpl.template_id.empty()) {
            templates_.push_back(std::move(tmpl));
            return true;
        }
    } catch (const nlohmann::json::exception&) {
        // Skip malformed templates silently.
    }
    return false;
}

// ============================================================================
// JSON Parsing
// ============================================================================

ReportTemplate TemplateRepository::ParseTemplate(const nlohmann::json& j) {
    ReportTemplate t;
    t.template_id = j.value("template_id", "");
    t.document_type = j.value("document_type", "unknown");
    t.description = j.value("description", "");

    if (j.contains("hospital_keywords") && j["hospital_keywords"].is_array()) {
        t.hospital_keywords = j["hospital_keywords"].get<std::vector<std::string>>();
    }
    if (j.contains("title_keywords") && j["title_keywords"].is_array()) {
        t.title_keywords = j["title_keywords"].get<std::vector<std::string>>();
    }
    if (j.contains("required_keywords") && j["required_keywords"].is_array()) {
        t.required_keywords = j["required_keywords"].get<std::vector<std::string>>();
    }
    if (j.contains("optional_keywords") && j["optional_keywords"].is_array()) {
        t.optional_keywords = j["optional_keywords"].get<std::vector<std::string>>();
    }

    if (j.contains("fields") && j["fields"].is_object()) {
        for (auto it = j["fields"].begin(); it != j["fields"].end(); ++it) {
            t.fields.push_back(ParseFieldRule(it.key(), it.value()));
        }
    }

    return t;
}

FieldRule TemplateRepository::ParseFieldRule(const std::string& field_name,
                                             const nlohmann::json& j) {
    FieldRule rule;
    rule.field_name = field_name;

    if (j.contains("anchors") && j["anchors"].is_array()) {
        rule.anchors = j["anchors"].get<std::vector<std::string>>();
    }
    rule.search_direction = j.value("search_direction", "right");
    rule.same_line_tolerance = j.value("same_line_tolerance", 20);
    rule.max_distance = j.value("max_distance", 250);

    if (j.contains("region") && j["region"].is_array() && j["region"].size() == 4) {
        rule.region = j["region"].get<std::vector<int>>();
    }
    rule.data_type = j.value("data_type", "string");
    rule.extraction_type = j.value("extraction_type", "");

    if (j.contains("start_anchors") && j["start_anchors"].is_array()) {
        rule.start_anchors = j["start_anchors"].get<std::vector<std::string>>();
    }
    if (j.contains("end_anchors") && j["end_anchors"].is_array()) {
        rule.end_anchors = j["end_anchors"].get<std::vector<std::string>>();
    }

    // If extraction_type is not set, derive it from search_direction.
    if (rule.extraction_type.empty()) {
        if (rule.search_direction == "below") {
            rule.extraction_type = "anchor_below";
        } else {
            rule.extraction_type = "anchor_right";
        }
    }

    return rule;
}

}  // namespace medical_ocr
