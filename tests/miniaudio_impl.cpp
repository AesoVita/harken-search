// Provide miniaudio implementation for standalone test executables
#define STB_VORBIS_HEADER_ONLY
#include "../thirdparty/whisper.cpp/examples/stb_vorbis.c"
#define MINIAUDIO_IMPLEMENTATION
#include "../thirdparty/miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "../thirdparty/whisper.cpp/examples/stb_vorbis.c"
#ifdef L
#undef L
#endif
#ifdef C
#undef C
#endif
#ifdef R
#undef R
#endif
