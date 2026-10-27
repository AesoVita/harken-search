// Standalone test: read cover thumbs from auro_blobs.db, decode via WIC, compute dHash
// Build: cl /EHsc /utf-8 /I../thirdparty /I../thirdparty/sqlite test_cover_dhash.cpp ../thirdparty/sqlite/sqlite3.c ole32.lib windowscodecs.lib
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#include <objbase.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cmath>
#include "sqlite3.h"

static bool DecodeImageWIC(const uint8_t* data, size_t dataSize, std::vector<uint8_t>& pixels, int& w, int& h) {
    if (!data || dataSize == 0) return false;
    IWICImagingFactory* pFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) { printf("  CoCreateInstance FAIL hr=0x%08lX\n", (unsigned long)hr); return false; }

    IWICStream* pStream = nullptr;
    IWICBitmapDecoder* pDecoder = nullptr;
    IWICBitmapFrameDecode* pFrame = nullptr;
    IWICFormatConverter* pConverter = nullptr;
    bool success = false;

    hr = pFactory->CreateStream(&pStream);
    if (SUCCEEDED(hr)) hr = pStream->InitializeFromMemory(const_cast<BYTE*>(data), (DWORD)dataSize);
    if (FAILED(hr)) { printf("  Stream init FAIL hr=0x%08lX\n", (unsigned long)hr); goto cleanup; }

    hr = pFactory->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr)) { printf("  CreateDecoder FAIL hr=0x%08lX\n", (unsigned long)hr); goto cleanup; }

    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) { printf("  GetFrame FAIL hr=0x%08lX\n", (unsigned long)hr); goto cleanup; }

    hr = pFactory->CreateFormatConverter(&pConverter);
    if (SUCCEEDED(hr)) {
        hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    }
    if (FAILED(hr)) { printf("  Converter FAIL hr=0x%08lX\n", (unsigned long)hr); goto cleanup; }

    {
        UINT uw = 0, uh = 0;
        hr = pConverter->GetSize(&uw, &uh);
        if (SUCCEEDED(hr) && uw > 0 && uh > 0) {
            w = (int)uw; h = (int)uh;
            pixels.resize((size_t)uw * uh * 4);
            hr = pConverter->CopyPixels(nullptr, uw * 4, (UINT)pixels.size(), pixels.data());
            success = SUCCEEDED(hr);
            if (!success) printf("  CopyPixels FAIL hr=0x%08lX\n", (unsigned long)hr);
        } else {
            printf("  GetSize FAIL hr=0x%08lX w=%u h=%u\n", (unsigned long)hr, uw, uh);
        }
    }

cleanup:
    if (pConverter) pConverter->Release();
    if (pFrame) pFrame->Release();
    if (pDecoder) pDecoder->Release();
    if (pStream) pStream->Release();
    if (pFactory) pFactory->Release();
    return success;
}

static void DownsampleToGray9x8_Box(const uint8_t* pixels, int w, int h, float out[72]) {
    for (int row = 0; row < 8; ++row) {
        int y0 = row * h / 8, y1 = (row + 1) * h / 8;
        if (y1 <= y0) y1 = y0 + 1; if (y1 > h) y1 = h;
        for (int col = 0; col < 9; ++col) {
            int x0 = col * w / 9, x1 = (col + 1) * w / 9;
            if (x1 <= x0) x1 = x0 + 1; if (x1 > w) x1 = w;
            double sum = 0; int cnt = 0;
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) {
                    size_t i = ((size_t)y * w + x) * 4;
                    sum += 0.299 * pixels[i] + 0.587 * pixels[i+1] + 0.114 * pixels[i+2];
                    ++cnt;
                }
            out[row * 9 + col] = cnt > 0 ? (float)(sum / cnt) : 0.0f;
        }
    }
}

static uint64_t ComputeDHash64(const float g[72]) {
    uint64_t h = 0;
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            if (g[r*9+c] > g[r*9+c+1]) h |= (1ULL << (r*8+c));
    return h;
}

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    const char* blobsPath = "C:\\Users\\User\\AppData\\Local\\Auro\\auro_blobs.db";
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(blobsPath, &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        printf("FAIL: cannot open %s: %s\n", blobsPath, sqlite3_errmsg(db));
        return 1;
    }

    // Sample from different parts of the table (not just first 10 which may all be old corrupt data)
    const char* sql = "SELECT cover_key, thumb_data, mime FROM cover_thumbs ORDER BY RANDOM() LIMIT 10";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        printf("FAIL: prepare: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    int tested = 0, decoded = 0, hashed = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* key = (const char*)sqlite3_column_text(stmt, 0);
        const uint8_t* blob = (const uint8_t*)sqlite3_column_blob(stmt, 1);
        int blobSize = sqlite3_column_bytes(stmt, 1);
        const char* mime = (const char*)sqlite3_column_text(stmt, 2);

        // Dump first 3 blobs to files for manual inspection
        if (tested < 3) {
            char fname[64];
            snprintf(fname, sizeof(fname), "thumb_dump_%d.bin", tested);
            FILE* df = fopen(fname, "wb");
            if (df) { fwrite(blob, 1, blobSize, df); fclose(df); printf("  Dumped %d bytes to %s\n", blobSize, fname); }
        }

        printf("\n[%d] key=%.60s sz=%d mime=%s hdr=%02X%02X%02X%02X\n",
            tested, key ? key : "?", blobSize, mime ? mime : "?",
            blobSize >= 4 ? blob[0] : 0, blobSize >= 4 ? blob[1] : 0,
            blobSize >= 4 ? blob[2] : 0, blobSize >= 4 ? blob[3] : 0);

        std::vector<uint8_t> pixels;
        int w = 0, h = 0;
        if (DecodeImageWIC(blob, blobSize, pixels, w, h)) {
            decoded++;
            printf("  Decoded: %dx%d (%d bytes RGBA)\n", w, h, (int)pixels.size());

            float gray[72];
            DownsampleToGray9x8_Box(pixels.data(), w, h, gray);
            uint64_t hash = ComputeDHash64(gray);
            printf("  dHash: 0x%016llX\n", (unsigned long long)hash);
            printf("  gray[0..8]: ");
            for (int i = 0; i < 9; i++) printf("%.1f ", gray[i]);
            printf("\n");
            if (hash != 0) hashed++;
        } else {
            printf("  DECODE FAILED\n");
        }
        tested++;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    CoUninitialize();

    printf("\n=== Result: tested=%d decoded=%d hashed=%d ===\n", tested, decoded, hashed);
    return (hashed > 0) ? 0 : 1;
}
