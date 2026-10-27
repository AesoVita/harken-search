#include "Database.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_pass = 0;
static int g_fail = 0;

static void expect(bool cond, const std::string& label) {
    if (cond) {
        std::cout << "PASS: " << label << "\n";
        ++g_pass;
    } else {
        std::cout << "FAIL: " << label << "\n";
        ++g_fail;
    }
}

static AudioRecord makeRec(const std::string& folder,
                           const std::string& name,
                           const std::string& category,
                           const std::string& keywords,
                           const std::string& description) {
    AudioRecord r;
    r.fileName = name;
    r.displayName = name;
    r.filePath = folder + "\\" + name;
    r.folderPath = folder;
    r.extension = ".wav";
    r.format = "WAV";
    r.durationSeconds = 1.0f;
    r.sampleRate = 48000;
    r.channels = 2;
    r.fileSize = 1024;
    r.totalFrames = 48000;
    r.dateAdded = 1;
    r.fileMtime = 1;
    r.category = category;
    r.keywords = keywords;
    r.description = description;
    return r;
}

static std::vector<AudioRecord> search(Database& db, std::string keyword) {
    SearchFilter filter;
    filter.keyword = std::move(keyword);
    return db.searchFiles(filter, 50, 0);
}

static bool hasFile(const std::vector<AudioRecord>& rows, const std::string& name) {
    for (const auto& row : rows) {
        if (row.fileName == name) return true;
    }
    return false;
}

static bool hasSnippetForAnyRow(const std::vector<AudioRecord>& rows) {
    for (const auto& row : rows) {
        if (!row.matchSnippet.empty()) return true;
    }
    return false;
}

int main() {
    fs::path tmpDir = fs::temp_directory_path() / "auro_fuzzy_search_prd";
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir, ec);

    const std::string dbPath = (tmpDir / "test.db").string();
    const std::string folder = "e:\\fuzzylib";
    const std::string whooshFolder = folder + "\\whoosh";

    Database db;
    if (!db.open(dbPath)) {
        std::cerr << "FAIL: db.open failed: " << db.getLastError() << "\n";
        return 1;
    }

    expect(db.addFile(makeRec(folder, u8"大笑_人群.wav", u8"喜剧", u8"人群 笑声", u8"多人开心大笑")),
           "fixture add da xiao crowd");
    expect(db.addFile(makeRec(folder, u8"倒下_身体.wav", u8"动作", u8"身体 倒地", u8"人物倒下")),
           "fixture add fall body");
    expect(db.addFile(makeRec(folder, u8"恐怖_钟声.wav", u8"恐怖", u8"钟声 金属", u8"远处钟声和恐怖氛围")),
           "fixture add horror bell");
    expect(db.addFile(makeRec(whooshFolder, "WHSH_Wind_Swoosh.wav", "whoosh", "wind air", "fast wind pass by")),
           "fixture add whoosh path");

    expect(hasFile(search(db, u8"笑"), u8"大笑_人群.wav"),
           "Chinese substring search finds da xiao crowd");
    expect(hasFile(search(db, "daxiao"), u8"大笑_人群.wav"),
           "full pinyin search finds da xiao crowd");
    expect(hasFile(search(db, u8"人群 daxiao"), u8"大笑_人群.wav"),
           "AND query with Chinese term plus pinyin OR group finds da xiao crowd");

    {
        auto rows = search(db, "dx");
        expect(hasFile(rows, u8"大笑_人群.wav"),
               "initials search dx finds da xiao crowd");
        expect(hasFile(rows, u8"倒下_身体.wav"),
               "initials search dx finds dao xia body");
    }

    expect(hasFile(search(db, u8"恐怖 钟声"), u8"恐怖_钟声.wav"),
           "Chinese multi-term search finds horror bell");
    expect(hasFile(search(db, "kongbuzhongsheng"), u8"恐怖_钟声.wav"),
           "full pinyin search finds horror bell");
    expect(hasFile(search(db, "kbzs"), u8"恐怖_钟声.wav"),
           "initials search finds horror bell");
    expect(hasFile(search(db, "path:whoosh"), "WHSH_Wind_Swoosh.wav"),
           "path power query finds whoosh path row");

    expect(hasSnippetForAnyRow(search(db, "daxiao")),
           "pinyin search returns a non-empty match snippet");

    db.close();
    fs::remove_all(tmpDir, ec);

    std::cout << "\n=== Summary: " << g_pass << " pass, " << g_fail << " fail ===\n";
    return g_fail == 0 ? 0 : 1;
}
