#include "CoverModule.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int g_failed = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        std::cout << "PASS: " << message << "\n";
    } else {
        std::cout << "FAIL: " << message << "\n";
        ++g_failed;
    }
}

std::string JoinLaneNames(const std::vector<CoverModule::CoverResolutionLane>& lanes) {
    std::string joined;
    for (size_t i = 0; i < lanes.size(); ++i) {
        if (i > 0) joined += " > ";
        joined += CoverModule::CoverResolutionLaneName(lanes[i]);
    }
    return joined;
}

}  // namespace

int main() {
    {
        constexpr auto currentOrder = CoverModule::GetCurrentCoverResolutionOrder();
        Check(currentOrder.size() == 4, "current preview lane order exposes four steps");
        Check(currentOrder[0] == CoverModule::CoverResolutionLane::EmbeddedArt,
              "current preview starts with embedded art");
        Check(currentOrder[1] == CoverModule::CoverResolutionLane::SameStemImage,
              "current preview falls back to same-stem image before DB src_path");
        Check(currentOrder[2] == CoverModule::CoverResolutionLane::DbSourcePath,
              "current preview prefers DB src_path before folder discovery");
        Check(currentOrder[3] == CoverModule::CoverResolutionLane::FolderDiscovery,
              "current preview ends with folder discovery");
    }

    {
        const auto lanes = CoverModule::GetFolderCoverResolutionOrder("C:/Lib/Folder/cover.jpg");
        Check(lanes.size() == 3, "folder cover image path plan includes three steps");
        Check(lanes[0] == CoverModule::CoverResolutionLane::ThumbByKey,
              "folder cover image path starts with thumb");
        Check(lanes[1] == CoverModule::CoverResolutionLane::DirectImagePath,
              "folder cover image path prefers direct image before folder discovery");
        Check(lanes[2] == CoverModule::CoverResolutionLane::FolderDiscovery,
              "folder cover image path ends with folder discovery");
    }

    {
        const auto lanes = CoverModule::GetFolderCoverResolutionOrder("C:/Lib/Folder");
        Check(lanes.size() == 2, "plain folder plan skips direct image lane");
        Check(lanes[0] == CoverModule::CoverResolutionLane::ThumbByKey,
              "plain folder plan still starts with thumb");
        Check(lanes[1] == CoverModule::CoverResolutionLane::FolderDiscovery,
              "plain folder plan falls back to folder discovery");
    }

    {
        Check(CoverModule::LooksLikeCoverImagePath("C:/Lib/Folder/cover.PNG"),
              "image path detector accepts PNG case-insensitively");
        Check(CoverModule::LooksLikeCoverImagePath("C:/Lib/Folder/cover.jpeg"),
              "image path detector accepts JPEG");
        Check(!CoverModule::LooksLikeCoverImagePath("C:/Lib/Folder"),
              "image path detector rejects plain folder path");
        Check(!CoverModule::LooksLikeCoverImagePath("C:/Lib/Folder/cover.txt"),
              "image path detector rejects non-image extension");
    }

    if (g_failed != 0) {
        std::cout << g_failed << " assertion(s) failed.\n";
        return 1;
    }

    std::cout << "All cover lane order tests passed.\n";
    return 0;
}
