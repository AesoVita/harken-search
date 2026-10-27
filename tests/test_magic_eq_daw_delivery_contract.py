import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"
ENGINE_H = ROOT / "src" / "AudioEngine.h"
ENGINE_CPP = ROOT / "src" / "AudioEngine.cpp"


def region(source: str, start: str, end: str) -> str:
    left = source.index(start)
    right = source.index(end, left + len(start))
    return source[left:right]


def definition_after_prototype(source: str, start: str, end: str) -> str:
    first = source.index(start)
    left = source.index(start, first + len(start))
    right = source.index(end, left + len(start))
    return source[left:right]


def require_text(test: unittest.TestCase, body: str, expected: str, message: str) -> None:
    if expected not in body:
        test.fail(message)


class MagicEqDawDeliveryContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.main = MAIN.read_text(encoding="utf-8")
        cls.engine_h = ENGINE_H.read_text(encoding="utf-8")
        cls.engine_cpp = ENGINE_CPP.read_text(encoding="utf-8")

    def test_enabled_magic_eq_makes_the_daw_payload_a_processed_copy(self) -> None:
        processing = region(
            self.main,
            "static bool TransferHasProcessing(const AudioEngine::TransferProcessingSnapshot& snapshot)",
            "static bool TransferHasRegion(",
        )
        prepare = definition_after_prototype(
            self.main,
            "static bool PrepareCurrentAudioForTransfer(AudioEngine& engine,\n                                           std::vector<std::string>& outFiles,",
            "static bool TransferHasProcessing(",
        )
        build = region(
            self.main,
            "static bool BuildTransferPayloadForPaths(",
            "static std::vector<std::string> BuildBrowserTransferSourcePaths(",
        )
        require_text(self, processing, "snapshot.magicEQ.enabled",
                     "enabled Magic EQ is not part of the processed-copy decision")
        require_text(self, prepare, "processingSnapshot.magicEQ.enabled",
                     "single-file DAW drag does not snapshot Magic EQ")
        self.assertIn("hasProcessing", build)
        self.assertIn("TransferHasProcessing(processingSnapshot)", build)

    def test_region_and_full_file_render_apply_the_supplied_magic_eq_snapshot(self) -> None:
        require_text(self, self.engine_h, "const TransferProcessingSnapshot* transferSnapshot = nullptr",
                     "render APIs do not accept an explicit frozen DAW EQ snapshot")
        region_render = region(
            self.engine_cpp,
            "bool AudioEngine::exportRegion(",
            "bool AudioEngine::hasAudioAdjustments()",
        )
        file_render = region(
            self.engine_cpp,
            "bool AudioEngine::exportFileProcessed(",
            "void AudioEngine::ensureSeekWorkerStarted()",
        )
        for body in (region_render, file_render):
            require_text(self, body, "transferSnapshot",
                         "file render does not consume the provided EQ snapshot")
            require_text(self, body, "CenterSenseDsp::processMagicEQ",
                         "file render does not run the Magic EQ DSP stage")
            self.assertLess(body.index("processCenterSense"),
                            body.index("processMagicEQ"))

    def test_independent_region_export_passes_current_magic_eq_snapshot(self) -> None:
        toolbar = region(
            self.main,
            "if (exportRegionClicked) {",
            "if (HoveredForHelp())",
        )
        require_text(self, toolbar, "captureTransferProcessingSnapshot()",
                     "independent region export does not capture the current Magic EQ settings")
        require_text(self, toolbar, "&processingSnapshot",
                     "independent region export does not pass the captured settings to the renderer")

    def test_one_snapshot_flows_through_render_before_any_path_is_dispatched(self) -> None:
        render_helper = region(
            self.main,
            "static bool ExportFileWithCurrentTransferProcessing(",
            "static bool BuildTransferPayloadForPaths(",
        )
        build_payload = region(
            self.main,
            "static bool BuildTransferPayloadForPaths(",
            "static std::vector<std::string> BuildBrowserTransferSourcePaths(",
        )
        send = region(
            self.main,
            "static void SendToDAW(AppState& state,\n                      int dawIndex,",
            "static void DispatchPreparedSendToDAW(AppState& state,\n                                      DawTarget& target,",
        )
        require_text(self, render_helper, "TransferProcessingSnapshot& processingSnapshot",
                     "DAW render helper has no immutable processing snapshot input")
        require_text(self, build_payload, "captureTransferProcessingSnapshot()",
                     "multi-file drag does not freeze EQ once before rendering")
        self.assertLess(build_payload.index("captureTransferProcessingSnapshot()"),
                        build_payload.index("for (size_t i = 0;"))
        self.assertLess(render_helper.index("return false"),
                        render_helper.index("outFile = tempFile"))
        require_text(self, send, "const bool payloadReady = BuildTransferPayload(state, files, prepError);",
                     "Send to DAW is not gated on a complete transfer payload")
        self.assertLess(send.index("return;", send.index("if (!payloadReady)")),
                        send.index("DispatchPreparedSendToDAW"))

    def test_current_file_drag_uses_the_same_magic_eq_preparation_gate(self) -> None:
        prepare = definition_after_prototype(
            self.main,
            "static bool PrepareCurrentAudioForTransfer(AudioEngine& engine,\n                                           std::vector<std::string>& outFiles,",
            "static bool TransferHasProcessing(",
        )
        require_text(self, prepare, "engine.exportRegion(",
                     "current-file region drag no longer uses the existing region renderer")
        require_text(self, prepare, "processingSnapshot.magicEQ.enabled",
                     "current-file region drag omits Magic EQ from its render")
        require_text(self, prepare, "&processingSnapshot",
                     "current-file region render does not receive the frozen snapshot")
        require_text(self, prepare, "TrackTempDragFile(tempFile)",
                     "rendered region output is not registered for drag cleanup")
        self.assertLess(prepare.index("engine.exportRegion("),
                        prepare.index("outFiles.push_back(tempFile)"))

    def test_batch_browser_drag_stops_on_processed_export_failure(self) -> None:
        browser_drag = region(
            self.main, "rawPaths.push_back(entry.fullPath);",
            "if (g_pendingDeferredBrowserSelect == entryIndex)",
        )
        self.assertNotIn("StartFileDrag(rawPaths);", browser_drag)
        self.assertIn("state.loadErrorToast = dragErr.empty() ? L(Misc_DragFailed) : dragErr;",
                      browser_drag)
        self.assertIn("state.loadErrorToastUntil = ImGui::GetTime() + 6.0;", browser_drag)

    def test_batch_database_drag_stops_on_processed_export_failure(self) -> None:
        database_drag = region(
            self.main, "rawPaths.push_back(record.filePath);",
            "if (g_pendingDeferredSingleSelect == i)",
        )
        self.assertNotIn("StartFileDrag(rawPaths);", database_drag)
        self.assertIn("state.loadErrorToast = dragErr.empty() ? L(Misc_DragFailed) : dragErr;",
                      database_drag)
        self.assertIn("state.loadErrorToastUntil = ImGui::GetTime() + 6.0;", database_drag)

    def test_other_transfer_entrypoints_keep_their_existing_error_display(self) -> None:
        region_drag = region(self.main,
                             "void StartRegionDrag(AudioEngine& engine, float regionFadeInNorm,",
                             "void StartRegionDrag(AudioEngine& engine) {")
        send = region(self.main,
                      "static void SendToDAW(AppState& state,\n                      int dawIndex,",
                      "static void DispatchPreparedSendToDAW(AppState& state,")
        self.assertIn("Platform::ShowMessageBox(g_hwnd", region_drag)
        self.assertIn("state.lastSendToDAWStatus = prepError;", send)
        self.assertLess(send.index("return;", send.index("if (!payloadReady)")),
                        send.index("DispatchPreparedSendToDAW"))


if __name__ == "__main__":
    unittest.main()
