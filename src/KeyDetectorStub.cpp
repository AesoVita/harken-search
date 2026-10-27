// KeyDetectorStub — no-op when aubio is not available (macOS)
#include "KeyDetector.h"

KeyResult KeyDetector::detect(const float*, int, int, uint32_t) {
    return {};
}
KeyResult KeyDetector::detectFile(const std::string&, double, double, double) {
    return {};
}
