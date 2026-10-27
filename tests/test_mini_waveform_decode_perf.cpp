#include "WaveformExtractor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kSyntheticMinFilesPerSecond = 20.0;

struct PerfResult {
    int files = 0;
    int ok = 0;
    int failed = 0;
    double elapsedSeconds = 0.0;
    double filesPerSecond = 0.0;
    double audioSeconds = 0.0;
    double audioSecondsPerWallSecond = 0.0;
};

static void writeLe16(std::ofstream& out, uint16_t v) {
    out.put(static_cast<char>(v & 0xff));
    out.put(static_cast<char>((v >> 8) & 0xff));
}

static void writeLe32(std::ofstream& out, uint32_t v) {
    out.put(static_cast<char>(v & 0xff));
    out.put(static_cast<char>((v >> 8) & 0xff));
    out.put(static_cast<char>((v >> 16) & 0xff));
    out.put(static_cast<char>((v >> 24) & 0xff));
}

static bool writePcm16Wav(const fs::path& path,
                          double durationSeconds,
                          int sampleRate,
                          int channels,
                          double frequency) {
    const uint32_t frames = static_cast<uint32_t>(durationSeconds * sampleRate);
    const uint16_t bitsPerSample = 16;
    const uint16_t blockAlign = static_cast<uint16_t>(channels * bitsPerSample / 8);
    const uint32_t byteRate = sampleRate * blockAlign;
    const uint32_t dataBytes = frames * blockAlign;

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }

    out.write("RIFF", 4);
    writeLe32(out, 36u + dataBytes);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    writeLe32(out, 16);
    writeLe16(out, 1);
    writeLe16(out, static_cast<uint16_t>(channels));
    writeLe32(out, static_cast<uint32_t>(sampleRate));
    writeLe32(out, byteRate);
    writeLe16(out, blockAlign);
    writeLe16(out, bitsPerSample);
    out.write("data", 4);
    writeLe32(out, dataBytes);

    for (uint32_t frame = 0; frame < frames; ++frame) {
        const double t = static_cast<double>(frame) / static_cast<double>(sampleRate);
        for (int ch = 0; ch < channels; ++ch) {
            const double phase = frequency + static_cast<double>(ch) * 37.0;
            const double sample = std::sin(2.0 * kPi * phase * t) * 0.45;
            const auto pcm = static_cast<int16_t>(std::clamp(sample, -1.0, 1.0) * 32767.0);
            writeLe16(out, static_cast<uint16_t>(pcm));
        }
    }
    return true;
}

#ifdef _WIN32
static std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), size, nullptr, nullptr);
    return out;
}

static std::vector<std::wstring> commandLineArgs() {
    int argc = 0;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    std::vector<std::wstring> args;
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    if (argv) {
        ::LocalFree(argv);
    }
    return args;
}
#endif

static std::string pathToUtf8(const fs::path& path) {
#ifdef _WIN32
    return wideToUtf8(path.wstring());
#else
    return path.u8string();
#endif
}

static std::string lowerExt(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

static bool isAudioFile(const fs::path& path) {
    const std::string ext = lowerExt(path);
    return ext == ".wav" || ext == ".wave" || ext == ".aif" || ext == ".aiff" ||
           ext == ".flac" || ext == ".mp3" || ext == ".ogg" || ext == ".m4a";
}

static std::vector<fs::path> collectAudioFiles(const fs::path& root, int limit) {
    std::vector<fs::path> files;
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    while (!ec && it != end) {
        if (it->is_regular_file(ec) && !ec && isAudioFile(it->path())) {
            files.push_back(it->path());
            if (limit > 0 && static_cast<int>(files.size()) >= limit) {
                break;
            }
        }
        it.increment(ec);
    }
    return files;
}

static fs::path timestampedReportDir(const std::string& prefix) {
    std::time_t now = std::time(nullptr);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &now);
#else
    localtime_r(&now, &tmBuf);
#endif
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tmBuf);
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path dir = fs::path(".openclaw") / "reports" /
                   (prefix + "_" + ts + "_" + std::to_string(unique));
    fs::create_directories(dir);
    return dir;
}

static std::vector<fs::path> createSyntheticFixtures(const fs::path& dir) {
    fs::create_directories(dir);
    std::vector<fs::path> files;
    const double durations[] = {0.25, 0.5, 1.5, 3.0};
    int index = 0;
    for (double duration : durations) {
        for (int i = 0; i < 16; ++i) {
            fs::path path = dir / ("mini_perf_" + std::to_string(index++) + ".wav");
            if (writePcm16Wav(path, duration, 48000, 2, 180.0 + i * 11.0)) {
                files.push_back(path);
            }
        }
    }
    return files;
}

static PerfResult runPerf(const std::vector<fs::path>& files, int targetPeaks) {
    PerfResult result;
    result.files = static_cast<int>(files.size());
    const auto started = std::chrono::steady_clock::now();
    for (const fs::path& file : files) {
        WaveformExtractResult waveform;
        if (WaveformExtractor::ExtractMini(pathToUtf8(file), targetPeaks, waveform, false) &&
            !waveform.peaks.empty() &&
            waveform.sampleRate > 0) {
            ++result.ok;
            const int channels = std::max(1, waveform.channels);
            const uint64_t estimatedFrames = waveform.decodedFrames > 0
                ? waveform.decodedFrames
                : static_cast<uint64_t>(std::max(1, waveform.samplesPerPeak)) *
                      static_cast<uint64_t>(waveform.peaks.size() / static_cast<size_t>(channels));
            result.audioSeconds += static_cast<double>(estimatedFrames) /
                                   static_cast<double>(waveform.sampleRate);
        } else {
            ++result.failed;
            std::fprintf(stderr, "mini waveform decode failed: %s (%s)\n",
                         pathToUtf8(file).c_str(),
                         waveform.errorCode.c_str());
        }
    }
    result.elapsedSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    if (result.elapsedSeconds > 0.0) {
        result.filesPerSecond = static_cast<double>(result.ok) / result.elapsedSeconds;
        result.audioSecondsPerWallSecond = result.audioSeconds / result.elapsedSeconds;
    }
    return result;
}

static void writeReport(const fs::path& reportPath,
                        const std::string& mode,
                        const fs::path& root,
                        const PerfResult& result,
                        int limit,
                        int targetPeaks) {
    std::ofstream out(reportPath);
    out << "{\n";
    out << "  \"mode\": \"" << mode << "\",\n";
    out << "  \"root\": \"" << pathToUtf8(root) << "\",\n";
    out << "  \"limit\": " << limit << ",\n";
    out << "  \"target_peaks\": " << targetPeaks << ",\n";
    out << "  \"files\": " << result.files << ",\n";
    out << "  \"ok\": " << result.ok << ",\n";
    out << "  \"failed\": " << result.failed << ",\n";
    out << "  \"elapsed_seconds\": " << std::fixed << std::setprecision(4) << result.elapsedSeconds << ",\n";
    out << "  \"files_per_second\": " << std::fixed << std::setprecision(2) << result.filesPerSecond << ",\n";
    out << "  \"audio_seconds\": " << std::fixed << std::setprecision(2) << result.audioSeconds << ",\n";
    out << "  \"audio_seconds_per_wall_second\": " << std::fixed << std::setprecision(2)
        << result.audioSecondsPerWallSecond << "\n";
    out << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    fs::path realRoot;
    int limit = 200;
    int targetPeaks = 160;

#ifdef _WIN32
    const auto args = commandLineArgs();
    if (args.size() >= 2) {
        realRoot = fs::path(args[1]);
    }
    if (args.size() >= 3) {
        limit = std::max(0, std::stoi(args[2]));
    }
    if (args.size() >= 4) {
        targetPeaks = std::max(1, std::stoi(args[3]));
    }
#else
    if (argc >= 2) {
        realRoot = fs::path(argv[1]);
    }
    if (argc >= 3) {
        limit = std::max(0, std::stoi(argv[2]));
    }
    if (argc >= 4) {
        targetPeaks = std::max(1, std::stoi(argv[3]));
    }
#endif
    (void)argc;
    (void)argv;

    const bool syntheticMode = realRoot.empty();
    fs::path reportDir = timestampedReportDir(syntheticMode ? "mini_waveform_perf_synthetic"
                                                            : "mini_waveform_perf_real");
    fs::path root = realRoot;
    std::vector<fs::path> files;
    std::string mode = "real";

    if (syntheticMode) {
        mode = "synthetic";
        root = reportDir / "fixtures";
        files = createSyntheticFixtures(root);
        limit = static_cast<int>(files.size());
    } else {
        if (!fs::exists(root)) {
            std::fprintf(stderr, "root does not exist: %s\n", pathToUtf8(root).c_str());
            return 1;
        }
        files = collectAudioFiles(root, limit);
    }

    if (files.empty()) {
        std::fprintf(stderr, "no audio files found: %s\n", pathToUtf8(root).c_str());
        return 1;
    }

    const PerfResult result = runPerf(files, targetPeaks);
    const fs::path reportPath = reportDir / "mini_waveform_decode_perf.json";
    writeReport(reportPath, mode, root, result, limit, targetPeaks);

    std::printf("mode=%s targetPeaks=%d files=%d ok=%d failed=%d elapsed=%.3fs filesPerSec=%.2f audioSec=%.2f audioSecPerWallSec=%.2f\n",
                mode.c_str(),
                targetPeaks,
                result.files,
                result.ok,
                result.failed,
                result.elapsedSeconds,
                result.filesPerSecond,
                result.audioSeconds,
                result.audioSecondsPerWallSecond);
    std::printf("report=%s\n", pathToUtf8(reportPath).c_str());

    if (result.ok == 0 || result.failed > 0) {
        return 1;
    }
    if (syntheticMode && result.filesPerSecond < kSyntheticMinFilesPerSecond) {
        std::fprintf(stderr,
                     "synthetic mini waveform throughput below floor: %.2f < %.2f files/sec\n",
                     result.filesPerSecond,
                     kSyntheticMinFilesPerSecond);
        return 1;
    }
    if (!syntheticMode && result.filesPerSecond < kSyntheticMinFilesPerSecond) {
        std::fprintf(stderr,
                     "WARN: real-folder mini waveform throughput below synthetic floor: %.2f < %.2f files/sec\n",
                     result.filesPerSecond,
                     kSyntheticMinFilesPerSecond);
    }
    return 0;
}
