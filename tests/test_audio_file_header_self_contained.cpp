#include "platform/audio_file.h"

#include <type_traits>

static_assert(!std::is_copy_constructible_v<Platform::AudioFile::MonoFloatReader>);
static_assert(std::is_move_constructible_v<Platform::AudioFile::MonoFloatReader>);
static_assert(!std::is_copy_constructible_v<Platform::AudioFile::LinearFloatResampler>);
static_assert(std::is_move_constructible_v<Platform::AudioFile::LinearFloatResampler>);

int main() { return 0; }
