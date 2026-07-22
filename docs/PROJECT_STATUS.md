# Medical OCR — 项目工作文档

> 最后更新：2026-07-22
> 当前阶段：**全部 5 个阶段已完成** (118 tests, 100% pass)

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
| DLL 接口 | C ABI（7 个导出函数） |
| 测试 | 118 个用例，10 个测试套件，100% 通过 |

---

## 二、项目结构总览

```
MedicalOCR/
├── CMakeLists.txt                    # 根构建配置
├── cmake/
│   ├── Dependencies.cmake            # nlohmann/json, GoogleTest, OpenCV, Paddle
│   └── FindPaddleInference.cmake     # Paddle SDK 查找模块
├── include/medical_ocr/              # 公开 C API 头文件
│   ├── medical_ocr_c_api.h           # 7 个 DLL 导出函数
│   ├── types.h                       # 全部数据结构
│   └── error_codes.h                 # 20 个错误码
├── src/
│   ├── api/
│   │   └── medical_ocr_c_api.cpp     # C ABI 实现（异常安全 + 线程安全）
│   ├── core/
│   │   ├── medical_ocr_service.h     # 单例服务层
│   │   └── medical_ocr_service.cpp   # 配置加载、引擎工厂、流水线编排、JSON 构建
│   ├── image/
│   │   ├── image_quality_checker.*   # 图像质量评估（空图/模糊/曝光/反光）
│   │   └── document_preprocessor.*   # 预处理（灰度/降噪/CLAHE/透视矫正/幅面检测）
│   ├── ocr/
│   │   ├── i_ocr_engine.h            # IOcrEngine 抽象接口
│   │   ├── mock_ocr_engine.*         # Mock 引擎（模拟数据）
│   │   └── paddle_ocr_engine.*       # PaddleOCR 引擎（真实 + Stub）
│   ├── document/
│   │   ├── template_repository.*     # JSON 模板加载
│   │   ├── document_classifier.*     # 文档类型分类
│   │   └── template_matcher.*        # 模板匹配 + 评分
│   ├── extraction/
│   │   ├── anchor_field_extractor.*  # 锚点→右侧/下方字段搜索
│   │   ├── paragraph_extractor.*     # 起止锚点段落提取
│   │   └── field_extractor.*         # 字段提取编排 + 校验 + 交叉验证
│   ├── validation/
│   │   ├── id_card_validator.*       # 身份证 GB 11643-1999 校验
│   │   ├── date_validator.*          # 日期格式统一 + 年龄计算
│   │   └── field_normalizer.*        # 性别/年龄标准化 + OCR 混淆修正
│   └── common/
│       ├── utf8_utils.*              # UTF-8 ↔ UTF-16 互转
│       └── geometry_utils.*          # 坐标归一化、同行/距离/区域判断
├── apps/cli_demo/main.cpp            # CLI 演示程序
├── tests/
│   ├── test_dll_api.cpp              # DLL API 生命周期测试（18 个）
│   ├── test_image_quality.cpp        # 图像质量测试（17 个）
│   ├── test_preprocessing.cpp        # 预处理测试（18 个）
│   ├── test_id_card_validator.cpp    # 身份证校验测试（16 个）
│   ├── test_date_validator.cpp       # 日期校验测试（13 个）
│   ├── test_field_normalizer.cpp     # 字段标准化测试（15 个）
│   ├── test_extraction.cpp           # 字段提取测试（15 个）
│   └── test_concurrency.cpp          # 并发/压力测试（6 个）
├── config/
│   ├── medical_ocr.json              # 默认配置
│   └── templates/
│       └── sample_template.json      # 示例超声报告模板
├── examples/
│   ├── example_c.c                   # C 语言集成
│   ├── example_cpp.cpp               # C++ RAII 集成
│   ├── ExampleCs.cs                  # C# P/Invoke 集成
│   └── example_py.py                 # Python ctypes 集成
├── scripts/
│   ├── download_models.ps1           # PP-OCRv4 模型下载脚本
│   └── package_release.ps1           # 发布打包脚本
├── models/README.md                  # 模型目录说明
├── samples/
│   ├── mock_ocr_result.json          # 预录制 OCR 模拟数据
│   └── README.md
└── docs/
    ├── API.md                        # API 参考（完整）
    ├── ARCHITECTURE.md               # 架构设计
    ├── BUILD_WINDOWS.md              # Windows 构建指南
    ├── DEPLOYMENT.md                 # 部署指南
    ├── PADDLE_SETUP.md               # PaddleOCR 集成指南
    ├── TEMPLATE_FORMAT.md            # 模板格式规范
    └── PROJECT_STATUS.md             # 本文件（项目状态）
```

---

## 三、各阶段完成情况

### Phase 1 — 基础框架 ✅

| 功能 | 文件 | 测试 |
|------|------|------|
| C ABI DLL（7 个导出函数） | `src/api/medical_ocr_c_api.cpp` | 18 |
| 20 个错误码 + 字符串转换 | `include/medical_ocr/error_codes.h` | — |
| IOcrEngine 抽象接口 | `src/ocr/i_ocr_engine.h` | — |
| MockOcrEngine（内置样本 + JSON 加载） | `src/ocr/mock_ocr_engine.*` | — |
| MedicalOcrService 单例 | `src/core/medical_ocr_service.*` | — |
| UTF-8 JSON 构建 | 同上 | — |
| UTF-8 ↔ UTF-16 互转 + 校验 | `src/common/utf8_utils.*` | — |
| 几何工具（归一化、同行判断） | `src/common/geometry_utils.*` | — |
| CLI Demo | `apps/cli_demo/main.cpp` | — |
| 异常安全（DLL 边界捕获） | `src/api/` | — |
| 线程安全（mutex + thread_local） | `src/core/` + `src/api/` | — |

### Phase 2 — 图像处理 ✅

| 功能 | 文件 | 测试 |
|------|------|------|
| 空图检测（全黑/全白） | `src/image/image_quality_checker.cpp` | 3 |
| 最低分辨率检查 | 同上 | 2 |
| Laplacian 方差模糊检测 | 同上 | 5 |
| 直方图过曝检测 | 同上 | 2 |
| 连通域反光检测 | 同上 | 2 |
| 可配置阈值 | 同上 | 2 |
| 方向纠正（横→竖旋转） | `src/image/document_preprocessor.cpp` | 2 |
| 灰度化 | 同上 | 1 |
| 高斯降噪 | 同上 | 1 |
| CLAHE 对比度增强 | 同上 | 1 |
| Canny 边缘检测 | 同上 | — |
| 最大凸四边形轮廓查找 | 同上 | 2 |
| 角点排序（TL,TR,BR,BL） | 同上 | 2 |
| warpPerspective 透视矫正 | 同上 | 1 |
| A4/A5 幅面判断 | 同上 | 4 |
| 全部步骤可配置开关 | 同上 | 2 |

### Phase 3 — 字段提取与校验 ✅

| 功能 | 文件 | 测试 |
|------|------|------|
| TemplateRepository（JSON 模板加载） | `src/document/template_repository.*` | — |
| DocumentClassifier（关键词分类） | `src/document/document_classifier.*` | — |
| TemplateMatcher（评分 + 最佳匹配） | `src/document/template_matcher.*` | — |
| AnchorFieldExtractor（锚点→右侧/下方） | `src/extraction/anchor_field_extractor.*` | 10 |
| ParagraphExtractor（起止锚点段落） | `src/extraction/paragraph_extractor.*` | 3 |
| FieldExtractor（编排 + 回退策略） | `src/extraction/field_extractor.*` | 2 |
| Gender 标准化 | `src/validation/field_normalizer.*` | 7 |
| Age 提取 | 同上 | 5 |
| Date 标准化（8 种格式） | `src/validation/date_validator.*` | 13 |
| ID 校验（GB 11643-1999 校验位） | `src/validation/id_card_validator.*` | 16 |
| OCR 混淆修正（O→0, I→1 等） | 同上 | 3 |
| 交叉验证（出生日期 vs 年龄 vs 身份证）| `src/extraction/field_extractor.cpp` | — |

### Phase 4 — PaddleOCR 集成 ✅

| 功能 | 文件 | 测试 |
|------|------|------|
| PaddleOcrEngine（真实实现） | `src/ocr/paddle_ocr_engine.cpp` (#ifdef) | — |
| PaddleOcrEngine（Stub 回退） | 同上 (#else) | — |
| FindPaddleInference.cmake | `cmake/FindPaddleInference.cmake` | — |
| 模型下载脚本 | `scripts/download_models.ps1` | — |
| PaddleOCR 配置文档 | `docs/PADDLE_SETUP.md` | — |
| 引擎工厂支持 "paddle" | `src/core/medical_ocr_service.cpp` | — |

### Phase 5 — 优化、测试、发布 ✅

| 功能 | 文件 | 测试 |
|------|------|------|
| 并发识别（2/4 线程） | `tests/test_concurrency.cpp` | 2 |
| 线程局部错误隔离 | 同上 | 1 |
| 重复 Init/Shutdown 压力 | 同上 | 1 |
| 多线程 OCR_FreeResult(NULL) | 同上 | 1 |
| 内存分配压力测试 | 同上 | 1 |
| 发布打包脚本 | `scripts/package_release.ps1` | — |
| 部署指南 | `docs/DEPLOYMENT.md` | — |
| C 集成示例 | `examples/example_c.c` | — |
| C++ RAII 集成示例 | `examples/example_cpp.cpp` | — |
| C# P/Invoke 集成示例 | `examples/ExampleCs.cs` | — |
| Python ctypes 集成示例 | `examples/example_py.py` | — |

---

## 四、业务处理流程

```
输入图片（文件/内存）
    ↓
[1] ImageQualityChecker     空图/分辨率/模糊/曝光/反光检测
    ↓
[2] DocumentPreprocessor    方向纠正→灰度→降噪→CLAHE→透视矫正→幅面检测
    ↓
[3] IOcrEngine.Recognize()  Mock:模拟数据 / Paddle:真实PP-OCRv4推理
    ↓
[4] 坐标归一化              pixel → 0–1000
    ↓
[5] DocumentClassifier      关键词→文档类型（超声/CT/MRI/X光/检验）
    ↓
[6] TemplateMatcher         模板评分→最佳模板（或 unknown）
    ↓
[7] FieldExtractor          锚点搜索+段落提取+回退策略
    ↓
[8] FieldValidator          ID校验+日期标准化+性别映射+交叉验证
    ↓
[9] ResultBuilder           JSON构建+身份证脱敏+warnings汇总
    ↓
输出 UTF-8 JSON → 调用方 → OCR_FreeResult
```

---

## 五、测试统计

| 套件 | 数量 | 覆盖内容 |
|------|------|---------|
| `DllApiTest` | 18 | API 生命周期、参数校验、错误路径、E2E 流程 |
| `ImageQualityTest` | 17 | 空图、分辨率、模糊、曝光、反光、可配置阈值 |
| `PreprocessorTest` | 18 | 灰度、降噪、CLAHE、轮廓检测、透视矫正、方向、幅面 |
| `IdCardValidatorTest` | 16 | 校验位、格式、OCR 修正、出生日期提取、性别推断 |
| `DateValidatorTest` | 13 | ISO/中文/Slash/8 位数字日期、闰年判定、年龄计算 |
| `FieldNormalizerTest` | 15 | 性别映射、年龄提取、字符串清理 |
| `AnchorFieldExtractorTest` | 10 | 锚点搜索、同行/下方匹配、距离限制、区域检查 |
| `ParagraphExtractorTest` | 3 | 起止锚点段落提取、缺失锚点处理 |
| `FieldExtractorTest` | 2 | 全字段提取编排、空模板处理 |
| `ConcurrencyTest` | 6 | 2/4 线程并发、错误隔离、Init/Shutdown 循环、内存压力 |
| **总计** | **118** | **100% 通过** |

---

## 六、安全与隐私设计

| 要求 | 实现方式 |
|------|---------|
| 完全离线 | 无任何网络请求、HTTP/HTTPS 代码、Socket 调用 |
| 无遥测 | 无崩溃上报、自动更新、使用统计 |
| 身份证脱敏（默认 ON） | `mask_id_number: true` → `4403****1234` 格式 |
| 完整身份证控制 | `mask_id_number: false` — 调用方自行合规 |
| 日志不记录患者数据 | `enable_raw_text_log: false`（默认） |
| 调试图片保护 | `enable_debug_image_output: false`（默认） |
| 零临时文件残留 | 处理结束自动清理 |
| 全局变量无患者数据 | 患者数据仅在 `RunPipeline()` 栈上 |
| DLL 内存边界安全 | `OCR_FreeResult` 统一释放（同堆分配/释放） |

---

## 七、构建命令

```powershell
# Visual Studio 2022
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

# MinGW GCC (Ninja)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DOpenCV_DIR="/ucrt64/lib/cmake/opencv4"
cmake --build build --config Release
ctest --test-dir build -C Release

# 构建产物
# build/bin/MedicalOCR.dll      DLL 主文件
# build/bin/medical_ocr_cli.exe CLI 工具
# build/bin/medical_ocr_tests.exe 118 个测试
```

---

## 八、待扩展功能

### 短期（低复杂度）

| 任务 | 说明 |
|------|------|
| EXIF 方向读取 | 集成 libexif 或手动解析元数据 |
| 多页 PDF/TIFF | 逐页拆分 + 识别 |
| 表格结构识别 | 检验报告项目表格提取 |
| 更多文档类型模板 | CT、MRI、X光 报告模板 |

### 中期（中复杂度）

| 任务 | 说明 |
|------|------|
| OnnxOcrEngine | 基于 ONNX Runtime 的 OCR 后端 |
| RapidOcrEngine | 基于 RapidOCR 的轻量引擎 |
| GPU 加速 | Paddle Inference CUDA/TensorRT 后端 |
| 扫描仪/TWAIN 采集模块 | 独立模块，不在核心 DLL 中 |

### 长期（高复杂度）

| 任务 | 说明 |
|------|------|
| 深度学习文档分类 | 替代规则匹配，提高准确率 |
| 表格单元格提取 | 检验报告数值结构化 |
| 多语言支持 | 英文/日文/韩文医疗报告 |
| Linux/macOS 移植 | 移除 Windows 特定代码 |
