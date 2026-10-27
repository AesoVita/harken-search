#include "SearchHitColumnIdentity.h"

#include <cstdint>
#include <cstdio>
#include <string>

// SearchQuery.cpp links InjectCJKSpaces from Utils.cpp. This focused exe does
// not pull Utils.cpp (it also pulls filesystem/user-data layout). The identity
// cases only use ASCII field prefixes, so a pass-through stub is enough.
std::string InjectCJKSpaces(const std::string& input) {
    return input;
}

static int g_pass = 0;
static int g_fail = 0;

static void expect(bool cond, const char* label) {
    if (cond) {
        std::printf("PASS: %s\n", label);
        ++g_pass;
    } else {
        std::printf("FAIL: %s\n", label);
        ++g_fail;
    }
}

static SearchHitColumnRecord MakeMetadataSummer() {
    SearchHitColumnRecord record;
    record.matchSource = SearchHitMatchSource::Metadata;
    record.title = "Summer";
    record.artist = "Summer";
    record.album = "Summer";
    record.category = "Summer";
    return record;
}

int main() {
    {
        const SearchHitColumnRecord record = MakeMetadataSummer();
        expect(ResolveSearchHitColumnIndex(record, "artist:Summer") == 10,
               "artist:Summer with same-value title resolves to Artist column 10");
        expect(ResolveSearchHitColumnIndex(record, "title:Summer") == 0,
               "title:Summer with same-value artist resolves to filename column 0");
        expect(ResolveSearchHitColumnIndex(record, "category:Summer") == 9,
               "category: prefix resolves to Category column 9");
        expect(ResolveSearchHitColumnIndex(record, "album:Summer") == 11,
               "album: prefix resolves to Album column 11");
    }
    {
        SearchHitColumnRecord record;
        record.matchSource = SearchHitMatchSource::Name;
        record.title = "Summer";
        record.artist = "Summer";
        expect(ResolveSearchHitColumnIndex(record, "Summer") == 0,
               "Name matchSource without field prefix resolves to filename column 0");
    }
    {
        SearchHitColumnRecord record;
        record.matchSource = SearchHitMatchSource::FuzzyName;
        expect(ResolveSearchHitColumnIndex(record, "Summer") == 0,
               "FuzzyName matchSource without field prefix resolves to filename column 0");
    }
    {
        SearchHitColumnRecord record;
        record.matchSource = SearchHitMatchSource::Path;
        expect(ResolveSearchHitColumnIndex(record, "Summer") == 16,
               "Path matchSource without field prefix resolves to FilePath column 16");
        expect(ResolveSearchHitColumnIndex(record, "path:Summer") == 16,
               "path: prefix resolves to FilePath column 16");
    }
    {
        SearchHitColumnRecord record;
        record.matchSource = SearchHitMatchSource::Description;
        expect(ResolveSearchHitColumnIndex(record, "Summer") == 6,
               "Description matchSource without field prefix resolves to Description column 6");
        expect(ResolveSearchHitColumnIndex(record, "desc:Summer") == 6,
               "desc: prefix resolves to Description column 6");
    }
    {
        SearchHitColumnRecord record;
        record.matchSource = SearchHitMatchSource::Keywords;
        expect(ResolveSearchHitColumnIndex(record, "Summer") == 7,
               "Keywords matchSource without field prefix resolves to Keywords column 7");
        expect(ResolveSearchHitColumnIndex(record, "keywords:Summer") == 7,
               "keywords: prefix resolves to Keywords column 7");
    }
    {
        SearchHitColumnRecord record;
        record.matchSource = SearchHitMatchSource::Name;
        expect(ResolveSearchHitColumnIndex(record, "name:Summer") == 0,
               "name: prefix resolves to filename column 0");
    }
    {
        const SearchHitColumnRecord record = MakeMetadataSummer();
        const int column = ResolveSearchHitColumnIndex(record, "Summer");
        expect(column != 0,
               "unprefixed Metadata does not guess title as filename column 0");
    }
    {
        SearchHitColumnRecord record;
        record.matchSource = SearchHitMatchSource::None;
        expect(ResolveSearchHitColumnIndex(record, "artist:Summer") == 10,
               "artist: field identity wins even when matchSource is None");
    }
    {
        expect(ShouldRevealSearchHitColumn("artist:Summer", 7, "artist:Summer", 7) == false,
               "same query and request does not reveal again (load more)");
        expect(ShouldRevealSearchHitColumn("artist:Summer", 8, "artist:Summer", 7) == false,
               "same query with a new load-more request id does not reveal again");
        expect(ShouldRevealSearchHitColumn("artist:Summer", 7, "artist:Winter", 7) == true,
               "new query reveals the hit column");
        expect(ShouldRevealSearchHitColumn("artist:Summer", 7, "", 0) == true,
               "first search intent reveals the hit column");
        expect(ShouldRevealSearchHitColumn("", 7, "", 0) == false,
               "empty query does not reveal a column");
    }

    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
