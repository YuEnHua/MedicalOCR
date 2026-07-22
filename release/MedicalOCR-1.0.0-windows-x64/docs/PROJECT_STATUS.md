# Medical OCR — 项目工作文档

> 最后更新：2026-07-22
> 当前阶段：Phase 4 已完成，Phase 5 待开始

---

## 一、项目概述

开发一个 Windows 10/11 x64 原生 C++ DLL，用于**完全离线**识别医疗检查报告（A4/A5 扫描件或照片），输出结构化 UTF-8 JSON，包含患者信息、检查信息、图像质量评估。

| 属性 | 值 |
|------|-----|
| 语言标准 | C++17 |
| 构建系统 | CMake 3.16+ |
| 编译器 | MSVC 2022 / MinGW GCC 15.2 |
| 平台 | Windows x64 |
| 编码 | UTF-8 |
| 开放网络 | 否（完全离线） |

---

## 二、功能模块总览

### 模块清单

```
MedicalOCR/
├── include/medical_ocr/          # 公开 C API 头文件
│   ├── medical_ocr_c_api.h       # 7 个导出函数
│   ├── types.h                   # 所有数据结构定义
│   └── error_codes.h             # 20 个错误码
│
├── src/api/                      # C ABI 实现层
│   └── medical_ocr_c_api.cpp     # 异常安全封装、线程局部错误存储
│
├── src/core/                     # 核心服务层
│   └── medical_ocr_service.*     # 单例、配置加载、引擎工厂、流水线编排
│
├── src/ocr/                      # OCR 引擎层
│   ├── i_ocr_engine.h            # 抽象接口 IOcrEngine
│   ├── mock_ocr_engine.*         # Mock 引擎（生成模拟 OCR 结果）
│   └── paddle_ocr_engine.*       # PaddleOCR 引擎（Phase 4）
│
├── src/image/                    # 图像处理层（Phase 2 ✅）
│   ├── image_quality_checker.*   # 图像质量评估
│   └── document_preprocessor.*   # 文档预处理与透视矫正
│
├── src/document/                 # 文档分析层（Phase 3 ⏳）
│   ├── document_classifier.*     # 报告类型分类
│   ├── template_repository.*     # JSON 模板加载库
│   └── template_matcher.*        # 关键词+布局模板匹配
│
├── src/extraction/               # 字段提取层（Phase 3 ⏳）
│   ├── field_extractor.*         # 字段提取编排
│   ├── anchor_field_extractor.*  # 锚点→右侧/下方搜索
│   └── paragraph_extractor.*     # 起止锚点段落提取
│
├── src/validation/               # 字段校验层（Phase 3 ⏳）
│   ├── id_card_validator.*       # 18 位身份证校验
│   ├── date_validator.*          # 日期格式统一
│   └── field_normalizer.*        # 性别/年龄标准化、OCR 混淆修正
│
├── src/common/                   # 公共工具
│   ├── utf8_utils.*              # UTF-8 ↔ UTF-16 互转、校验
│   ├── geometry_utils.*          # 坐标归一化、同行/右侧/下方判断
│   └── result_builder.*          # JSON 构建（集成在 service 中）
│
├── apps/cli_demo/                # CLI 演示程序
├── tests/                        # 单元测试（53 个，100% 通过）
├── config/                       # 配置文件 + 模板
├── samples/                      # 模拟数据
├── models/                       # OCR 模型目录（Phase 4）
└── docs/                         # 文档
```

### 各模块状态一览

| 模块 | 文件数 | 状态 | 完成度 |
|------|--------|------|--------|
| C API 接口 | 3 头文件 + 1 实现 | ✅ 已完成 | 100% |
| 错误码系统 | 1 头文件 | ✅ 已完成 | 100% |
| Core Service | 2 文件 | ✅ 已完成 | 100% |
| OCR 引擎接口 | 1 头文件 | ✅ 已完成 | 100% |
| Mock OCR 引擎 | 2 文件 | ✅ 已完成 | 100% |
| 图像质量检查 | 2 文件 | ✅ 已完成 | 100% |
| 文档预处理 | 2 文件 | ✅ 已完成 | 100% |
| UTF-8 工具 | 2 文件 | ✅ 已完成 | 100% |
| 几何工具 | 2 文件 | ✅ 已完成 | 100% |
| JSON 构建 | 集成在 Service | ✅ 已完成 | 80%（仅基础字段） |
| 文档分类器 | 0 文件 | ⏳ Phase 3 | 0% |
| 模板仓库 | 0 文件 | ⏳ Phase 3 | 0% |
| 模板匹配器 | 0 文件 | ⏳ Phase 3 | 0% |
| 字段提取器 | 0 文件 | ⏳ Phase 3 | 0% |
| ID 校验器 | 0 文件 | ⏳ Phase 3 | 0% |
| 日期校验器 | 0 文件 | ⏳ Phase 3 | 0% |
| 字段标准化 | 0 文件 | ⏳ Phase 3 | 0% |
| PaddleOCR 引擎 | 0 文件 | ⏳ Phase 4 | 0% |
| CLI Demo | 1 文件 | ✅ 已完成 | 100% |
| 单元测试 | 3 文件 / 53 用例 | ✅ 已完成 | 100% |
| 配置文件 | 2 JSON | ✅ 已完成 | 100% |
| 文档 | 5 文件 | ✅ 已完成 | 100% |

---

## 三、业务处理流程

### 3.1 完整流水线

```
┌──────────────────────────────────────────────────────────────────┐
│                      调用方（C/C++/C#/Python）                     │
│                                                                   │
│   OCR_Init(model_dir, config)  →  初始化（加载配置 + 引擎）        │
│   OCR_RecognizeFile(path, &json)  →  识别图片文件                  │
│   OCR_RecognizeMemory(data, size, &json)  →  识别内存图片          │
│   OCR_FreeResult(json)          →  释放 DLL 分配的字符串           │
│   OCR_Shutdown()                →  释放资源                        │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Stage 1: ImageQualityChecker                                     │
│                                                                   │
│  输入：cv::Mat (BGR 彩色图像)                                      │
│                                                                   │
│  ├─ 1.1 空图检测  →  统计零值像素比例 < 1% → 拒绝                  │
│  ├─ 1.2 全白检测  →  统计 255 值像素比例 < 1% → 拒绝              │
│  ├─ 1.3 分辨率    →  width < 800 或 height < 800 → 拒绝            │
│  ├─ 1.4 模糊检测  →  Laplacian 方差 < 80 → 标记                    │
│  ├─ 1.5 曝光检测  →  直方图 246-255 比例 > 15% → 过曝              │
│  └─ 1.6 反光检测  →  连通域分析 → 白色区域 > 5% 面积 → 标记        │
│                                                                   │
│  输出：QualityInfo { blur_score, is_blurry, is_overexposed,       │
│                      document_detected }                          │
└──────────────────────────┬───────────────────────────────────────┘
                           │ 通过质量检查
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Stage 2: DocumentPreprocessor（每步可配置开关）                    │
│                                                                   │
│  ├─ 2.1 方向纠正    → 横版 → rotate 90°                           │
│  ├─ 2.2 灰度化      → BGR → GRAY（可选）                           │
│  ├─ 2.3 高斯降噪    → GaussianBlur(3×3)                           │
│  ├─ 2.4 CLAHE 增强  → 自适应直方图均衡（clip=2.0, tile=8×8）      │
│  ├─ 2.5 自适应二值  → adaptiveThreshold（可选）                    │
│  ├─ 2.6 边缘检测    → Canny(low=50, high=150)                     │
│  ├─ 2.7 轮廓查找    → findContours → approxPolyDP                  │
│  ├─ 2.8 最大四边形  → 按面积排序，凸四边形优先                      │
│  ├─ 2.9 角点排序    → 按 y 分上下 → 按 x 分左右 → TL,TR,BR,BL     │
│  ├─ 2.10 透视矫正   → getPerspectiveTransform + warpPerspective    │
│  └─ 2.11 幅面检测   → 宽高比 ≈ 0.707 → A4；否则 → unknown         │
│                                                                   │
│  输出：矫正后的 cv::Mat + DocumentDetectionResult                  │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Stage 3: IOcrEngine::Recognize()                                 │
│                                                                   │
│  当前：MockOcrEngine                                               │
│  ├─ 从 JSON 文件加载预录制 OCR 结果                                │
│  └─ 或生成内置模拟超声报告数据                                      │
│                                                                   │
│  未来：PaddleOcrEngine (Phase 4)                                   │
│  ├─ 加载 Paddle Inference 检测模型                                 │
│  ├─ 加载 Paddle Inference 识别模型                                 │
│  └─ 返回真实 OCR 文本框                                            │
│                                                                   │
│  输出：OcrResult { imageWidth, imageHeight, boxes[] }             │
│        OcrTextBox { points[4], text, confidence, direction }      │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Stage 4: DocumentClassifier（Phase 3 ⏳）                         │
│                                                                   │
│  ├─ 搜索 OCR 文本中的医院名称关键词                                │
│  ├─ 搜索报告标题关键词（超声/CT/MRI/X光/检验）                      │
│  └─ 输出：DocumentType + candidate_template_ids                   │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Stage 5: TemplateMatcher（Phase 3 ⏳）                            │
│                                                                   │
│  ├─ 遍历所有候选模板                                               │
│  ├─ 检查 required_keywords 是否全部命中                            │
│  ├─ 统计 hospital_keywords + title_keywords 匹配数                 │
│  └─ 选择得分最高的模板（或 unknown）                                │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Stage 6: FieldExtractor（Phase 3 ⏳）                             │
│                                                                   │
│  对每个模板字段：                                                   │
│  ├─ anchor_right:     找锚点 → 同行右侧最近文本框                   │
│  ├─ anchor_below:     找锚点 → 下方最近文本框                      │
│  ├─ anchor_region:    找锚点 → 归一化区域内的文本                   │
│  └─ paragraph_between: 起始锚点行 → 终止锚点行之间的全部文本         │
│                                                                   │
│  每个字段返回：{ value, raw_value, confidence, source_bbox,        │
│                extraction_method, validation_status }             │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Stage 7: FieldValidator（Phase 3 ⏳）                             │
│                                                                   │
│  ├─ 身份证：18 位校验位算法 → 通过/不通过                          │
│  ├─ 身份证：从第 7-14 位提取出生日期                               │
│  ├─ 身份证：第 17 位奇偶判断性别                                   │
│  ├─ OCR 混淆：O→0, I/l→1, Z→2, B→8, ×→X（仅身份证区域）           │
│  ├─ 日期统一：各种格式 → "YYYY-MM-DD"                              │
│  ├─ 性别统一：男/Male/M/1 → "男"，女/Female/F/0 → "女"            │
│  ├─ 年龄范围：0-150                                                │
│  └─ 冲突检测：出生日期 vs 年龄 vs 身份证 → 不覆盖，记入 warnings    │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│  Stage 8: ResultBuilder                                           │
│                                                                   │
│  ├─ 构建 document 节点（类型、模板 ID、幅面、尺寸）                 │
│  ├─ 构建 patient 节点（姓名、性别、出生日期、年龄、身份证号）       │
│  ├─ 构建 examination 节点（医院、检查项目、所见、诊断、医生）       │
│  ├─ 构建 quality 节点（模糊度、过曝、文档检测）                     │
│  ├─ 隐私处理：mask_id_number 控制身份证脱敏                        │
│  └─ 输出：UTF-8 JSON（美化格式，2 空格缩进）                       │
└──────────────────────────────────────────────────────────────────┘
```

### 3.2 数据流示意

```
图片文件 (JPEG/PNG/BMP/TIFF)
    │
    ▼
OpenCV imdecode (UTF-8 路径 → 宽字符 → 文件读取)
    │
    ▼
cv::Mat BGR 3-channel
    │
    ├──[质量失败]──→ JSON {"success":false, "error_code":2004, "quality":{...}}
    │
    ▼
cv::Mat 预处理后（灰度/CLAHE/去噪/透视矫正）
    │
    ▼
OcrResult (文本框数组)
    │
    ├──[OCR 失败]──→ JSON {"success":false, "error_code":3001}
    │
    ▼
RecognitionResult (患者 + 检查 + 质量 + 警告)
    │
    ▼
UTF-8 JSON string → 调用方 → OCR_FreeResult 释放
```

---

## 四、DLL 导出接口

| 函数 | 参数 | 返回 | 说明 |
|------|------|------|------|
| `OCR_Init` | model_dir (UTF-8), config_path (UTF-8) | int 错误码 | 初始化库，加载配置和引擎 |
| `OCR_RecognizeFile` | image_path (UTF-8), **out_json | int 错误码 | 识别图片文件，JSON 需 OCR_FreeResult 释放 |
| `OCR_RecognizeMemory` | image_data*, size, **out_json | int 错误码 | 识别内存图片数据 |
| `OCR_FreeResult` | json_string* | void | 释放 DLL 分配的字符串 |
| `OCR_GetVersion` | — | const char* | 返回版本号，不可释放 |
| `OCR_GetLastError` | — | const char* | 线程局部错误信息，下次调用覆盖 |
| `OCR_Shutdown` | — | void | 释放所有资源，可无 Init 调用 |

### 错误码速查

| 范围 | 首个错误码 | 类别 |
|------|-----------|------|
| 0 | `MEDOCR_OK` | 成功 |
| 1001–1005 | 未初始化/初始化失败/配置不存在/模型加载失败/重复初始化 | 初始化 |
| 2001–2004 | 文件不存在/格式不支持/解码失败/质量不合格 | 输入 |
| 3001–3002 | OCR 推理失败/无结果 | OCR |
| 4001–4002 | 字段提取失败/无模板匹配 | 提取 |
| 5001 | 输出内存分配失败 | 输出 |
| 9000–9002 | 未知错误/无效参数/内部异常 | 通用 |

---

## 五、配置体系

### medical_ocr.json

```jsonc
{
  "ocr": {
    "engine": "mock",           // "mock" | "paddle" (Phase 4)
    "model_directory": "./models",
    "use_gpu": false,
    "cpu_threads": 4,
    "minimum_confidence": 0.65
  },
  "preprocessing": {
    "enable_document_detection": true,     // 文档轮廓检测
    "enable_perspective_correction": true, // 透视矫正
    "enable_grayscale": false,             // 灰度化
    "enable_contrast_enhancement": true,   // CLAHE 对比度增强
    "enable_adaptive_threshold": false,    // 自适应二值化
    "minimum_width": 800,
    "minimum_height": 800,
    "blur_threshold": 80.0                 // Laplacian 方差阈值
  },
  "privacy": {
    "mask_id_number": true,               // 身份证号默认脱敏
    "enable_debug_image_output": false,    // 调试图片输出（需显式开启）
    "enable_raw_text_log": false           // 原始 OCR 文本日志
  },
  "templates_directory": "./config/templates"
}
```

### 模板文件格式（sample_template.json）

```jsonc
{
  "template_id": "sample_hospital_ultrasound_v1",
  "document_type": "ultrasound_report",
  "hospital_keywords": ["某某市第一人民医院"],
  "title_keywords": ["超声检查报告"],
  "required_keywords": ["姓名", "性别", "检查所见"],
  "fields": {
    "name": {
      "anchors": ["姓名", "患者姓名", "Name"],
      "search_direction": "right",        // right | below
      "same_line_tolerance": 20,          // 垂直容差（归一化单位）
      "max_distance": 250,                // 最大搜索距离
      "region": [0, 0, 1000, 400],        // 搜索区域（归一化 0–1000）
      "data_type": "string"               // string | date | age | gender | id_number | text
    },
    "findings": {
      "start_anchors": ["检查所见", "影像所见"],
      "end_anchors": ["诊断意见", "检查结论"],
      "extraction_type": "paragraph_between_anchors"
    }
  }
}
```

---

## 六、已完成功能（Phase 1 + Phase 2）

### Phase 1 — 基础框架 ✅

| 功能点 | 实现文件 | 测试数 |
|--------|---------|--------|
| C ABI DLL（7 个导出函数） | `src/api/medical_ocr_c_api.cpp` | 18 |
| 20 个错误码 + 字符串转换 | `include/medical_ocr/error_codes.h` | — |
| IOcrEngine 抽象接口 | `src/ocr/i_ocr_engine.h` | — |
| MockOcrEngine（内置样本 + JSON 加载） | `src/ocr/mock_ocr_engine.*` | — |
| MedicalOcrService 单例 | `src/core/medical_ocr_service.*` | — |
| 配置加载 + 引擎工厂 | 同上 | 2 |
| UTF-8 JSON 构建（基础字段） | 同上 | — |
| UTF-8 ↔ UTF-16 互转 + 校验 | `src/common/utf8_utils.*` | — |
| 几何工具（坐标归一化、同行判断等） | `src/common/geometry_utils.*` | — |
| CLI Demo 程序 | `apps/cli_demo/main.cpp` | — |
| 异常安全（DLL 边界捕获） | `src/api/` | — |
| 线程安全（mutex 保护 init/shutdown） | `src/core/` | 3 |
| 重复初始化保护 | `src/core/` | 1 |
| NULL 参数保护 | `src/api/` | 6 |
| 身份证号默认脱敏（配置控制） | `src/core/` | — |
| 调试输出显式启用（默认关闭） | `config/` | — |
| 患者数据不存储于全局变量 | 设计保证 | — |

### Phase 2 — 图像处理 ✅

| 功能点 | 实现文件 | 测试数 |
|--------|---------|--------|
| 空图检测（全黑/全白） | `src/image/image_quality_checker.cpp` | 3 |
| 最低分辨率检查 | 同上 | 2 |
| Laplacian 方差模糊检测 | 同上 | 5 |
| 直方图过曝检测 | 同上 | 2 |
| 连通域反光检测 | 同上 | 2 |
| 可配置阈值 | 同上 | 2 |
| 空 Mat 输入保护 | 同上 | 1 |
| 方向纠正（横版→竖版旋转） | `src/image/document_preprocessor.cpp` | 2 |
| 灰度化 | 同上 | 1 |
| 高斯降噪 | 同上 | 1 |
| CLAHE 对比度增强 | 同上 | 1 |
| Canny 边缘检测 | 同上 | — |
| 最大凸四边形轮廓查找 | 同上 | 2 |
| 角点排序（TL,TR,BR,BL） | 同上 | 2 |
| warpPerspective 透视矫正 | 同上 | 1 |
| A4/A5 幅面判断 | 同上 | 4 |
| 全部步骤可配置开关 | 同上 | 2 |
| EnhanceOnly 模式（跳过几何校正） | 同上 | 1 |
| 空 Mat 保护 | 同上 | 1 |

---

## 七、待完成工作

### Phase 3 — 字段提取与校验 ⏳

| 序号 | 任务 | 预计文件 | 复杂度 | 依赖 |
|------|------|---------|--------|------|
| 3.1 | `TemplateRepository` — 加载 `templates_directory` 下所有 JSON 模板 | `src/document/template_repository.*` | 中 | 无 |
| 3.2 | `DocumentClassifier` — 关键词规则匹配，输出 `DocumentType` | `src/document/document_classifier.*` | 中 | 3.1 |
| 3.3 | `TemplateMatcher` — 统计关键词命中，选出最佳模板 | `src/document/template_matcher.*` | 中 | 3.1, 3.2 |
| 3.4 | `AnchorFieldExtractor` — 锚点→同行右侧/下方最近文本框搜索 | `src/extraction/anchor_field_extractor.*` | 高 | 3.3 |
| 3.5 | `ParagraphExtractor` — 起始锚点→终止锚点之间的段落文本 | `src/extraction/paragraph_extractor.*` | 高 | 3.3 |
| 3.6 | `FieldExtractor` — 编排所有字段提取逻辑 | `src/extraction/field_extractor.*` | 高 | 3.4, 3.5 |
| 3.7 | `IdCardValidator` — 18 位身份证校验位算法、出生日期/性别提取 | `src/validation/id_card_validator.*` | 高 | 无 |
| 3.8 | `DateValidator` — 日期格式统一（YYYY-MM-DD），年龄范围检查 | `src/validation/date_validator.*` | 中 | 无 |
| 3.9 | `FieldNormalizer` — 性别标准化、年龄提取、OCR 混淆字符修正 | `src/validation/field_normalizer.*` | 中 | 无 |
| 3.10 | 将 FieldExtractor + FieldValidator 集成到 MedicalOcrService | `src/core/medical_ocr_service.cpp` | 中 | 3.6–3.9 |
| 3.11 | 完善 ResultBuilder — 填充完整的 patient + examination 字段 | 同上 | 中 | 3.10 |
| 3.12 | 单元测试 — 身份证校验位、日期转换、锚点搜索、段落提取、冲突检测等 | `tests/` | 高 | 3.7–3.9 |
| 3.13 | 端到端测试 — Mock OCR 全流程：图片→分类→模板→提取→校验→JSON | `tests/` | 高 | 全部 |

### Phase 4 — PaddleOCR 集成 ⏳

| 序号 | 任务 | 预计文件 | 复杂度 | 依赖 |
|------|------|---------|--------|------|
| 4.1 | 下载 Paddle Inference C++ 库 | — | 低 | 无 |
| 4.2 | 下载 PP-OCRv4 中文检测+识别模型 | `models/` | 低 | 无 |
| 4.3 | `PaddleOcrEngine` — 初始化 Paddle Inference，加载模型 | `src/ocr/paddle_ocr_engine.*` | 高 | 4.1, 4.2 |
| 4.4 | PaddleOCR 检测结果 → OcrTextBox 转换 | 同上 | 中 | 4.3 |
| 4.5 | PaddleOCR 识别结果 → OcrTextBox 转换 | 同上 | 中 | 4.3 |
| 4.6 | 方向分类器（可选，处理倒置文本） | 同上 | 中 | 4.3 |
| 4.7 | 更新 CMake — `MEDICAL_OCR_ENABLE_PADDLE=ON` | `CMakeLists.txt` | 中 | 4.3 |
| 4.8 | 更新配置 — `ocr.engine: "paddle"` | `config/` | 低 | 4.3 |
| 4.9 | DLL 运行时依赖说明（Paddle DLL 路径） | `docs/` | 低 | 4.3 |
| 4.10 | 真实图片端到端测试 | `tests/` | 高 | 全部 |

### Phase 5 — 优化与发布 ⏳

| 序号 | 任务 | 复杂度 |
|------|------|--------|
| 5.1 | 性能优化 — OCR 推理多线程、预处理流水线并行 | 高 |
| 5.2 | 内存优化 — 大图片分块处理、临时 Mat 复用 | 中 |
| 5.3 | DLL 内存边界测试 — 跨堆分配/释放 | 中 |
| 5.4 | 多线程压力测试 — 并发 Recognize 调用 | 高 |
| 5.5 | Release 打包脚本 — DLL + 头文件 + 模型 + 配置 | 中 |
| 5.6 | 部署文档 — 生产环境安装指南 | 低 |
| 5.7 | 示例程序 — C/C++/C#/Python 调用示例 | 中 |

### 未来扩展（低优先级）

| 任务 | 说明 |
|------|------|
| OnnxOcrEngine | 基于 ONNX Runtime 的 OCR 后端，跨平台 |
| RapidOcrEngine | 基于 RapidOCR（ONNX 加速）的轻量引擎 |
| EXIF 方向读取 | 集成 libexif 或手动解析 EXIF 元数据 |
| 扫描仪/TWAIN 模块 | 独立模块，不在 DLL 核心中 |
| 多页 PDF/TIFF 支持 | 逐页识别 |
| 表格识别 | 检验报告中的检验项目表格结构化提取 |
| GPU 加速 | CUDA/TensorRT Paddle Inference 后端 |

---

## 八、测试统计

### 当前测试覆盖（53 个用例，100% 通过）

| 测试套件 | 用例数 | 类别 |
|---------|--------|------|
| `DllApiTest` | 18 | API 生命周期、参数校验、错误路径、E2E |
| `ImageQualityTest` | 17 | 空图、分辨率、模糊、曝光、反光、配置 |
| `PreprocessorTest` | 18 | 灰度、降噪、CLAHE、轮廓、角点、透视、幅面 |
| **合计** | **53** | — |

### 待添加测试（Phase 3 + Phase 4）

| 类别 | 预计用例数 |
|------|-----------|
| 身份证校验位（合法/非法/边界） | ~10 |
| 日期格式转换 | ~8 |
| 性别标准化 | ~6 |
| 年龄提取 | ~5 |
| 锚点同行右侧搜索 | ~8 |
| 锚点下方搜索 | ~6 |
| 段落起止锚点提取 | ~6 |
| 模板匹配（匹配/不匹配/多候选） | ~6 |
| OCR 混淆字符修正 | ~8 |
| 字段冲突检测 | ~4 |
| PaddleOCR 引擎 | ~8 |
| 端到端全流程 | ~5 |
| **合计（预计）** | **~80** |

---

## 九、安全与隐私设计

| 要求 | 实现方式 |
|------|---------|
| 完全离线 | 无任何网络请求，无 HTTP/HTTPS 代码 |
| 无遥测 | 无崩溃上报、无自动更新、无使用统计 |
| 身份证脱敏 | `privacy.mask_id_number: true` 默认生效，保留前 4 后 4 位 |
| 完整身份证控制 | 通过 `mask_id_number: false` 显式允许，调用方自行合规 |
| 日志不记录患者数据 | `enable_raw_text_log: false` 默认关闭，服务层不写日志 |
| 调试图片保护 | `enable_debug_image_output: false` 默认关闭 |
| 临时文件清理 | 处理结束后删除临时文件 |
| 全局变量无患者数据 | 患者数据仅在函数栈上，`Shutdown` 清空所有状态 |
| C API 边界 | 字符串通过 `OCR_FreeResult` 释放，调用方不能 delete DLL 内存 |

---

## 十、构建与运行

### 构建命令

```powershell
# 配置（Visual Studio 2022）
cmake -S . -B build -A x64

# 配置（MinGW GCC）
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DOpenCV_DIR="path/to/OpenCV/lib/cmake/opencv4"

# 构建
cmake --build build --config Release

# 测试
ctest --test-dir build -C Release --output-on-failure

# 运行 CLI
.\build\bin\medical_ocr_cli.exe report.jpg config\medical_ocr.json
```

### 构建产物

```
build/bin/
├── MedicalOCR.dll           # 主 DLL
├── MedicalOCR.lib           # 导入库
├── medical_ocr_cli.exe      # CLI 演示
└── medical_ocr_tests.exe    # 单元测试
```

### 集成到其他项目

```c
#include "medical_ocr/medical_ocr_c_api.h"

int main() {
    OCR_Init(NULL, "config/medical_ocr.json");

    char* json = NULL;
    int rc = OCR_RecognizeFile("report.jpg", &json);
    if (rc == 0) {
        printf("%s\n", json);
        OCR_FreeResult(json);
    }

    OCR_Shutdown();
    return rc;
}
```
