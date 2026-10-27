#pragma once

enum class AppTheme;
struct ThemeColorScheme;

// @stage7-contract
// API contract: built-in AppTheme color tables only. Callers borrow the
// returned scheme by value and must not treat this as theme switching,
// custom-color persistence, or ApplyTheme. Color-token meanings stay as-is.
ThemeColorScheme MakeBuiltinScheme(AppTheme t);
