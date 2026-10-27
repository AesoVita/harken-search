
// UserActionLogStub — Release build stub (AURA_BUILD_DIAGNOSTICS=0)

#include "UserActionLog.h"

UserActionLog& UserActionLog::instance() {
    static UserActionLog s_instance;
    return s_instance;
}

UserActionLog::UserActionLog()
    : m_sessionStart(std::chrono::steady_clock::now())
    , m_lastAction(m_sessionStart)
{}

UserActionLog::~UserActionLog() {}

void UserActionLog::setEnabled(bool) {}
void UserActionLog::setOutputPath(const std::string&) {}
void UserActionLog::log(UACategory, const char*) {}
void UserActionLog::logFmt(UACategory, const char*, ...) {}
bool UserActionLog::exportTimeline(const std::string&) const { return false; }
std::string UserActionLog::exportTimelineString() const { return {}; }
std::string UserActionLog::exportRecentString(int) const { return {}; }
size_t UserActionLog::entryCount() const { return 0; }
void UserActionLog::clear() {}
int UserActionLog::copyRecent(UAEntry*, int) const { return 0; }
