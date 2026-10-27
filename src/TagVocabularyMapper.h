#pragma once
#include <string>
#include <vector>

// Normalizes raw vendor tags into Aris vocabulary IDs.
// All methods are pure functions (no state, no DB access)
// @stage7-contract
// API contract: vocabulary mapping is pure and deterministic over caller text;
// returned IDs are owned values. Unknown or malformed tags return the existing
// empty-result forms without database or global-state side effects.
class TagVocabularyMapper {
public:
    // Map a raw genre string -> internal genre ID (e.g. "Chill" -> "chillout")
    // Returns "" if no mapping found
    static std::string mapGenre(const std::string& raw);

    // Map a comma/semicolon-separated raw instruments string -> vector of internal IDs
    // (e.g. "Acoustic Guitar, Piano" -> {"guitar_acoustic", "piano"})
    static std::vector<std::string> mapInstruments(const std::string& raw);

    // Map a raw mood string -> internal mood ID (e.g. "Tense" -> "tense")
    static std::string mapMood(const std::string& raw);

    // Infer vocal type from raw genre+description hint
    // Returns "instrumental", "has_vocals", "has_lyrics", or ""
    static std::string inferVocalType(const std::string& genreHints, const std::string& descriptionHints);

    // Process all fields at once. Populates normGenre, normInstruments, normMoodTags, vocalType
    // from rawGenre and rawInstruments strings.
    static void normalize(
        const std::string& rawGenre,
        const std::string& rawInstruments,
        std::string& outNormGenre,
        std::string& outNormInstruments,
        std::string& outNormMoodTags,
        std::string& outVocalType
    );
};
