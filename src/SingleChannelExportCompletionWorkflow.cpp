#include "SingleChannelExportCompletionWorkflow.h"

#include <chrono>
#include <exception>

namespace aura::audio_export {

std::vector<ChannelExportCompletion> PollChannelExportCompletions(
    std::vector<AppState::ChannelExportJob>& jobs) {
    std::vector<ChannelExportCompletion> completions;
    for (size_t i = 0; i < jobs.size();) {
        AppState::ChannelExportJob& job = jobs[i];
        if (!job.future.valid()) {
            jobs.erase(jobs.begin() + static_cast<ptrdiff_t>(i));
            continue;
        }
        if (job.future.wait_for(std::chrono::milliseconds(0)) !=
            std::future_status::ready) {
            ++i;
            continue;
        }

        AppState::ChannelExportJobResult result;
        try {
            result = job.future.get();
        } catch (const std::exception& ex) {
            result.ok = false;
            result.errorMessage = ex.what();
        } catch (...) {
            result.ok = false;
            result.errorMessage = "Unexpected export failure";
        }

        completions.push_back({job.channelLabel, std::move(result)});
        jobs.erase(jobs.begin() + static_cast<ptrdiff_t>(i));
    }
    return completions;
}

}  // namespace aura::audio_export
