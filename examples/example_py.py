"""
Medical OCR — Python Integration Example

Uses ctypes to call the native MedicalOCR.dll.

Requirements:
  - MedicalOCR.dll in PATH or next to this script
  - config/medical_ocr.json in working directory

Usage:
  python example_py.py [image_path] [config_path]
"""

import ctypes
import json
import os
import sys

# ============================================================================
# ctypes bindings
# ============================================================================

# Load the DLL.
_dll_path = os.path.join(os.path.dirname(__file__), "MedicalOCR.dll")
if not os.path.exists(_dll_path):
    _dll_path = "MedicalOCR.dll"  # Search PATH.
_ocr = ctypes.CDLL(_dll_path)

# ---- Function signatures ----

# int OCR_Init(const char* model_directory, const char* config_path)
_ocr.OCR_Init.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
_ocr.OCR_Init.restype = ctypes.c_int

# int OCR_RecognizeFile(const char* image_path, char** output_json)
_ocr.OCR_RecognizeFile.argtypes = [ctypes.c_char_p,
                                    ctypes.POINTER(ctypes.c_char_p)]
_ocr.OCR_RecognizeFile.restype = ctypes.c_int

# int OCR_RecognizeMemory(const unsigned char* data, int size, char** output_json)
_ocr.OCR_RecognizeMemory.argtypes = [ctypes.POINTER(ctypes.c_ubyte),
                                      ctypes.c_int,
                                      ctypes.POINTER(ctypes.c_char_p)]
_ocr.OCR_RecognizeMemory.restype = ctypes.c_int

# void OCR_FreeResult(char* json)
_ocr.OCR_FreeResult.argtypes = [ctypes.c_char_p]
_ocr.OCR_FreeResult.restype = None

# const char* OCR_GetVersion()
_ocr.OCR_GetVersion.argtypes = []
_ocr.OCR_GetVersion.restype = ctypes.c_char_p

# const char* OCR_GetLastError()
_ocr.OCR_GetLastError.argtypes = []
_ocr.OCR_GetLastError.restype = ctypes.c_char_p

# void OCR_Shutdown()
_ocr.OCR_Shutdown.argtypes = []
_ocr.OCR_Shutdown.restype = None

# ============================================================================
# Pythonic wrapper
# ============================================================================


class MedicalOcr:
    """RAII-style wrapper for the Medical OCR DLL."""

    def __init__(self, config_path=None):
        self._initialized = False
        rc = _ocr.OCR_Init(None, self._to_cstr(config_path))
        if rc != 0:
            err = self._get_error()
            raise RuntimeError(f"OCR_Init failed (code {rc}): {err}")
        self._initialized = True

    @property
    def version(self):
        return _ocr.OCR_GetVersion().decode("utf-8")

    def recognize_file(self, image_path):
        if not self._initialized:
            raise RuntimeError("Not initialized")

        output = ctypes.c_char_p()
        rc = _ocr.OCR_RecognizeFile(
            image_path.encode("utf-8"),
            ctypes.byref(output))
        if rc != 0:
            err = self._get_error()
            raise RuntimeError(f"OCR_RecognizeFile failed (code {rc}): {err}")

        result = output.value.decode("utf-8") if output.value else "{}"
        _ocr.OCR_FreeResult(output)
        return result

    def recognize_memory(self, image_bytes):
        if not self._initialized:
            raise RuntimeError("Not initialized")

        data = (ctypes.c_ubyte * len(image_bytes))(*image_bytes)
        output = ctypes.c_char_p()
        rc = _ocr.OCR_RecognizeMemory(
            data, len(image_bytes),
            ctypes.byref(output))
        if rc != 0:
            err = self._get_error()
            raise RuntimeError(f"OCR_RecognizeMemory failed (code {rc}): {err}")

        result = output.value.decode("utf-8") if output.value else "{}"
        _ocr.OCR_FreeResult(output)
        return result

    def close(self):
        if self._initialized:
            _ocr.OCR_Shutdown()
            self._initialized = False

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    @staticmethod
    def _get_error():
        err = _ocr.OCR_GetLastError()
        return err.decode("utf-8") if err else "Unknown"

    @staticmethod
    def _to_cstr(s):
        return s.encode("utf-8") if s else None


# ============================================================================
# Main
# ============================================================================

def main():
    image_path = sys.argv[1] if len(sys.argv) > 1 else "sample_report.jpg"
    config_path = sys.argv[2] if len(sys.argv) > 2 else "config/medical_ocr.json"

    print("Medical OCR Python Example")
    print("==========================")

    with MedicalOcr(config_path) as ocr:
        print(f"Version: {ocr.version}")
        print(f"Recognizing: {image_path}")

        result_json = ocr.recognize_file(image_path)
        result = json.loads(result_json)

        print(f"Success: {result['success']}")
        if result["success"]:
            print(f"Document type: {result['document']['type']}")
            print(f"Blur score: {result['quality']['blur_score']:.1f}")
            if "patient" in result:
                name = result["patient"].get("name", {}).get("value", "N/A")
                print(f"Patient: {name}")

    print("Done.")


if __name__ == "__main__":
    main()
