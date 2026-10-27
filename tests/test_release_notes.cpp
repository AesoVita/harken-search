#include "release_notes/ReleaseNotes.h"

#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void check(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

struct MemoryReadState final : aris::release_notes::ReadStatePort {
    aris::release_notes::ReadState state;
    int saves = 0;
    bool saveResult = true;

    aris::release_notes::ReadState Load() override { return state; }
    bool Save(const aris::release_notes::ReadState& next) override {
        ++saves;
        if (!saveResult) return false;
        state = next;
        return true;
    }
};

struct MemoryReleaseNotesFiles final : aris::release_notes::ReleaseNotesFilePort {
    std::map<std::string, std::string> files;
    std::vector<std::string> requested;

    bool ReadText(std::string_view fileName, std::string& outText) override {
        requested.emplace_back(fileName);
        const auto found = files.find(std::string(fileName));
        if (found == files.end()) return false;
        outText = found->second;
        return true;
    }
};

std::string MakeRelease(const char* version, const char* date) {
    std::string result = std::string("{\"version\":\"") + version +
                         "\",\"release_date\":\"" + date +
                         "\",\"headline\":\"Update\",\"pull_request_count\":2,"
                         "\"commit_count\":3,\"contributors\":[\"Airs Team\"],"
                         "\"entries\":[{\"title\":\"Title ";
    result += version;
    result += "\",\"description\":\"Description\",\"contributors\":[\"Airs Team\"],";
    result += "\"sources\":[{\"kind\":\"commit\",\"ref\":\"abc\"}]}]}";
    return result;
}

}  // namespace

int main() {
    const std::string v3 =
        R"json({"schema_version":3,"product":"aris","channel":"stable","releases":[{"version":"1.0.3","release_date":"2026-07-31","headline":"What's New","pull_request_count":40,"commit_count":103,"contributors":["yan","Dash"],"entries":[{"title":"Automatic updates","description":"Checks and retries.","contributors":["yan","Dash"],"sources":[{"kind":"pull_request","ref":"#184","url":"https://github.com/xindong/aris/pull/184"},{"kind":"commit","ref":"7167e34"}]}]}]})json";

    const auto autoParsed = aris::release_notes::ParseDocument(v3);
    check(autoParsed.ok(), "schema v3 parses");
    check(autoParsed.document.releases.size() == 1, "schema v3 keeps one release");
    check(autoParsed.document.releases.front().pullRequestCount.value() == 40,
          "PR count is preserved");
    check(autoParsed.document.releases.front().entries.front().sources.size() == 2,
          "provenance sources are preserved");

    const std::string v2 =
        R"json({"schema_version":2,"releases":[{"version":"1.0.2","release_date":"2026-07-30","headline":"Legacy","sections":[{"title":"Improvements","items":[{"name":"Codex","list":["Faster startup"]}]}],"contributors":["Codex"]}]})json";
    const auto legacyParsed = aris::release_notes::ParseDocument(v2);
    check(legacyParsed.ok(), "schema v2 parses without product/channel envelope");
    check(legacyParsed.document.releases.front().entries.front().title == "Improvements",
          "v2 section title normalizes to entry title");
    check(legacyParsed.document.releases.front().entries.front().contributors.front() == "Codex",
          "v2 author normalizes to contributors");
    check(legacyParsed.document.releases.front().entries.front().sources.front().kind ==
              aris::release_notes::ReleaseNoteSourceKind::ReleaseNote,
          "legacy entry receives release-note provenance");

    const std::string structuredV2 =
        R"json({"schema_version":2,"releases":[{"version":"1.0.1","release_date":"2026-07-30","sections":[{"title":"Improvements","items":[{"name":"Codex","list":[{"title":"Faster startup","description":"Startup is faster.","source":"GitHub commit abc"}]}]}]}]})json";
    const auto structuredLegacy = aris::release_notes::ParseDocument(structuredV2);
    check(structuredLegacy.ok(), "schema v2 structured change entries parse");
    if (structuredLegacy.ok()) {
        check(structuredLegacy.document.releases.front().entries.front().title == "Faster startup",
              "schema v2 structured entry preserves its title");
        check(structuredLegacy.document.releases.front().entries.front().description == "Startup is faster.",
              "schema v2 structured entry preserves its description");
    }

    std::string unsortedJson =
        "{\"schema_version\":3,\"product\":\"aris\",\"channel\":\"stable\",\"releases\":[";
    unsortedJson += MakeRelease("1.0.1", "2026-07-29");
    unsortedJson += ",";
    unsortedJson += MakeRelease("1.0.2", "2026-07-30");
    unsortedJson += "]}";
    const auto unsorted = aris::release_notes::ParseDocument(unsortedJson);
    check(unsorted.error == aris::release_notes::ParseError::InvalidVersionOrder,
          "unsorted releases fail closed");

    std::string sixReleases =
        "{\"schema_version\":3,\"product\":\"aris\",\"channel\":\"stable\",\"releases\":[";
    sixReleases += MakeRelease("1.0.6", "2026-07-31");
    sixReleases += "," + MakeRelease("1.0.5", "2026-07-30");
    sixReleases += "," + MakeRelease("1.0.4", "2026-07-29");
    sixReleases += "," + MakeRelease("1.0.3", "2026-07-28");
    sixReleases += "," + MakeRelease("1.0.2", "2026-07-27");
    sixReleases += "," + MakeRelease("1.0.1", "2026-07-26");
    sixReleases += "]}";
    const auto parsedHistory = aris::release_notes::ParseDocument(sixReleases);
    check(parsedHistory.ok(), "history fixture parses");

    MemoryReadState state;
    aris::release_notes::ReleaseNotesService service(parsedHistory.document, state);
    const auto first = service.OpenAutomatic("1.0.6", 5);
    check(first.status == aris::release_notes::OpenStatus::Ready,
          "first launch opens the current release");
    check(first.releases.size() == 1 && first.releases.front().version == "1.0.6",
          "first launch does not dump the entire history");
    const auto manualFirst = service.OpenManual({}, 2);
    check(manualFirst.releases.size() == 2 && manualFirst.hasMore,
          "manual history returns a bounded first page");
    const auto manualSecond = service.OpenManual(manualFirst.next, 2);
    check(manualSecond.releases.size() == 2 && manualSecond.releases.front().version == "1.0.4",
          "manual cursor continues from the previous page");
    check(state.state.lastReadVersion.empty(), "manual browsing does not advance read state");

    check(service.AcknowledgeAutomatic("1.0.6"), "automatic acknowledgement persists");
    check(state.state.lastReadVersion == "1.0.6" && state.saves == 1,
          "acknowledgement advances the read cursor once");
    const auto noUnread = service.OpenAutomatic("1.0.6", 5);
    check(noUnread.status == aris::release_notes::OpenStatus::NoUnread,
          "same version does not reopen automatically");

    state.state.lastReadVersion = "1.0.1";
    const auto upgraded = service.OpenAutomatic("1.0.6", 5);
    check(upgraded.releases.size() == 5 && upgraded.releases.front().version == "1.0.6" &&
              upgraded.releases.back().version == "1.0.2",
          "upgrade notice caps unread history at five newest releases");

    state.saveResult = false;
    check(!service.AcknowledgeAutomatic("1.0.6"),
          "read-state persistence failure is reported");
    check(state.state.lastReadVersion == "1.0.1",
          "failed read-state persistence does not mutate the cursor");

    MemoryReleaseNotesFiles files;
    files.files.emplace("aris_release_notes.json", v3);
    files.files.emplace("auro_release_notes.json", "not-json");
    aris::release_notes::ReleaseNotesRepository repository(files);
    const auto preferred = repository.Load();
    check(preferred.ok(), "repository loads the preferred Aris filename");
    check(preferred.filename == "aris_release_notes.json",
          "repository reports the preferred filename");
    check(files.requested.size() == 1,
          "repository does not probe the legacy filename after preferred success");

    MemoryReleaseNotesFiles legacyFiles;
    legacyFiles.files.emplace("auro_release_notes.json", v2);
    aris::release_notes::ReleaseNotesRepository legacyRepository(legacyFiles);
    const auto legacy = legacyRepository.Load();
    check(legacy.ok(), "repository falls back to the legacy filename");
    check(legacy.filename == "auro_release_notes.json",
          "repository reports the legacy filename");
    check(legacyFiles.requested.size() == 2,
          "repository probes preferred then legacy filename");

    MemoryReleaseNotesFiles malformedFiles;
    malformedFiles.files.emplace("aris_release_notes.json", "not-json");
    malformedFiles.files.emplace("auro_release_notes.json", v2);
    aris::release_notes::ReleaseNotesRepository malformedRepository(malformedFiles);
    const auto malformed = malformedRepository.Load();
    check(!malformed.ok(), "repository rejects malformed preferred data");
    check(malformed.error == aris::release_notes::RepositoryError::ParseFailed,
          "repository reports parse failure instead of silently substituting legacy data");
    check(malformedFiles.requested.size() == 1,
          "repository does not hide malformed preferred data with legacy data");

    return failures == 0 ? 0 : 1;
}
