#include "mock_ocr_engine.h"

#include <cstdio>
#include <fstream>
#include <sstream>

// nlohmann/json is fetched by CMake; include from the fetched location.
// When nlohmann/json is not yet available, we fall back to a minimal
// JSON reader for the mock data file.
#include "nlohmann/json.hpp"

namespace medical_ocr {

// ============================================================================
// Construction / Destruction
// ============================================================================

MockOcrEngine::MockOcrEngine() = default;
MockOcrEngine::~MockOcrEngine() { Shutdown(); }

// ============================================================================
// IOcrEngine Interface
// ============================================================================

bool MockOcrEngine::Initialize(const std::string& model_directory,
                               const std::string& config_json) {
    (void)model_directory;  // Not used by mock engine.

    // Parse config JSON to look for mock_data_path.
    std::string mock_data_path;
    if (!config_json.empty()) {
        try {
            auto cfg = nlohmann::json::parse(config_json);
            if (cfg.contains("mock_data_path") && cfg["mock_data_path"].is_string()) {
                mock_data_path = cfg["mock_data_path"].get<std::string>();
            }
        } catch (const nlohmann::json::exception& e) {
            last_error_ = std::string("MockOcrEngine: failed to parse config JSON: ") + e.what();
            return false;
        }
    }

    // If a mock data file is specified, load from it.
    if (!mock_data_path.empty()) {
        if (!LoadMockData(mock_data_path)) {
            return false;
        }
    } else {
        // Otherwise, generate built-in sample data.
        mock_result_ = GenerateSampleResult();
    }

    initialized_ = true;
    last_error_.clear();
    return true;
}

bool MockOcrEngine::Recognize(const std::string& image_path_utf8,
                              OcrResult& result) {
    if (!initialized_) {
        last_error_ = "MockOcrEngine: not initialized";
        return false;
    }

    (void)image_path_utf8;  // Mock engine ignores the actual image.
    result = mock_result_;
    return true;
}

bool MockOcrEngine::Recognize(const unsigned char* image_data,
                              int image_size,
                              OcrResult& result) {
    if (!initialized_) {
        last_error_ = "MockOcrEngine: not initialized";
        return false;
    }

    (void)image_data;
    (void)image_size;  // Mock engine ignores the actual image data.
    result = mock_result_;
    return true;
}

const char* MockOcrEngine::EngineName() const {
    return "MockOcrEngine";
}

std::string MockOcrEngine::GetLastError() const {
    return last_error_;
}

void MockOcrEngine::Shutdown() {
    initialized_ = false;
    mock_result_ = OcrResult{};
    last_error_.clear();
}

// ============================================================================
// JSON Loading
// ============================================================================

bool MockOcrEngine::LoadMockData(const std::string& json_path) {
    std::ifstream ifs(json_path);
    if (!ifs.is_open()) {
        last_error_ = "MockOcrEngine: cannot open mock data file: " + json_path;
        return false;
    }

    try {
        auto j = nlohmann::json::parse(ifs);
        mock_result_.imageWidth = j.value("image_width", 2480);
        mock_result_.imageHeight = j.value("image_height", 3508);

        if (j.contains("boxes") && j["boxes"].is_array()) {
            for (const auto& bj : j["boxes"]) {
                OcrTextBox box;
                if (bj.contains("text")) {
                    box.text = bj["text"].get<std::string>();
                }
                box.confidence = bj.value("confidence", 0.95f);
                box.direction = bj.value("direction", 0.0f);

                if (bj.contains("points") && bj["points"].is_array() &&
                    bj["points"].size() == 4) {
                    for (size_t i = 0; i < 4 && i < bj["points"].size(); ++i) {
                        const auto& pj = bj["points"][i];
                        box.points[i].x = pj.value("x", 0.0f);
                        box.points[i].y = pj.value("y", 0.0f);
                    }
                }
                mock_result_.boxes.push_back(std::move(box));
            }
        }
    } catch (const nlohmann::json::exception& e) {
        last_error_ = std::string("MockOcrEngine: failed to parse mock data JSON: ") +
                      e.what();
        return false;
    }

    return true;
}

// ============================================================================
// Built-in Sample Data
// ============================================================================

// Helper to create a text box with a rectangular bounding box.
static OcrTextBox MakeTextBox(const std::string& text,
                               float x1, float y1, float x2, float y2,
                               float confidence = 0.98f) {
    OcrTextBox box;
    box.text = text;
    box.confidence = confidence;
    box.points = {{
        {x1, y1},  // top-left
        {x2, y1},  // top-right
        {x2, y2},  // bottom-right
        {x1, y2}   // bottom-left
    }};
    return box;
}

OcrResult MockOcrEngine::GenerateSampleResult() const {
    // Simulates a typical Chinese hospital ultrasound report on A4 paper
    // at 300 DPI (~2480 x 3508 pixels).
    OcrResult r;
    r.imageWidth = 2480;
    r.imageHeight = 3508;

    // ---- Header area ----
    // Hospital name at the top.
    r.boxes.push_back(MakeTextBox("某某市第一人民医院", 800, 80, 1680, 140));
    r.boxes.push_back(MakeTextBox("超声检查报告", 950, 160, 1480, 230));

    // ---- Patient info area (left side of the header) ----
    // These are arranged as label-value pairs on the same row.
    r.boxes.push_back(MakeTextBox("姓名：", 200, 300, 350, 360));
    r.boxes.push_back(MakeTextBox("张三", 380, 300, 560, 360));

    r.boxes.push_back(MakeTextBox("性别：", 700, 300, 850, 360));
    r.boxes.push_back(MakeTextBox("男", 880, 300, 960, 360));

    r.boxes.push_back(MakeTextBox("年龄：", 1100, 300, 1250, 360));
    r.boxes.push_back(MakeTextBox("37岁", 1280, 300, 1400, 360));

    r.boxes.push_back(MakeTextBox("出生日期：", 200, 380, 420, 440));
    r.boxes.push_back(MakeTextBox("1988-09-21", 450, 380, 700, 440));

    r.boxes.push_back(MakeTextBox("身份证号：", 200, 460, 430, 520));
    r.boxes.push_back(MakeTextBox("440305198809211234", 460, 460, 880, 520));

    r.boxes.push_back(MakeTextBox("科室：", 200, 540, 330, 600));
    r.boxes.push_back(MakeTextBox("超声科", 360, 540, 520, 600));

    r.boxes.push_back(MakeTextBox("检查日期：", 200, 620, 430, 680));
    r.boxes.push_back(MakeTextBox("2026-07-20", 460, 620, 700, 680));

    r.boxes.push_back(MakeTextBox("检查项目：", 200, 700, 430, 760));
    r.boxes.push_back(MakeTextBox("腹部超声检查", 460, 700, 760, 760));

    // ---- Examination findings ----
    r.boxes.push_back(MakeTextBox("检查所见：", 200, 900, 430, 970));
    r.boxes.push_back(MakeTextBox(
        "肝脏形态大小正常，表面光滑，实质回声均匀。肝内管道系统走行正常，"
        "门静脉内径正常。胆囊大小形态正常，壁光滑，腔内未见异常回声。"
        "胆总管未见扩张。胰腺形态大小正常，实质回声均匀，胰管未见扩张。"
        "脾脏大小形态正常，实质回声均匀。双肾大小形态正常，实质回声均匀，"
        "集合系统未见分离。",
        200, 1000, 2280, 1400, 0.96f));

    // ---- Impression ----
    r.boxes.push_back(MakeTextBox("诊断意见：", 200, 1500, 430, 1570));
    r.boxes.push_back(MakeTextBox(
        "1. 肝脏、胆囊、胰腺、脾脏、双肾未见明显异常。\n"
        "2. 建议定期复查。",
        200, 1600, 2280, 1800, 0.97f));

    // ---- Doctor info ----
    r.boxes.push_back(MakeTextBox("报告医生：", 200, 1950, 430, 2010));
    r.boxes.push_back(MakeTextBox("李医生", 460, 1950, 620, 2010));

    r.boxes.push_back(MakeTextBox("审核医生：", 900, 1950, 1130, 2010));
    r.boxes.push_back(MakeTextBox("王主任", 1160, 1950, 1360, 2010));

    // ---- Footer ----
    r.boxes.push_back(MakeTextBox("本报告仅供临床参考，不作为最终诊断依据。",
                                  700, 2150, 1800, 2210, 0.95f));

    return r;
}

}  // namespace medical_ocr
