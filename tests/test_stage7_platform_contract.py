from pathlib import Path

from tools import stage7_api_contract_audit as audit


PLATFORM_BATCH = (
    "src/platform/filesystem.h",
    "src/platform/audio_file.h",
    "src/platform/audio_playback.h",
)

PLATFORM_BATCH_B = (
    "src/platform/audio_data_source.h",
    "src/platform/durable_file_ops.h",
    "src/platform/text_encoding.h",
    "src/platform/executable_path.h",
)

PLATFORM_BATCH_O = (
    "src/platform/database_audio_probe.h",
    "src/platform/emergency_file_writer.h",
    "src/platform/file_hash.h",
    "src/platform/image_codec.h",
    "src/platform/metadata_legacy_tag_content.h",
    "src/platform/pe_inspection.h",
)


def test_platform_batch_o_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PLATFORM_BATCH_O:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_platform_batch_o_headers_carry_contract_marker() -> None:
    for relative_path in PLATFORM_BATCH_O:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_platform_batch_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PLATFORM_BATCH:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_platform_batch_headers_carry_contract_marker() -> None:
    for relative_path in PLATFORM_BATCH:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_platform_batch_b_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PLATFORM_BATCH_B:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_platform_batch_b_headers_carry_contract_marker() -> None:
    for relative_path in PLATFORM_BATCH_B:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


PRODUCT_BATCH_A = (
    "src/MetadataReader.h",
    "src/MetadataWriter.h",
)


def test_product_batch_a_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_A:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_a_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_A:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


PRODUCT_BATCH_B = (
    "src/AudioEngine.h",
    "src/TransferEngine.h",
)

PRODUCT_BATCH_C = (
    "src/platform/system.h",
    "src/DiagnosticsLogger.h",
    "src/Database.h",
)

PRODUCT_BATCH_D = (
    "src/Utils.h",
    "src/Theme.h",
)

PRODUCT_BATCH_E = (
    "src/AIEngine.h",
    "src/Localization.h",
    "src/PathUtils.h",
    "src/WaapiClient.h",
    "src/MediaFoundationAudioDecoder.h",
)

PRODUCT_BATCH_F = (
    "src/AppState.h",
    "src/BuildConfig.h",
    "src/CoverModule.h",
    "src/WwiseTransferConfig.h",
)

PRODUCT_BATCH_G = (
    "src/WaveformCachePolicy.h",
    "src/WaveformExtractor.h",
    "src/platform/text_encoding.inl",
    "src/HarkenSearchWidgets.h",
    "src/BackfillQueue.h",
    "src/BpmDetector.h",
)

PRODUCT_BATCH_H = (
    "src/MemoryBudget.h",
    "src/ThemeInspector.h",
    "src/HarkenSearchIcons.h",
    "src/AvatarCropper.h",
    "src/FeatureExtractor.h",
    "src/KeyDetector.h",
)

PRODUCT_BATCH_I = (
    "src/WindowsMain.h",
    "src/WwiseStructureTemplate.h",
    "src/platform/named_pipe.h",
    "src/BackfillWorkerPool.h",
    "src/BetaFeatureFlags.h",
    "src/CoverReplacementFoundation.h",
)

PRODUCT_BATCH_J = (
    "src/DevToolsModule.h",
    "src/ProToolsBridge.h",
    "src/SearchQuery.h",
    "src/UserActionLog.h",
    "src/WwiseTransferPersist.h",
    "src/platform/process_launch.h",
)

PRODUCT_BATCH_K = (
    "src/platform/process_query.h",
    "src/platform/thread_id.h",
    "src/AsyncWorker.h",
    "src/HarkenSearchMetrics.h",
    "src/BackgroundIoPolicy.h",
    "src/CenterSenseDsp.h",
)

PRODUCT_BATCH_L = (
    "src/CoverBrowseSurfaceState.h",
    "src/DAWPanel.h",
    "src/DawBridgeInstaller.h",
    "src/EmbedScript.h",
    "src/FeatureManifest.h",
    "src/FileBrowser.h",
)

PRODUCT_BATCH_M = (
    "src/HostSendModel.h",
    "src/ModuleUI.h",
    "src/MoodAnalyzer.h",
    "src/NamingScript.h",
    "src/ReaperBridgeClient.h",
    "src/SearchTextIndex.h",
)

PRODUCT_BATCH_N = (
    "src/SendToTargetSurfaceModel.h",
    "src/SpectrogramMath.h",
    "src/TagSystem.h",
    "src/TagVocabularyMapper.h",
    "src/TimeStretch.h",
    "src/UIRenderer.h",
)

PRODUCT_BATCH_P = (
    "src/WwiseDiff.h",
    "src/WwiseQuery.h",
    "src/WwiseTransferExecute.h",
    "src/WwiseTransferPanel.h",
)

PLATFORM_BATCH_Q = (
    "src/platform/database_audio_probe_backend.h",
    "src/platform/database_file_dialog.h",
    "src/platform/file_watcher.h",
    "src/platform/hmac_sha256.h",
)

PRODUCT_BATCH_R = (
    "src/UndoSystem.h",
    "src/VoiceAssetOrganizer.h",
    "src/WaveformBucketMath.h",
    "src/WaveformPresentationPolicy.h",
)

PLATFORM_BATCH_S = (
    "src/platform/file_prefix_probe.h",
    "src/platform/cover_discovery.h",
    "src/platform/qa_files.h",
    "src/platform/pro_tools_path_encoding.h",
    "src/platform/search_text_gb2312.h",
)

PRODUCT_BATCH_T = (
    "src/platform/platform.h",
    "src/platform/theme_json_dialogs.h",
    "src/AnnotationBridge.h",
    "src/HarkenSearchThemeManager.h",
)

PRODUCT_BATCH_U = (
    "src/FrameBudgetDiagnostics.h",
    "src/NativeFloatReader.h",
    "src/ProductEdition.h",
    "src/ProductModeRouting.h",
)

PRODUCT_BATCH_V = (
    "src/ReaperBridgeProtocol.h",
    "src/SearchIntent.h",
    "src/WaapiResponseParse.h",
    "src/WaveformPrewarmPolicy.h",
)

PRODUCT_BATCH_W = (
    "src/ScanInterruption.h",
    "src/ResultActionMenu.h",
    "src/SimilaritySearch.h",
    "src/WaveformRegionEditor.h",
)

PRODUCT_BATCH_X = (
    "src/CenterSenseDataSource.h",
    "src/GaplessEngine.h",
    "src/MusicAnalyzer.h",
    "src/MusicQueue.h",
)

PRODUCT_BATCH_Y = (
    "src/MetadataFields.h",
    "src/ThemeEditor.h",
    "src/UCSCategory.h",
    "src/WelcomeGreeting.h",
)

PRODUCT_BATCH_Z = (
    "src/ExternalLibraryCover.h",
    "src/coverwall_lab/CoverWallLabState.h",
    "src/ReaperBridgeStatus.h",
    "src/ReaperBridgeVersion.h",
)

PRODUCT_BATCH_AA = (
    "src/CubaseAdapter.h",
    "src/GMPanel.h",
    "src/HostSendRuntime.h",
    "src/WorkflowEngine.h",
)

PRODUCT_BATCH_AB = (
    "src/ImGuiAcrylicFader.h",
    "src/ThemeMascot.h",
    "src/WorkerThreadPolicy.h",
    "src/AsyncSnapshotWorker.h",
)

PRODUCT_BATCH_AC = (
    "src/AudioEnvironmentPolicy.h",
    "src/DbNorm.h",
    "src/SearchCommands.h",
    "src/SpotRequest.h",
)

PRODUCT_BATCH_AD = (
    "src/FolderTreeContextActions.h",
    "src/MiniWaveformGeometry.h",
    "src/ScanWorkflow.h",
    "src/TapTempoAccumulator.h",
)

PRODUCT_BATCH_AE = (
    "src/MiniWaveformRender.h",
    "src/MiniWaveformScheduler.h",
    "src/PlaylistFolderTree.h",
    "src/PlaylistQuickBarMotion.h",
)

PRODUCT_BATCH_AF = (
    "src/SelectedWaveformRebuildQueue.h",
    "src/ThemeEditorSchema.h",
    "src/VoiceDeliveryAcceptance.h",
    "src/WaveformCompression.h",
    "src/platform/database_paths.inl",
    "src/platform/file_times.inl",
)

PRODUCT_BATCH_AG = (
    "src/SimilarityQueryService.h",
    "src/SimilarityWorkflow.h",
)


def test_product_batch_ag_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_AG:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_ag_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_AG:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_af_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_AF:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_af_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_AF:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_ae_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_AE:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_ae_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_AE:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_ad_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_AD:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_ad_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_AD:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_ac_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_AC:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_ac_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_AC:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_ab_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_AB:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_ab_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_AB:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_aa_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_AA:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_aa_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_AA:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_z_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_Z:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_z_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_Z:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_y_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_Y:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_y_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_Y:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_x_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_X:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_x_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_X:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_w_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_W:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_w_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_W:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_v_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_V:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_v_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_V:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_u_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_U:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_u_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_U:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_t_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_T:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_t_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_T:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_platform_batch_s_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PLATFORM_BATCH_S:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_platform_batch_s_headers_carry_contract_marker() -> None:
    for relative_path in PLATFORM_BATCH_S:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_r_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_R:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_r_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_R:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_platform_batch_q_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PLATFORM_BATCH_Q:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_platform_batch_q_headers_carry_contract_marker() -> None:
    for relative_path in PLATFORM_BATCH_Q:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_p_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_P:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_p_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_P:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_b_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_B:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_b_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_B:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_c_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_C:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_c_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_C:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_d_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_D:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_d_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_D:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_e_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_E:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_e_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_E:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_f_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_F:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_f_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_F:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_g_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_G:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_g_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_G:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_h_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_H:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_h_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_H:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_i_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_I:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_i_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_I:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_j_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_J:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_j_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_J:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_k_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_K:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_k_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_K:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_l_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_L:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_l_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_L:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_m_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_M:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_m_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_M:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text


def test_product_batch_n_is_explicitly_reviewed() -> None:
    register = audit.build_register(Path.cwd())
    by_path = {item["path"]: item for item in register["headers"]}

    for path in PRODUCT_BATCH_N:
        assert by_path[path]["contract_state"] == "reviewed"
        assert by_path[path]["contract_evidence"]


def test_product_batch_n_headers_carry_contract_marker() -> None:
    for relative_path in PRODUCT_BATCH_N:
        text = Path(relative_path).read_text(encoding="utf-8")
        assert "@stage7-contract" in text
