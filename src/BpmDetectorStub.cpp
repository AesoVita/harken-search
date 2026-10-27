// BpmDetectorStub — no-op when aubio is not available (macOS)
#include "BpmDetector.h"

BpmResult BpmDetector::detect(const float*, int, int, uint32_t) {
    return {};
}
BpmResult BpmDetector::detectFile(const std::string&, double, double, double) {
    return {};
}
