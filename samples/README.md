# Sample Data

This directory contains sample data for testing the Medical OCR pipeline
without real images or PaddleOCR.

## mock_ocr_result.json

A pre-recorded OCR result simulating what PaddleOCR would produce for a
typical Chinese hospital ultrasound report.

### Usage with MockOcrEngine

To use this file, set the `mock_data_path` in your engine config:

```json
{
  "engine": "mock",
  "mock_data_path": "samples/mock_ocr_result.json"
}
```

The MockOcrEngine will load these pre-recorded boxes instead of generating
built-in sample data.

### Format

```jsonc
{
  "image_width": 2480,    // Width of the source image
  "image_height": 3508,   // Height of the source image
  "boxes": [
    {
      "text": "OCR text",       // Recognized text (UTF-8)
      "confidence": 0.99,       // Confidence [0.0, 1.0]
      "direction": 0.0,         // Text direction in degrees
      "points": [               // 4 corners: TL, TR, BR, BL
        {"x": 100, "y": 50},
        {"x": 200, "y": 50},
        {"x": 200, "y": 80},
        {"x": 100, "y": 80}
      ]
    }
  ]
}
```

### Creating Custom Mock Data

1. Run a real image through a PaddleOCR or similar OCR system
2. Save the detected text boxes as JSON in this format
3. Place the file here and point `mock_data_path` to it

This allows you to test the extraction pipeline with realistic data
without having PaddleOCR installed.
