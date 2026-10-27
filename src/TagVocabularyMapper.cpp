#include "TagVocabularyMapper.h"
#include <algorithm>
#include <cctype>
#include <sstream>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static std::string toLower(const std::string& s) {
    std::string out(s.size(), '\0');
    std::transform(s.begin(), s.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Split a string by comma or semicolon, trimming whitespace from each token.
static std::vector<std::string> splitTokens(const std::string& s) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : s) {
        if (c == ',' || c == ';') {
            // trim leading/trailing spaces from cur
            size_t start = cur.find_first_not_of(" \t");
            size_t end   = cur.find_last_not_of(" \t");
            if (start != std::string::npos) {
                tokens.push_back(cur.substr(start, end - start + 1));
            }
            cur.clear();
        } else {
            cur += c;
        }
    }
    // last token
    size_t start = cur.find_first_not_of(" \t");
    size_t end   = cur.find_last_not_of(" \t");
    if (start != std::string::npos) {
        tokens.push_back(cur.substr(start, end - start + 1));
    }
    return tokens;
}

// Lookup tables: {lowercase_keyword, internal_id}
// Longer/more-specific entries should come before shorter/generic ones so that
// a more-specific substring match wins first.
using Mapping = std::vector<std::pair<std::string, std::string>>;

static const Mapping& genreTable() {
    static const Mapping t = {
        // chillout
        {"chill",            "chillout"},
        {"downtempo",        "chillout"},
        {"trip hop",         "chillout"},
        {"lounge",           "chillout"},
        // electronic
        {"retro electronic", "electronic"},
        {"drum & bass",      "electronic"},
        {"lo-fi",            "electronic"},
        {"lofi",             "electronic"},
        {"techno",           "electronic"},
        {"house",            "electronic"},
        {"electronic",       "electronic"},
        {"synth",            "electronic"},
        {"edm",              "electronic"},
        {"dance",            "electronic"},
        // hiphop
        {"hip hop",          "hiphop"},
        {"hiphop",           "hiphop"},
        {"rap",              "hiphop"},
        {"urban",            "hiphop"},
        {"beats",            "hiphop"},
        {"trap",             "hiphop"},
        // ambient
        {"ambient",          "ambient"},
        {"atmospheric",      "ambient"},
        {"drone",            "ambient"},
        {"soundscape",       "ambient"},
        // metal (before rock to catch "hard rock", "industrial rock")
        {"industrial rock",  "metal"},
        {"hard rock",        "metal"},
        {"metal",            "metal"},
        {"heavy",            "metal"},
        // rock
        {"post-rock",        "rock"},
        {"guitar rock",      "rock"},
        {"alternative",      "rock"},
        {"indie",            "rock"},
        {"rock",             "rock"},
        // pop (synth pop before pop)
        {"modern pop",       "pop"},
        {"synth pop",        "pop"},
        {"contemporary",     "pop"},
        {"feel good",        "pop"},
        {"pop",              "pop"},
        // jazz
        {"bossa nova",       "jazz"},
        {"bebop",            "jazz"},
        {"swing",            "jazz"},
        {"jazz",             "jazz"},
        // blues_soul
        {"r&b",              "blues_soul"},
        {"blues",            "blues_soul"},
        {"soul",             "blues_soul"},
        {"funk",             "blues_soul"},
        // folk_acoustic
        {"bluegrass",        "folk_acoustic"},
        {"americana",        "folk_acoustic"},
        {"country",          "folk_acoustic"},
        {"acoustic",         "folk_acoustic"},
        {"folk",             "folk_acoustic"},
        // classical
        {"grand piano",      "classical"},  // specific before plain "piano"
        {"symphony",         "classical"},
        {"classical",        "classical"},
        {"chamber",          "classical"},
        {"baroque",          "classical"},
        // world
        {"african",          "world"},
        {"celtic",           "world"},
        {"latin",            "world"},
        {"asian",            "world"},
        {"ethnic",           "world"},
        {"traditional",      "world"},
        {"global",           "world"},
        {"world",            "world"},
        // cinematic
        {"film score",       "cinematic"},
        {"dramatic",         "cinematic"},
        {"cinematic",        "cinematic"},
        {"epic",             "cinematic"},
        // orchestral (after cinematic so "orchestral" beats "epic" if needed)
        {"orchestra",        "orchestral"},
        {"orchestral",       "orchestral"},
        // trailer
        {"blockbuster",      "trailer"},
        {"hybrid",           "trailer"},
        {"trailer",          "trailer"},
        {"action",           "trailer"},
        // corporate
        {"presentation",     "corporate"},
        {"motivational",     "corporate"},
        {"technology",       "corporate"},
        {"business",         "corporate"},
        {"corporate",        "corporate"},
        // comedy
        {"lighthearted",     "comedy"},
        {"quirky",           "comedy"},
        {"cartoon",          "comedy"},
        {"comedy",           "comedy"},
        {"fun",              "comedy"},
        // children
        {"nursery",          "children"},
        {"children",         "children"},
        {"kids",             "children"},
        // horror
        {"suspense",         "horror"},
        {"thriller",         "horror"},
        {"creepy",           "horror"},
        {"horror",           "horror"},
        {"dark",             "horror"},
        // sports
        {"athletic",         "sports"},
        {"sports",           "sports"},
        {"hype",             "sports"},
        {"energy",           "sports"},
        // nature
        {"documentary",      "nature"},
        {"wildlife",         "nature"},
        {"travel",           "nature"},
        {"nature",           "nature"},
        // news
        {"broadcast",        "news"},
        {"journalism",       "news"},
        {"news",             "news"},
        // romantic
        {"wedding",          "romantic"},
        {"tender",           "romantic"},
        {"romantic",         "romantic"},
        {"love",             "romantic"},
        // inspirational
        {"inspirational",    "inspirational"},
        {"uplifting",        "inspirational"},
        {"spiritual",        "inspirational"},
        {"positive",         "inspirational"},
        // experimental
        {"avant-garde",      "experimental"},
        {"experimental",     "experimental"},
        {"abstract",         "experimental"},
    };
    return t;
}

static const Mapping& instrumentTable() {
    static const Mapping t = {
        // drums_acoustic (more specific first)
        {"acoustic drum",     "drums_acoustic"},
        {"drum kit",          "drums_acoustic"},
        {"jazz drum",         "drums_acoustic"},
        // drums_electronic
        {"drum machine",      "drums_electronic"},
        {"electronic drum",   "drums_electronic"},
        {"808",               "drums_electronic"},
        // percussion
        {"tambourine",        "percussion"},
        {"percussion",        "percussion"},
        {"bongo",             "percussion"},
        {"conga",             "percussion"},
        // bass_electric
        {"electric bass",     "bass_electric"},
        {"bass guitar",       "bass_electric"},
        // bass_synth
        {"synth bass",        "bass_synth"},
        {"moog",              "bass_synth"},
        // bass_upright
        {"upright bass",      "bass_upright"},
        {"double bass",       "bass_upright"},
        {"contrabass",        "bass_upright"},
        // piano (grand/upright before plain)
        {"grand piano",       "piano"},
        {"upright piano",     "piano"},
        {"piano",             "piano"},
        // keys_electric
        {"electric piano",    "keys_electric"},
        {"wurlitzer",         "keys_electric"},
        {"rhodes",            "keys_electric"},
        {"e piano",           "keys_electric"},
        // organ
        {"hammond",           "organ"},
        {"organ",             "organ"},
        // synth_pad
        {"lead synth",        "synth_pad"},
        {"synthesizer",       "synth_pad"},
        {"synth",             "synth_pad"},
        {"pad",               "synth_pad"},
        // guitar_acoustic (specific before generic "guitar")
        {"acoustic guitar",   "guitar_acoustic"},
        {"classical guitar",  "guitar_acoustic"},
        {"fingerpicking",     "guitar_acoustic"},
        {"nylon guitar",      "guitar_acoustic"},
        // guitar_electric
        {"electric guitar",   "guitar_electric"},
        {"guitar",            "guitar_electric"},   // fallback
        // strings_section (more specific before "strings")
        {"orchestra strings", "strings_section"},
        {"violin section",    "strings_section"},
        {"string section",    "strings_section"},
        {"strings",           "strings_section"},
        // strings_solo
        {"solo violin",       "strings_solo"},
        {"solo strings",      "strings_solo"},
        {"viola",             "strings_solo"},
        {"cello",             "strings_solo"},
        // brass_section
        {"horn section",      "brass_section"},
        {"brass section",     "brass_section"},
        {"brass",             "brass_section"},
        // brass_solo
        {"solo trumpet",      "brass_solo"},
        {"trombone",          "brass_solo"},
        {"trumpet",           "brass_solo"},
        // woodwinds
        {"woodwind",          "woodwinds"},
        {"clarinet",          "woodwinds"},
        {"bassoon",           "woodwinds"},
        {"saxophone",         "woodwinds"},
        {"flute",             "woodwinds"},
        {"oboe",              "woodwinds"},
        // harp
        {"harp",              "harp"},
        // choir
        {"vocal ensemble",    "choir"},
        {"a cappella",        "choir"},
        {"choral",            "choir"},
        {"chorus",            "choir"},
        {"choir",             "choir"},
        // world_instruments
        {"world instrument",  "world_instruments"},
        {"didgeridoo",        "world_instruments"},
        {"shamisen",          "world_instruments"},
        {"hand pan",          "world_instruments"},
        {"sitar",             "world_instruments"},
        {"erhu",              "world_instruments"},
        {"ethnic",            "world_instruments"},
        // sound_design
        {"sound design",      "sound_design"},
        {"texture",           "sound_design"},
        {"sample",            "sound_design"},
        {"noise",             "sound_design"},
    };
    return t;
}

static const Mapping& moodTable() {
    static const Mapping t = {
        {"energetic",    "energetic"},
        {"powerful",     "energetic"},
        {"driving",      "energetic"},
        {"intense",      "energetic"},
        {"uplifting",    "uplifting"},
        {"triumphant",   "uplifting"},
        {"victorious",   "uplifting"},
        {"feel good",    "happy"},
        {"cheerful",     "happy"},
        {"joyful",       "happy"},
        {"happy",        "happy"},
        {"whimsical",    "playful"},
        {"playful",      "playful"},
        {"fun",          "playful"},
        {"intimate",     "romantic"},
        {"romantic",     "romantic"},
        {"love",         "romantic"},
        {"meditative",   "calm"},
        {"peaceful",     "calm"},
        {"relaxing",     "calm"},
        {"serene",       "calm"},
        {"calm",         "calm"},
        {"bittersweet",  "melancholic"},
        {"longing",      "melancholic"},
        {"melancholic",  "melancholic"},
        {"sorrowful",    "sad"},
        {"mourning",     "sad"},
        {"grief",        "sad"},
        {"sad",          "sad"},
        {"nostalgic",    "nostalgic"},
        {"vintage",      "nostalgic"},
        {"retro",        "nostalgic"},
        {"enigmatic",    "mysterious"},
        {"mysterious",   "mysterious"},
        {"strange",      "mysterious"},
        {"suspenseful",  "tense"},
        {"anxious",      "tense"},
        {"nervous",      "tense"},
        {"tense",        "tense"},
        {"ominous",      "dark"},
        {"sinister",     "dark"},
        {"gloomy",       "dark"},
        {"dark",         "dark"},
        {"aggressive",   "aggressive"},
        {"fierce",       "aggressive"},
        {"brutal",       "aggressive"},
        {"angry",        "aggressive"},
        {"monumental",   "epic"},
        {"majestic",     "epic"},
        {"grand",        "epic"},
        {"epic",         "epic"},
    };
    return t;
}

// Match one lowercase token against a lookup table.
// Returns the internal ID of the first entry whose keyword appears as a
// substring within the (already-lowercased) token, or "" if none found.
static std::string matchOne(const std::string& lowerToken, const Mapping& table) {
    for (const auto& kv : table) {
        if (lowerToken.find(kv.first) != std::string::npos) {
            return kv.second;
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::string TagVocabularyMapper::mapGenre(const std::string& raw) {
    if (raw.empty()) return {};
    return matchOne(toLower(raw), genreTable());
}

std::vector<std::string> TagVocabularyMapper::mapInstruments(const std::string& raw) {
    if (raw.empty()) return {};
    std::vector<std::string> results;
    std::vector<std::string> tokens = splitTokens(raw);
    for (const auto& tok : tokens) {
        std::string id = matchOne(toLower(tok), instrumentTable());
        if (!id.empty()) {
            // Deduplicate
            bool found = false;
            for (const auto& r : results) {
                if (r == id) { found = true; break; }
            }
            if (!found) results.push_back(id);
        }
    }
    return results;
}

std::string TagVocabularyMapper::mapMood(const std::string& raw) {
    if (raw.empty()) return {};
    return matchOne(toLower(raw), moodTable());
}

std::string TagVocabularyMapper::inferVocalType(const std::string& genreHints,
                                                const std::string& descriptionHints) {
    std::string combined = toLower(genreHints) + " " + toLower(descriptionHints);

    // Check for lyrics-level hints first (most specific)
    if (combined.find("lyric") != std::string::npos ||
        combined.find("lyrics") != std::string::npos ||
        combined.find("singing") != std::string::npos ||
        combined.find("sung") != std::string::npos ||
        combined.find("song") != std::string::npos) {
        return "has_lyrics";
    }

    // Then general vocal presence
    if (combined.find("vocal") != std::string::npos ||
        combined.find("voice") != std::string::npos ||
        combined.find("choir") != std::string::npos ||
        combined.find("a cappella") != std::string::npos ||
        combined.find("spoken") != std::string::npos ||
        combined.find("choral") != std::string::npos ||
        combined.find("chorus") != std::string::npos ||
        combined.find("rap") != std::string::npos) {
        return "has_vocals";
    }

    // Instrumental hint
    if (combined.find("instrumental") != std::string::npos ||
        combined.find("no vocal") != std::string::npos ||
        combined.find("no lyric") != std::string::npos) {
        return "instrumental";
    }

    return {};
}

void TagVocabularyMapper::normalize(
        const std::string& rawGenre,
        const std::string& rawInstruments,
        std::string& outNormGenre,
        std::string& outNormInstruments,
        std::string& outNormMoodTags,
        std::string& outVocalType) {

    // Map genre
    outNormGenre = mapGenre(rawGenre);

    // Map instruments — join internal IDs with comma
    {
        std::vector<std::string> ids = mapInstruments(rawInstruments);
        std::string joined;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) joined += ',';
            joined += ids[i];
        }
        outNormInstruments = std::move(joined);
    }

    // Derive mood tags from genre (genre string may carry mood hints)
    {
        // Split rawGenre by comma/semicolon — each token may map to a mood
        std::vector<std::string> genreTokens = splitTokens(rawGenre);
        std::vector<std::string> moodIds;
        for (const auto& tok : genreTokens) {
            std::string id = mapMood(tok);
            if (!id.empty()) {
                bool dup = false;
                for (const auto& m : moodIds) { if (m == id) { dup = true; break; } }
                if (!dup) moodIds.push_back(id);
            }
        }
        std::string joined;
        for (size_t i = 0; i < moodIds.size(); ++i) {
            if (i > 0) joined += ',';
            joined += moodIds[i];
        }
        outNormMoodTags = std::move(joined);
    }

    // Infer vocal type from genre + instruments combined
    outVocalType = inferVocalType(rawGenre, rawInstruments);
}
