#pragma once

namespace Platform::FileSystem::Testing {

struct AtomicReplaceScript {
    bool moveFileExResult = false;
    bool deleteFileResult = false;
    bool moveFileResult = false;
    bool targetPresent = true;
    bool sourcePresent = true;
    int moveFileExCalls = 0;
    int deleteFileCalls = 0;
    int moveFileCalls = 0;
};

void SetAtomicReplaceScript(AtomicReplaceScript* script);

} // namespace Platform::FileSystem::Testing
