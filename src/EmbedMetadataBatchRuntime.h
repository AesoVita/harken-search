#pragma once

#include <future>
#include <string>

namespace aura::metadata {

struct EmbedMetadataTask {
    std::string path;
    std::string targetKey;
    std::string folderSourcePath;
};

class EmbedMetadataBatchStatePort {
public:
    virtual ~EmbedMetadataBatchStatePort() = default;
    virtual bool active() const = 0;
    virtual int progress() const = 0;
    virtual int total() const = 0;
    virtual EmbedMetadataTask currentTask() const = 0;
    virtual void advance() = 0;
    virtual void finish() = 0;
};

class EmbedMetadataTaskPort {
public:
    virtual ~EmbedMetadataTaskPort() = default;
    virtual std::future<bool> launch(EmbedMetadataTask task) = 0;
};

class EmbedMetadataBatchRuntime {
public:
    void pump(EmbedMetadataBatchStatePort& state, EmbedMetadataTaskPort& tasks);
    bool inFlight() const { return inFlight_; }

private:
    std::future<bool> future_;
    bool inFlight_ = false;
};

}  // namespace aura::metadata
