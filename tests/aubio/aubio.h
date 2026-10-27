#pragma once

#include <cstdlib>

using uint_t = unsigned int;

struct fvec_t {
    uint_t length;
    float* data;
};

struct aubio_tempo_t {};

inline aubio_tempo_t* new_aubio_tempo(const char*, uint_t, uint_t, uint_t) {
    return new aubio_tempo_t;
}
inline void del_aubio_tempo(aubio_tempo_t* tempo) { delete tempo; }
inline fvec_t* new_fvec(uint_t length) {
    auto* vector = new fvec_t{length, static_cast<float*>(std::calloc(length, sizeof(float)))};
    return vector->data ? vector : (delete vector, nullptr);
}
inline void del_fvec(fvec_t* vector) {
    if (!vector) return;
    std::free(vector->data);
    delete vector;
}
inline void aubio_tempo_do(aubio_tempo_t*, const fvec_t*, fvec_t* output) {
    output->data[0] = 1.0f;
}
inline float aubio_tempo_get_bpm(const aubio_tempo_t*) { return 120.0f; }
inline float aubio_tempo_get_confidence(const aubio_tempo_t*) { return 0.75f; }
