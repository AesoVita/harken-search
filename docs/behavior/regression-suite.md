# Aris Behavior Regression Suite

**Status:** Active baseline
**Created:** 2026-06-03
**Last Updated:** 2026-07-06

This suite turns the local BDD/TDD behavior contracts into a regression-test entry point. It is not a replacement for full app QA; it is the module-level contract suite that decides which automated proofs must run when behavior is touched.

For signoff wording after the suite runs, use `docs/behavior/acceptance-checklist.md`.

## How To Use

List the suite:

```powershell
py tools/behavior_regression_suite.py --list
```

Run one module:

```powershell
py tools/behavior_regression_suite.py --module search-query --run
```

Run the full behavior regression suite:

```powershell
py tools/behavior_regression_suite.py --all --run
```

Every run starts with:

```powershell
py tools/behavior_contract_check.py
py tools/graphify_auto_refresh.py --once
cmake --build build_dbg --config Debug --target <mapped tests> -- /m:1
```

## Module Matrix

| Module | Contract | CTest regex | Diagnostics / QA follow-up |
| --- | --- | --- | --- |
| `search-query` | `docs/behavior/modules/search-query.md` | `TestSearchSemanticMatrix|TestSearchQueryTrigram|TestSearchRecall` | `SearchLane` coverage assertion |
| `search-execution` | `docs/behavior/modules/search-execution.md` | `TestDbSearchFacets|TestTrigramE2E|TestBrowserSearch` | `Database.searchFiles` duration and lane assertions |
| `scan-indexing` | `docs/behavior/modules/scan-indexing.md` | `TestScanRenameQa|TestBackfillRecoveryWorker|TestScanWaveformCacheContract` | `Scan.*` summary/cache report with no rollback / no flush failure |
| `waveform` | `docs/behavior/modules/waveform.md` | `TestScanWaveformCacheContract|TestWaveformPresentationPolicy|TestImportPrecomputeWaveform` | `MiniWave` and scan-cache runtime assertions |
| `cover` | `docs/behavior/modules/cover.md` | `TestCoverPathNorm|TestCoverLaneOrder|TestCoverClickPerf|TestCoverDiagnostics` | `CoverLane` and `Cover.TextureUploaded` runtime assertions |
| `playback-library-lifecycle` | `docs/behavior/modules/playback-library-lifecycle.md` | `TestSwitchDatabaseBoundary|TestStartupReconciliation|TestOggPlaybackSupport` | `Audio.loadFile` and switch-stage diagnostics |
| `audioengine` | `docs/behavior/modules/audioengine.md` | `TestOggPlaybackSupport|TestWmaDecodeSupport|TestCenterSense|TestTimeStretch|TestTimeStretch2|TestChannelRoutingSettings|TestMultichannelMeter|TestProcessedFileExport|TestSingleChannelExport|TestSilenceSkipGate` | `Audio.loadFile` / export / device-switch diagnostics |
| `ui-orchestration` | `docs/behavior/modules/ui-orchestration.md` | `TestSearchWorkspace|TestSidebarOrder|TestResultActionMenuModel|TestShortcutPresets|TestSwitchDatabaseBoundary|TestUserActionLog` | selection / shortcut / menu / user-action-log assertions |
| `folder-browser` | `docs/behavior/modules/folder-browser.md` | `TestBrowserSearch|TestOpenWith` | browser no-DB audition, system-tree routing, and browser-path handoff assertions |
| `metadata-tags` | `docs/behavior/modules/metadata-tags.md` | `TestMetadataFields|TestTagSystem|TestTagVocabularyMapper|TestMultiTabSync` | metadata patch success/rejection diagnostics assertions |
| `playlist-organization` | `docs/behavior/modules/playlist-organization.md` | `TestSubPlaylists|TestPlaylistFolderTree|TestSetPlaylistParent|TestPlaylistQuickBarMotion` | playlist parent success/reject diagnostics assertions |
| `delivery-export` | `docs/behavior/modules/delivery-export.md` | `TestExportRegionPreserve|TestProcessedFileExport|TestVoiceDeliveryAcceptance` | export start/completion/failure diagnostics assertions |
| `settings-startup` | `docs/behavior/modules/settings-startup.md` | `TestAppSettingsDefaults|TestStartupReconciliation|TestProductEdition|TestSettingsEncryption` | startup branch diagnostics source-contract assertions |
| `database-cache-architecture` | `docs/behavior/modules/database-cache-architecture.md` | `TestDatabaseFeatures|TestStartupReconciliation|TestScanWaveformCacheContract|TestSearchWorkspace|TestUserActionLog|TestExportRegionPreserve` | DB package metrics, sidecar cache state, current workspace/action/export evidence; durable snapshot/activity DB remains follow-up |
| `coverwall-lab` | `docs/behavior/modules/coverwall-lab.md` | `TestCoverWallLabContract` | GUI visual smoke remains human-required |
| `cover-replacement-foundation` | `docs/behavior/modules/cover-replacement-foundation.md` | `TestCoverReplacementFoundation` | replacement identity/storage contract remains stable before wider integration |
| `cover-replacement-browse-surface` | `docs/behavior/modules/cover-replacement-browse-surface.md` | `TestCoverBrowseSurfaceState|TestDatabaseFeatures|TestCoverClickPerf|TestBackgroundIoPolicy` | targeted browse-surface proof at `.openclaw/reports/qa_probe_cover_browse_surface/slice3_targeted/report.json` now covers keyword/tree/path/cover/slider/preview scope; screenshot-level wall readability/top-bottom-info smoke is recorded; only real interaction feel remains human-required |
| `cover-replacement-retirement` | `docs/behavior/modules/cover-replacement-retirement.md` | `TestCoverReplacementFoundation|TestCoverReplacementIntegration|TestDatabaseFeatures|TestBackgroundIoPolicy` | integrated retirement probe on `harness_multi` must prove no-filter / keyword / path / cover prime-repeat, preview parity, rollback, rebuild, and legacy fallback audit after each retirement cut; suite runs `TestDatabaseFeatures` with `AURA_TEST_DATABASE_FEATURES_CASE=cover_tile_membership` for this module |

## Completion Rule

If a module's CTest regex is not run, or diagnostics follow-up is not checked when relevant, final status must say `INCOMPLETE_VERIFICATION`.

If a touched module has `UNKNOWN_BLOCKING` entries relevant to the changed files, the completion report must say how they were resolved, classified, or deferred.
