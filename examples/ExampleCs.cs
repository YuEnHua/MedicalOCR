/**
 * Medical OCR — C# Integration Example
 *
 * Uses P/Invoke to call the native MedicalOCR.dll.
 *
 * Build:
 *   csc ExampleCs.cs /platform:x64
 *
 * Run:
 *   ExampleCs.exe sample_report.jpg config\medical_ocr.json
 */

using System;
using System.Runtime.InteropServices;
using System.Text;

public class MedicalOcr
{
    // ---- P/Invoke declarations ----
    [DllImport("MedicalOCR.dll", CallingConvention = CallingConvention.Cdecl,
               CharSet = CharSet.Ansi)]
    private static extern int OCR_Init(
        [MarshalAs(UnmanagedType.LPStr)] string modelDirectory,
        [MarshalAs(UnmanagedType.LPStr)] string configPath);

    [DllImport("MedicalOCR.dll", CallingConvention = CallingConvention.Cdecl,
               CharSet = CharSet.Ansi)]
    private static extern int OCR_RecognizeFile(
        [MarshalAs(UnmanagedType.LPStr)] string imagePath,
        out IntPtr outputJson);

    [DllImport("MedicalOCR.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int OCR_RecognizeMemory(
        byte[] imageData, int imageSize, out IntPtr outputJson);

    [DllImport("MedicalOCR.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void OCR_FreeResult(IntPtr json);

    [DllImport("MedicalOCR.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr OCR_GetVersion();

    [DllImport("MedicalOCR.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr OCR_GetLastError();

    [DllImport("MedicalOCR.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void OCR_Shutdown();

    // ---- Managed wrapper ----
    private bool _initialized = false;

    public string Version
    {
        get
        {
            IntPtr ptr = OCR_GetVersion();
            return Marshal.PtrToStringAnsi(ptr);
        }
    }

    public void Initialize(string configPath = null)
    {
        int rc = OCR_Init(null, configPath);
        if (rc != 0)
        {
            IntPtr errPtr = OCR_GetLastError();
            string err = Marshal.PtrToStringAnsi(errPtr);
            throw new InvalidOperationException($"OCR_Init failed (code {rc}): {err}");
        }
        _initialized = true;
    }

    public string RecognizeFile(string imagePath)
    {
        if (!_initialized) throw new InvalidOperationException("Not initialized");

        int rc = OCR_RecognizeFile(imagePath, out IntPtr jsonPtr);
        if (rc != 0)
        {
            IntPtr errPtr = OCR_GetLastError();
            string err = Marshal.PtrToStringAnsi(errPtr);
            throw new InvalidOperationException($"OCR_RecognizeFile failed (code {rc}): {err}");
        }

        string result = Marshal.PtrToStringAnsi(jsonPtr);
        OCR_FreeResult(jsonPtr);
        return result;
    }

    public void Shutdown()
    {
        if (_initialized)
        {
            OCR_Shutdown();
            _initialized = false;
        }
    }

    // ---- Entry point ----
    public static void Main(string[] args)
    {
        string imagePath = args.Length > 0 ? args[0] : "sample_report.jpg";
        string configPath = args.Length > 1 ? args[1] : "config/medical_ocr.json";

        var ocr = new MedicalOcr();
        try
        {
            Console.WriteLine($"Medical OCR C# Example");
            Console.WriteLine($"======================");
            Console.WriteLine($"Version: {ocr.Version}\n");

            ocr.Initialize(configPath);
            Console.WriteLine("Initialized.");

            string json = ocr.RecognizeFile(imagePath);
            Console.WriteLine($"Result:\n{json}");
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Error: {ex.Message}");
            Environment.Exit(1);
        }
        finally
        {
            ocr.Shutdown();
        }

        Console.WriteLine("Done.");
    }
}
