#pragma once
// Canonical and display-path helpers.
//
// 设计契约: docs/plans/2026-04-30-canonical-display-separation.md (T1.0 v2)
// 父 plan : docs/plans/2026-04-30-cover-folder-canonical-key-refactor.md
//
// 两个 helper 提供严格分离的 path 角色:
//   - CanonicalFolderPath()        : 比较用 key (字节相等)
//   - DisplayFolderFromFilePath()  : UI 显示用 (保留扫描时大小写)
//
// 使用规则:
//   * 任何 folder_path 字段写入 DB / 进入 unordered_map / 做 == 比较前
//     必须经过 CanonicalFolderPath()
//   * 任何 UI 渲染文件夹路径前必须经过 DisplayFolderFromFilePath()
//   * 详细调用点清单见 T1.0 设计文档第 4 节
//
// 禁止:
//   * 字面 == 比较 folder_path 字符串
//   * 直接渲染 canonical key 给用户看 (会全小写)
//   * 用 fs::path(std::string) 而非 Utf8ToPath() (UTF-8 不安全)

#include <string>
#include <string_view>

namespace HarkenSearch {

// @stage7-contract
// API contract: canonical helpers return normalized owned UTF-8 paths for key
// comparisons, while display helpers preserve user-facing spelling. Callers
// must use the canonical helper for persistence/equality and the display helper
// for UI; neither retains input storage.

// ----------------------------------------------------------------------
// CanonicalFolderPath — 字节比较安全的 canonical 形态
// ----------------------------------------------------------------------
// 输入: 任意来源的 folder path (DB 列、用户输入、扫描产生)
// 输出: 单一规范形态, 可直接做 == / unordered_map key
//
// 规范化规则 (Windows):
//   1. strip 长路径前缀: \\?\ → 去掉; \\?\UNC\ → \\
//   2. 反斜杠分隔符 (/ → \)
//   3. 去尾部斜杠 (但保留 drive root 的尾斜杠: "E:\" 保持原样)
//   4. ASCII 字符小写化 (非 ASCII 不做 case folding)
//
// 不变量:
//   - 幂等: CanonicalFolderPath(CanonicalFolderPath(x)) == CanonicalFolderPath(x)
//   - 异常安全: 空 / 非法输入返回空字符串, 永不抛异常
//
// 注意:
//   - 仅承诺 ASCII 大小写归一. 非 ASCII (例 "É" / "ñ") 保留原样.
//   - 不读磁盘. 不调 fs::canonical (会因文件不存在失败回退).
std::string CanonicalFolderPath(std::string_view rawPath);

// ----------------------------------------------------------------------
// DisplayFolderFromFilePath — UI 显示用 (保留大小写)
// ----------------------------------------------------------------------
// 输入: audio_files.file_path 列的字面值 (可能含 \\?\ 长路径前缀)
// 输出: 适合 UI 渲染的 folder path, 保留扫描时大小写
//
// 处理:
//   1. strip \\?\ / \\?\UNC\ 长路径前缀
//   2. byte-stable separator split first (preserves UTF-8 exactly, including CJK + [] path segments)
//   3. fs::path parent_path() fallback for unusual path forms
//   4. 不做大小写处理 (display 角色保留原大小写)
//
// 异常安全:
//   - 空 / 非法 file_path 返回空字符串
//   - 任何 fs 异常被捕获并 swallow, 保护 UI 渲染线程
//
// 禁止用法:
//   - 不要把它的输出做 == 比较 (那是 CanonicalFolderPath 的职责)
//   - 不要把它的输出存进 DB 列
std::string DisplayFolderFromFilePath(std::string_view filePath);

// ResolveFolderBreadcrumbDisplayPath — UI breadcrumb source selection.
// Prefer the selected audio file's actual parent folder so the list header
// stays linked to the highlighted row/detail panel. Fall back to the row's
// stored folder path, then to the active folder-tree filter when no file is
// selected.
std::string ResolveFolderBreadcrumbDisplayPath(
    std::string_view selectedFilePath,
    std::string_view selectedRecordFolderPath,
    std::string_view activeFolderFilterPath);

std::string ResolveFolderBreadcrumbDisplayLabel(
    std::string_view displayPath,
    std::string_view fallbackLabel);

std::string ResolveActiveFolderBreadcrumbFilterPath(
    bool folderFilterActive,
    std::string_view selectedFolderPath);

} // namespace HarkenSearch
