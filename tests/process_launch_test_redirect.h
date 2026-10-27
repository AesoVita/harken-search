#if defined(AURO_PROCESS_LAUNCH_SEAM_ENABLE)
#define CreateProcessW ::Platform::ProcessLaunchTesting::TestCreateProcessW
#define GetLastError ::Platform::ProcessLaunchTesting::TestLastError
#define CloseHandle ::Platform::ProcessLaunchTesting::TestClose
#elif defined(AURO_PROCESS_LAUNCH_SEAM_DISABLE)
#undef CreateProcessW
#undef GetLastError
#undef CloseHandle
#endif
