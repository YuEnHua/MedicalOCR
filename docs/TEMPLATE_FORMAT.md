# Template Format Specification

## Overview

Hospital report templates define how to **match** a document and **extract** fields from OCR results. Templates are JSON files stored in the `templates_directory` (configured in `medical_ocr.json`). All coordinates are **normalized to 0–1000**, independent of image resolution.

> **Why 0–1000?**  A template written for a 300 DPI A4 scan (2480×3508 px) also works for a 150 DPI A5 photo (1240×1752 px). The normalization is automatic.

---

## Template File Naming

Templates are loaded from `templates_directory/*.json`. Files with invalid JSON are silently skipped. Use descriptive filenames:
```
templates/
├── hospital_a_ultrasound.json
├── hospital_a_ct.json
├── hospital_b_ultrasound.json
└── hospital_b_laboratory.json
```

---

## Template Structure

```jsonc
{
  // Required — unique identifier (used in output JSON and logging)
  "template_id": "hospital_001_ultrasound_v1",

  // Required — document type
  "document_type": "ultrasound_report",

  // Optional — human-readable note (not used in matching)
  "description": "某某市第一人民医院 超声检查报告 v1",

  // Optional — hospital identifiers (higher weight in scoring)
  "hospital_keywords": ["某某市第一人民医院", "某某医院"],

  // Optional — report title keywords
  "title_keywords": ["超声检查报告", "超声波检查报告"],

  // Required — ALL of these must appear in the OCR text to match
  "required_keywords": ["姓名", "性别", "检查所见"],

  // Optional — bonus score if present, not required to match
  "optional_keywords": ["年龄", "出生日期", "身份证号"],

  // Required — field extraction definitions
  "fields": { ... }
}
```

### Top-Level Fields

| Field | Type | Required | Weight | Description |
|-------|------|----------|--------|-------------|
| `template_id` | string | **Yes** | — | Unique identifier |
| `document_type` | string | **Yes** | — | `ultrasound_report`, `ct_report`, `mri_report`, `xray_report`, `laboratory_report` |
| `description` | string | No | — | For documentation only |
| `hospital_keywords` | string[] | No | ×10 | Hospital name keywords |
| `title_keywords` | string[] | No | ×8 | Report title keywords |
| `required_keywords` | string[] | **Yes** | ×5 | **Must all be present** for this template to match |
| `optional_keywords` | string[] | No | ×2 | Bonus scoring keywords |
| `fields` | object | **Yes** | — | Field extraction rules |

---

## Matching Algorithm (Scoring)

For each loaded template, against the concatenated OCR text:

```
score = hospital_hits × 10
      + title_hits × 8
      + required_hits × 5
      + optional_hits × 2
```

**Match condition**: ALL `required_keywords` must appear in the text. The template with the **highest score** wins. If no template passes the required-keyword check, the document is classified as `"unknown"`.

**Keyword matching** uses `std::string::find()` — substring matching. "超声" matches "超声检查报告". Matching is case-sensitive for Chinese, case-insensitive for ASCII.

---

## Field Definitions

### Supported Extraction Types

| Type | `extraction_type` | Description |
|------|-------------------|-------------|
| Anchor-Right | `anchor_right` (default) | Find anchor keyword → closest box to the right on the same line |
| Anchor-Below | `anchor_below` | Find anchor keyword → closest box directly below |
| Paragraph | `paragraph_between_anchors` | Start anchor → end anchor → all text between |

### Anchor-Right Field

```jsonc
"name": {
  "anchors": ["姓名", "患者姓名", "Name"],  // Keywords to find the label
  "search_direction": "right",               // Search direction
  "same_line_tolerance": 20,                 // Vertical tolerance (0–1000 units)
  "max_distance": 250,                       // Maximum search distance
  "region": [0, 0, 1000, 400],              // Optional: [x1,y1,x2,y2] bounding search area
  "data_type": "string"                      // Validation type
}
```

**How it works:**
1. Scan all OCR boxes for text containing one of the `anchors`
2. From the anchor box, search to the right on the same horizontal line (within `same_line_tolerance` vertical pixels)
3. Pick the closest box within `max_distance`
4. If `region` is specified, the value box center must be within [x1,y1,x2,y2]

**`same_line_tolerance`**: In normalized 0–1000 units. A value of 20 means the vertical center of the value box can differ from the anchor center by up to 20 units. Increase for skewed documents.

**`max_distance`**: Euclidean distance between box centers. A value of 250 in 0–1000 space ≈ 25% of the page width. Decrease for tighter layout, increase for sparse labels.

### Anchor-Below Field

```jsonc
"value": {
  "anchors": ["结果"],
  "search_direction": "below",
  "max_distance": 200
}
```

Same as Anchor-Right, but searches **below** the anchor box. Horizontal alignment is checked within `max_distance × 2`.

### Paragraph Between Anchors

```jsonc
"findings": {
  "start_anchors": ["检查所见", "影像所见", "超声所见"],
  "end_anchors": ["诊断意见", "检查结论", "超声提示"],
  "extraction_type": "paragraph_between_anchors",
  "data_type": "text"
}
```

**How it works:**
1. Find the **start anchor** box (e.g., "检查所见：")
2. Find the **end anchor** box that appears **below** the start anchor (e.g., "诊断意见：")
3. Collect all text boxes whose vertical center is between the start and end anchors
4. Concatenate text with newline separators
5. Sort by vertical position (top-to-bottom reading order)

**Important**: The end anchor must appear **below** the start anchor on the page. If multiple end anchors match, the closest one below the start is used.

---

## Data Types and Validation

| `data_type` | Output format | Validation applied |
|-------------|--------------|--------------------|
| `string` | Raw text, trimmed | None |
| `text` | Multi-line text | None |
| `date` | `"YYYY-MM-DD"` | Regex → `DateValidator::Normalize()`. Accepts Chinese (2026年7月20日), ISO (2026-07-20), slash (2026/07/20), 8-digit compact (20260720) |
| `age` | Integer (string) | `FieldNormalizer::ExtractAge()`. Accepts "37岁", "37", "37Y". Range 0–150. |
| `gender` | `"男"` / `"女"` | `FieldNormalizer::NormalizeGender()`. Accepts: 男/女, Male/Female, M/F, 1/0/2. |
| `id_number` | 18-digit string | `IdCardValidator::Validate()`. GB 11643-1999 checksum. OCR correction (O→0, I→1等). `checksum_valid` in output. |

---

## Field Fallback Strategy

When a field is not found using its primary extraction method:

1. `anchor_right` → try `anchor_below` (same anchors, same max_distance)
2. `anchor_below` → try `anchor_right` (same anchors, same same_line_tolerance)
3. If still not found → field is omitted from output; warning is logged

This handles cases where the label is on the same line in one report but above the value in another.

---

## Cross-Validation

After all fields are extracted, the following consistency checks run:

| Check | Conflict handling |
|-------|-------------------|
| `birth_date` vs ID number (positions 7–14) | Warning logged; **neither value changed** |
| `gender` vs ID number (digit 17 parity) | Warning logged; **neither value changed** |
| `age` vs `birth_date` vs current date | Warning logged if age differs by > 1 year |

**Principle**: Cross-validation never overwrites extracted values. Conflicts are reported in `warnings[]` for caller resolution.

---

## Coordinate Normalization Formula

```
normalized_x = pixel_x * 1000 / image_width
normalized_y = pixel_y * 1000 / image_height
```

All template coordinates (`region`, `max_distance`, `same_line_tolerance`) are in 0–1000 units.

---

## Complete Template Example

See `config/templates/sample_template.json` for a full working template covering all field types.

---

## Template Debugging Tips

1. **Template not matching**: Enable `enable_raw_text_log: true` in config. Check that `required_keywords` all appear in the OCR text. OCR may misread characters — use shorter substrings (e.g., "超声" not "超声检查报告").

2. **Field not extracted**: The anchor keyword must **exactly match** a substring of an OCR text box. Check for OCR errors: "姓名" may be read as "姓 名" (with space) and won't match.

3. **Wrong field value**: Check `same_line_tolerance` — increase if the anchor and value are on slightly different lines. Check `max_distance` — increase if the value is far from the anchor.

4. **Paragraph missing content**: Ensure both start and end anchors are found. The end anchor must appear **physically below** the start anchor on the page.

5. **Multiple templates match**: The one with the highest score wins. Increase `hospital_keywords` weight by adding more unique hospital-specific terms.
