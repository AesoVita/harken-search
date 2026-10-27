# Aris BDD / TDD Module Index

**Status:** Active baseline
**Created:** 2026-06-03
**Last Updated:** 2026-07-16

## Coverage Table

| Module | Contract | Semantic source | Primary Graphify targets | Primary proof | Status |
| --- | --- | --- | --- | --- | --- |
| Search Query | `modules/search-query.md` | `SEM-P0-SEARCHQUERY-semantic-audit-v1.md` | `SearchQuery`, `BuildSearchQueryChips` | `TestSearchSemanticMatrix`, `TestSearchQueryTrigram`, `TestSearchRecall` | active |
| Search Execution | `modules/search-execution.md` | `SEM-P0-BUILDSEARCHWHERE-semantic-audit-v1.md` | `buildSearchWhere`, `searchFiles`, `searchFilesCount` | `TestDbSearchFacets`, `TestTrigramE2E`, `TestBrowserSearch` | active |
| Scan / Indexing | `modules/scan-indexing.md` | `SEM-P0-SCANINDEXING-semantic-audit-v1.md` | `scanDirectory`, `scanDirectoryEx`, `FileSystemWatcher` | `TestScanRenameQa`, `TestBackfillRecoveryWorker`, `TestScanWaveformCacheContract` | active |
| Similar Sounds | `modules/similar-sounds.md` | F-028 similarity-search behavior contract | `SimilaritySearch`, `FeatureExtractor`, `SimilarityWorkflow`, `SimilarityQueryService`, `RenderSimilarSoundsWindow` | `TestSimilaritySearch`, `TestFeatureExtractor`, `TestSimilarityWorkflow`, `TestSimilarityQueryService`, `TestSimilarSoundsWindowContract`, `TestLocalization` | active |
| Similar Sounds | `modules/similar-sounds.md` | F-028 similarity-search behavior contract | `SimilaritySearch`, `FeatureExtractor`, `SimilarityWorkflow`, `SimilarityQueryService`, `RenderSimilarSoundsWindow` | `TestSimilaritySearch`, `TestFeatureExtractor`, `TestSimilarityWorkflow`, `TestSimilarityQueryService`, `TestSimilarSoundsWindowContract`, `TestLocalization` | active |
| Waveform | `modules/waveform.md` | `SEM-P0-WAVEFORM-semantic-audit-v1.md` | `WaveformWorker`, `waveformWorkerLoop`, `WaveformExtractor` | `TestScanWaveformCacheContract`, `TestWaveformPresentationPolicy`, `TestImportPrecomputeWaveform` | active |
| Cover | `modules/cover.md` | `SEM-P0-COVER-semantic-audit-v1.md` | `CoverWallState`, `CoverLane`, cover DB/cache APIs | `TestCoverPathNorm`, `TestCoverLaneOrder`, `TestCoverClickPerf` | active |
| Playback / Library Lifecycle | `modules/playback-library-lifecycle.md` | `SEM-P0-PLAYBACK-LIFECYCLE-semantic-audit-v1.md` | `loadFileAsync`, `SwitchDatabase`, `ResetDatabaseState` | `TestSwitchDatabaseBoundary`, playback diagnostics, startup reconciliation tests | active |
| AudioEngine | `modules/audioengine.md` | product cards + source scan | `AudioEngine::loadFileAsync`, `switchOutputDevice`, `exportRegion`, `exportFileProcessed` | `TestOggPlaybackSupport`, `TestWmaDecodeSupport`, `TestCenterSense`, `TestProcessedFileExport` | active |
| UI Orchestration | `modules/ui-orchestration.md` | product cards + source scan | `RenderUI`, `ProcessKeybindings`, sidebar/detail/action routing | `TestSearchWorkspace`, `TestSidebarOrder`, `TestResultActionMenuModel`, `TestShortcutPresets` | active |
| Folder Browser | `modules/folder-browser.md` | current task card + source scan | `OpenFileInFileBrowser`, Browser row selection/send helpers, Browser system tree helpers | `TestBrowserSearch`, `TestOpenWith` | active |
| Metadata / Tags | `modules/metadata-tags.md` | product cards + source scan | `MetadataFields`, `MetadataEditPatch`, `TagSystem` | `TestMetadataFields`, `TestTagSystem`, `TestTagVocabularyMapper` | active |
| Playlist Organization | `modules/playlist-organization.md` | product cards + source scan | playlist DB APIs, sub-playlist helpers | `TestSubPlaylists`, `TestPlaylistFolderTree`, `TestSetPlaylistParent` | active |
| Delivery / Export | `modules/delivery-export.md` | product cards + source scan | export processors, delivery helpers | `TestExportRegionPreserve`, `TestProcessedFileExport`, `TestVoiceDeliveryAcceptance` | active |
| Editor Handoff | `modules/editor-handoff.md` | product cards + source scan | `SendToDAW`, `OpenWwiseTransferPanelForRecords`, `OpenTransferDialogForRecords`, `SendToTargetSurfaceModel` | `TestSendToTargetSurfaceModel`, `TestCubaseDirectInsertContract`, `TestResultActionMenuModel`, `TestSendToTargetUiContract` | active |
| Settings / Startup | `modules/settings-startup.md` | product cards + source scan | `AppSettings`, startup reconciliation, auth/tester-name source | `TestAppSettingsDefaults`, `TestStartupReconciliation`, `TestProductEdition` | active |
| Database / Cache Architecture | `modules/database-cache-architecture.md` | Soundminer benchmark + source scan | `Database::open`, `attachSidecarDatabases`, `createSidecarTables`, `getCacheSizeReport`, `compactCachePackage` | `TestDatabaseFeatures`, `TestStartupReconciliation`, `TestScanWaveformCacheContract` | active |
| CoverWallLab | `modules/coverwall-lab.md` | current task card + product/source scan | `CoverWallLabState`, `CoverWallLab` | `TestCoverWallLabContract` | active |
| Cover Replacement Foundation | `modules/cover-replacement-foundation.md` | replacement design spec + current task + source scan | `CoverStorageCompatibilityMode`, `CoverIdentityState`, `NormalizeIdentityKeyForComparison` | `TestCoverReplacementFoundation` | active |
| Cover Replacement Browse Surface | `modules/cover-replacement-browse-surface.md` | replacement design spec + durable Slice 3 card + Graphify/source scan | `RenderSidebarPanelContent`, `BuildCoverWallPageLoadResult`, `CoverWallState`, `displaySnapshot`, `pinnedCoverFilterItem` | `TestCoverBrowseSurfaceState`, `TestCoverClickPerf`, `TestBackgroundIoPolicy`, targeted browse-surface runtime probe | active |
| Cover Replacement Retirement | `modules/cover-replacement-retirement.md` | replacement design spec + durable Slice 5 card + Graphify/source scan | `BuildCoverWallPageLoadResult`, `ApplyAsyncSearchResult`, `CoverModule::AsyncLoaderThread`, `Database::getGlobalCoverWallEntriesPage`, `Database::getCoverFacets`, `Database::getFileIdsForCoverTileMembership` | retirement source/DB contracts + integrated retirement runtime probe | active |
| Platform Services | `modules/platform-services.md` | `2026-07-14-stage6-platform-abstraction-design.md` | `Platform`, `FileSystemWatcher`, `AudioEngine` | `TestStage6PlatformBoundary` | active |

> `Primary proof` 是索引层的代表性证据，不是完整测试清单；完整且可执行的模块映射以 `tools/behavior_regression_suite.py` 为准，并由 `TestBehaviorContractCheck` 持续校验两侧 20/20 契约集合与 Status 一致。

## Global Change Gate

- Run `py tools/behavior_regression_suite.py --list` to identify the regression module key.
- Run `py tools/behavior_contract_check.py`.
- Run `py tools/graphify_auto_refresh.py --once`.
- For the changed module, run the listed TDD / QA proof or mark unrun proof as `INCOMPLETE_VERIFICATION`.
- For module or release signoff, use `docs/behavior/acceptance-checklist.md` to state what was checked, what command ran, and what pass signals were observed.
- For software-only human validation, use `docs/behavior/manual-acceptance-checklist.md`.
- If `UNKNOWN_BLOCKING` entries remain in the touched module's graph path, either resolve them, classify them, or record them as explicit follow-up debt.
- For full BDD/TDD regression, run `py tools/behavior_regression_suite.py --all --run`.
