# Model Files Directory

This directory is reserved for OCR model files.

## Offline Models Required

When using the **PaddleOcrEngine** (Phase 4+), you need to place the following
PaddleOCR inference model files here:

### Detection Model
- `ch_PP-OCRv4_det_infer/`
  - `inference.pdmodel`
  - `inference.pdiparams`

### Recognition Model
- `ch_PP-OCRv4_rec_infer/`
  - `inference.pdmodel`
  - `inference.pdiparams`

### Dictionary
- `ppocr_keys_v1.txt`

## Download Instructions

1. Visit the PaddleOCR GitHub releases page:
   https://github.com/PaddlePaddle/PaddleOCR

2. Download the Chinese PP-OCRv4 inference models.

3. Extract and place the files as shown above.

## Current Status (Phase 1)

The MockOcrEngine is the default and does not require any model files.
This directory is empty in Phase 1 and will be populated when PaddleOCR
integration is added in Phase 4.

## Security Note

Model files are loaded from the local filesystem only.
No network access is required or performed during model loading.
