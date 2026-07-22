# Template Format Specification

## Overview

Hospital report templates are JSON files that define how to extract fields
from OCR results. Templates use **normalized coordinates** (0–1000) rather
than pixel coordinates, ensuring resolution independence.

## Template Structure

```jsonc
{
  "template_id": "unique_template_identifier",
  "document_type": "ultrasound_report",
  "description": "Optional human-readable description",
  "hospital_keywords": ["keyword1", "keyword2"],
  "title_keywords": ["title1"],
  "required_keywords": ["must have 1", "must have 2"],
  "optional_keywords": ["nice to have"],
  "fields": {
    "field_name": { /* field definition */ }
  }
}
```

### Top-Level Fields

| Field | Type | Description |
|-------|------|-------------|
| `template_id` | string | Unique identifier for this template |
| `document_type` | string | One of: ultrasound_report, ct_report, mri_report, xray_report, laboratory_report |
| `hospital_keywords` | string[] | Keywords that identify this hospital |
| `title_keywords` | string[] | Keywords found in the report title |
| `required_keywords` | string[] | Must ALL be present to match this template |
| `optional_keywords` | string[] | Helpful but not required for matching |
| `fields` | object | Field extraction definitions |

## Field Definitions

### Anchor-Right Field

Finds a text box to the right of an anchor keyword:

```jsonc
"name": {
  "anchors": ["姓名", "患者姓名", "Name"],
  "search_direction": "right",
  "same_line_tolerance": 20,
  "max_distance": 250,
  "region": [0, 0, 1000, 400],
  "data_type": "string"
}
```

| Parameter | Description |
|-----------|-------------|
| `anchors` | Keywords to find in the OCR result |
| `search_direction` | "right" or "below" |
| `same_line_tolerance` | Vertical tolerance in normalized units for "same line" |
| `max_distance` | Maximum distance from anchor to field |
| `region` | Optional [x1, y1, x2, y2] in 0–1000 normalized space |
| `data_type` | Expected data type: string, date, age, gender, id_number |

### Anchor-Below Field

Finds text below an anchor keyword:

```jsonc
"value": {
  "anchors": ["结果"],
  "search_direction": "below",
  "same_line_tolerance": 50,
  "max_distance": 200
}
```

### Paragraph Between Anchors

Captures all text between a start anchor and an end anchor:

```jsonc
"findings": {
  "start_anchors": ["检查所见", "影像所见"],
  "end_anchors": ["诊断意见", "检查结论"],
  "extraction_type": "paragraph_between_anchors",
  "data_type": "text"
}
```

## Data Types

| Type | Description | Validation |
|------|-------------|------------|
| `string` | Free text | None |
| `text` | Multi-line text block | None |
| `date` | Date value | Normalized to YYYY-MM-DD |
| `age` | Age value | Range 0–150 |
| `gender` | Gender value | Normalized to 男/女/其他 |
| `id_number` | Chinese ID number | Checksum validated |

## Coordinate Normalization

All coordinates are normalized to fit within a 0–1000 square. This ensures
that the same template works regardless of whether the input image is 300 DPI
or 150 DPI, A4 or A5.

The normalization formula:
```
normalized_x = pixel_x * 1000 / image_width
normalized_y = pixel_y * 1000 / image_height
```

## Matching Algorithm

1. All `required_keywords` must appear in the OCR text
2. At least one `title_keyword` must appear
3. Template with the most matching keywords wins
4. If no template matches, `document_type` is "unknown"

## Example

See `config/templates/sample_template.json` for a complete example.
