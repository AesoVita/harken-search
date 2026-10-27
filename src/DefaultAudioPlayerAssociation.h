#pragma once

#include <string>

// 默认播放器能力登记与取消。测试必须传入本卡专用 HKCU 根，禁止读写真实 FileExts。
bool RegisterDefaultProgramsCapabilitiesUnder(const std::string& hkcuRootUtf8);
bool RemoveDefaultPlayerUnder(const std::string& fileExtsRootUtf8);
bool IsRegisteredAsDefaultUnder(const std::string& fileExtsRootUtf8);
bool RegisterDefaultProgramsCapabilities();
bool IsSystemDefaultAudioPlayer();
void RegisterOpenWith();
void SetAsDefaultPlayer();
void RemoveDefaultPlayer();
bool IsRegisteredAsDefault();
