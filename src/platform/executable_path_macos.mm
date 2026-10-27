#ifdef __APPLE__

#include "executable_path.h"

#import <Foundation/Foundation.h>

namespace Platform {

ExecutablePathResult GetExecutablePath() {
    @autoreleasepool {
        ExecutablePathResult result;
        NSString* path = [[NSBundle mainBundle] executablePath];
        if (!path || [path length] == 0) {
            result.status = ExecutablePathStatus::QueryFailed;
            return result;
        }
        const char* utf8 = [path UTF8String];
        if (!utf8 || utf8[0] == '\0') {
            result.status = ExecutablePathStatus::EncodingFailed;
            return result;
        }
        result.pathUtf8 = utf8;
        result.status = ExecutablePathStatus::Success;
        return result;
    }
}

} // namespace Platform

#endif // __APPLE__
