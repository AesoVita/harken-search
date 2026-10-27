#include "SameFolderScanRuntime.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++g_failures;
}

std::string ToUtf8(const fs::path& path) {
    return path.u8string();
}

bool CollectUntilDone(aura::same_folder::SameFolderScanRuntime& runtime,
                      uint64_t generation,
                      std::vector<std::string>& paths,
                      std::string& error,
                      int& dataChunks,
                      size_t& largestChunk) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        aura::same_folder::SameFolderScanChunk chunk;
        while (runtime.tryPopChunk(chunk)) {
            if (chunk.generation != generation) continue;
            if (!chunk.filePaths.empty()) {
                ++dataChunks;
                largestChunk = std::max(largestChunk, chunk.filePaths.size());
                paths.insert(paths.end(), chunk.filePaths.begin(),
                             chunk.filePaths.end());
            }
            if (chunk.done) {
                error = chunk.error;
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

class TemporaryTree {
public:
    TemporaryTree() {
        const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
        root_ = fs::temp_directory_path() /
                ("aris_same_folder_runtime_" + std::to_string(stamp));
        fs::create_directories(root_ / "nested");
    }

    ~TemporaryTree() {
        std::error_code error;
        fs::remove_all(root_, error);
    }

    const fs::path& root() const { return root_; }

    void WriteFile(const fs::path& path) {
        std::ofstream output(path, std::ios::binary);
        output << "test";
    }

private:
    fs::path root_;
};

} // namespace

int main() {
    TemporaryTree tree;
    const std::vector<std::string> extensions = {
        ".wav", ".WAV", ".mp3", ".FLAC"};
    for (int index = 0; index < 130; ++index) {
        tree.WriteFile(tree.root() /
                       ("audio_" + std::to_string(index) +
                        extensions[static_cast<size_t>(index) % extensions.size()]));
    }
    tree.WriteFile(tree.root() / "ignore.txt");
    tree.WriteFile(tree.root() / "nested" / "nested.wav");

    aura::same_folder::SameFolderScanRuntime runtime;
    aura::same_folder::SameFolderScanChunk emptyChunk;
    Expect(!runtime.tryPopChunk(emptyChunk),
           "polling before submission must be nonblocking and empty");

    aura::same_folder::SameFolderScanRequest validRequest;
    validRequest.generation = 11;
    validRequest.folderPathUtf8 = ToUtf8(tree.root());
    validRequest.missingDirectoryErrorUtf8 = "missing-directory";
    runtime.submitLatest(validRequest);

    std::vector<std::string> paths;
    std::string error;
    int dataChunks = 0;
    size_t largestChunk = 0;
    Expect(CollectUntilDone(runtime, validRequest.generation, paths, error,
                            dataChunks, largestChunk),
           "valid folder scan must publish a terminal chunk before deadline");
    Expect(error.empty(), "valid folder scan must not publish an error");
    Expect(paths.size() == 130,
           "valid folder scan must publish all regular audio files only");
    Expect(dataChunks == 2, "130 audio files must be split into two data chunks");
    Expect(largestChunk <= 128, "data chunks must contain at most 128 paths");
    Expect(std::set<std::string>(paths.begin(), paths.end()).size() == 130,
           "published audio paths must be unique");
    Expect(std::none_of(paths.begin(), paths.end(), [](const std::string& path) {
               return path.find("ignore.txt") != std::string::npos ||
                      path.find("nested.wav") != std::string::npos;
           }),
           "non-audio and nested files must not be published");

    aura::same_folder::SameFolderScanRequest missingRequest;
    missingRequest.generation = 12;
    missingRequest.folderPathUtf8 =
        ToUtf8(tree.root() / "does-not-exist");
    missingRequest.missingDirectoryErrorUtf8 = "missing-directory";
    runtime.submitLatest(missingRequest);

    paths.clear();
    error.clear();
    dataChunks = 0;
    largestChunk = 0;
    Expect(CollectUntilDone(runtime, missingRequest.generation, paths, error,
                            dataChunks, largestChunk),
           "missing folder scan must publish a terminal chunk before deadline");
    Expect(paths.empty(), "missing folder scan must not publish paths");
    Expect(error == "missing-directory",
           "missing folder scan must preserve the request-provided error");

    runtime.stop();
    runtime.stop();

    {
        aura::same_folder::SameFolderScanRuntime scopedRuntime;
        aura::same_folder::SameFolderScanRequest request;
        request.generation = 21;
        request.folderPathUtf8 = ToUtf8(tree.root());
        request.missingDirectoryErrorUtf8 = "missing-directory";
        scopedRuntime.submitLatest(request);
    }

    if (g_failures != 0) return 1;
    std::cout << "PASS: same folder scan runtime contracts\n";
    return 0;
}
