import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class AudioEngineWaveformInternalTypesContract(unittest.TestCase):
    def test_peak_accumulation_uses_standard_internal_counter_types(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::buildWaveform(", 1)[1].split(
            "// Seek-based quick waveform", 1
        )[0]
        self.assertIn("uint64_t blockFrames = 0;", body)
        self.assertIn("for (uint64_t i = 0; i < framesRead; i++)", body)
        self.assertIn("for (uint32_t ch = 0; ch < actualChannels; ch++)", body)
        self.assertNotIn("ma_uint64 blockFrames", body)
        self.assertNotIn("for (ma_uint64 i = 0; i < framesRead; i++)", body)
        self.assertNotIn("for (ma_uint32 ch = 0; ch < actualChannels; ch++)", body)

    def test_streaming_peak_commit_uses_standard_channel_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        self.assertIn(
            "const uint32_t actualChannels = decoderFormat.channels;", worker
        )
        body = worker.split("auto commitStreamingPeak =", 1)[1].split(
            "uint64_t blockFrames = 0;", 1
        )[0]
        self.assertIn(
            "for (uint32_t ch = 0; ch < actualChannels; ++ch)", body
        )
        self.assertNotIn(
            "for (ma_uint32 ch = 0; ch < actualChannels; ++ch)", body
        )

    def test_streaming_decode_uses_standard_channel_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        self.assertIn(
            "const uint32_t actualChannels = decoderFormat.channels;", worker
        )
        self.assertIn("Platform::AudioDataSource::ReadPcmFrames(", worker)
        body = worker.split(
            "uint64_t framesRead = readResult.framesRead;",
            1,
        )[1].split("if (needTrueRmsForMainWave) {", 1)[0]
        self.assertIn("for (uint32_t ch = 0; ch < actualChannels; ++ch)", body)
        self.assertNotIn("for (ma_uint32 ch = 0; ch < actualChannels; ++ch)", body)

    def test_truncated_stretch_uses_standard_channel_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile", 1
        )[0]
        self.assertIn(
            "const uint32_t actualChannels = decoderFormat.channels;", worker
        )
        body = worker.split(
            "if (kEnableWaveformVisualStretchRepair && srcPeaks > 0 && srcPeaks < dstPeaks) {",
            1,
        )[1].split("req = requestedPeaksSnapshot;", 1)[0]
        self.assertIn("for (uint32_t ch = 0; ch < actualChannels; ++ch)", body)
        self.assertNotIn("for (ma_uint32 ch = 0; ch < actualChannels; ++ch)", body)

    def test_seek_lod_peak_decode_uses_standard_channel_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        self.assertIn(
            "const uint32_t actualChannels = decoderFormat.channels;", worker
        )
        body = worker.split("uint64_t actualFramesForPeak = 0;", 1)[1].split(
            "actualFramesForPeak += framesRead;", 1
        )[0]
        self.assertIn("uint64_t framesRead = readResult.framesRead;", body)
        self.assertIn("for (uint32_t ch = 0; ch < actualChannels; ++ch)", body)
        self.assertNotIn(
            "for (ma_uint32 ch = 0; ch < actualChannels; ++ch)", body
        )

    def test_seek_lod_peak_commit_uses_standard_channel_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        self.assertIn(
            "const uint32_t actualChannels = decoderFormat.channels;", worker
        )
        self.assertIn("const int base = peakIdx * (int)actualChannels;", worker)
        body = worker.split(
            "m_fileInfo.durationSeconds = (float)((double)realTotalFrames / (double)sr);",
            1,
        )[1].split("completedPeaks = peakIdx + 1;", 1)[0]
        self.assertIn(
            "sumSqByCh[(size_t)ch] / (double)actualFramesForPeak", body
        )
        self.assertIn("for (uint32_t ch = 0; ch < actualChannels; ++ch)", body)
        self.assertNotIn(
            "for (ma_uint32 ch = 0; ch < actualChannels; ++ch)", body
        )

    def test_append_tail_peak_uses_standard_channel_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        self.assertIn(
            "const uint32_t actualChannels = decoderFormat.channels;", worker
        )
        self.assertIn(
            "auto appendTailPeak = [&](uint64_t usedFrames) {", worker
        )
        body = worker.split("auto appendTailPeak =", 1)[1].split("};", 1)[0]
        self.assertIn("sumSqByCh[(size_t)ch] / (double)usedFrames", body)
        self.assertIn("peakByCh[(size_t)ch]", body)
        self.assertIn("for (uint32_t ch = 0; ch < actualChannels; ++ch)", body)
        self.assertNotIn(
            "for (ma_uint32 ch = 0; ch < actualChannels; ++ch)", body
        )
        self.assertIn("++completedPeaks;", body)
        self.assertLess(
            body.index("for (uint32_t ch = 0; ch < actualChannels; ++ch)"),
            body.index("++completedPeaks;"),
        )

    def test_tail_accumulation_uses_standard_channel_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        self.assertIn(
            "const uint32_t actualChannels = decoderFormat.channels;", worker
        )
        start = "coveredPeakCount > requestedPeaksSnapshot) {"
        end = "Platform::AudioDataSource::UninitializeDecoder("
        body = worker.split(start, 1)[1].split(end, 1)[0]
        self.assertIn(
            "std::fill(peakByCh.begin(), peakByCh.end(), 0.0f);", body
        )
        self.assertIn(
            "std::fill(minPeakByCh.begin(), minPeakByCh.end(), 0.0f);", body
        )
        self.assertIn("if (needRms) {", body)
        self.assertIn(
            "std::fill(sumSqByCh.begin(), sumSqByCh.end(), 0.0);", body
        )
        self.assertIn("uint64_t blockFrames = 0;", body)
        self.assertIn("auto appendTailPeak = [&](uint64_t usedFrames) {", body)
        accumulation = body.split("while (true) {", 1)[1]
        self.assertIn("Platform::AudioDataSource::ReadPcmFrames(", body)
        self.assertIn("uint64_t framesRead = readResult.framesRead;", body)
        self.assertIn("for (uint64_t i = 0; i < framesRead; ++i)", body)
        self.assertIn(
            "for (uint32_t ch = 0; ch < actualChannels; ++ch)", accumulation
        )
        self.assertNotIn(
            "for (ma_uint32 ch = 0; ch < actualChannels; ++ch)", accumulation
        )
        self.assertIn(
            "const float s = buffer[(size_t)(i * actualChannels + ch)];", body
        )
        self.assertIn(
            "if (s > peakByCh[(size_t)ch]) peakByCh[(size_t)ch] = s;", body
        )
        self.assertIn(
            "if (s < minPeakByCh[(size_t)ch]) minPeakByCh[(size_t)ch] = s;",
            body,
        )
        self.assertIn(
            "sumSqByCh[(size_t)ch] += (double)s * (double)s;", body
        )
        self.assertIn("if (needTrueRmsForMainWave) {", body)
        self.assertIn("const double sd = (double)s;", body)
        self.assertIn("totalSigmaX2 += sd * sd;", body)
        self.assertIn("totalDecodedSamples += actualChannels;", body)
        self.assertIn("++blockFrames;", body)
        self.assertIn(
            "if (blockFrames >= (uint64_t)samplesPerPeak) {", body
        )
        self.assertIn("appendTailPeak(blockFrames);", body)
        self.assertIn("blockFrames = 0;", body)
        self.assertIn("if (blockFrames > 0) {", body)
        self.assertLess(body.index("++blockFrames;"), body.index("if (blockFrames >= "))
        commit_window = body.split("++blockFrames;", 1)[1]
        self.assertLess(
            commit_window.index("if (blockFrames >= "),
            commit_window.index("appendTailPeak(blockFrames);"),
        )
        self.assertLess(
            commit_window.index("appendTailPeak(blockFrames);"),
            commit_window.index("blockFrames = 0;"),
        )
        self.assertIn(start, worker)
        self.assertIn(end, worker)

    def test_head_average_uses_standard_channel_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        self.assertIn(
            "const uint32_t actualChannels = decoderFormat.channels;", worker
        )
        start = "const int headCount = (std::max)(1, completedPeaks / 10);"
        end = "const float headAvg = (float)(headSum / ((double)headCount * (double)actualChannels));"
        body = worker.split(start, 1)[1].split(end, 1)[0]
        self.assertIn("double headSum = 0.0;", body)
        self.assertIn(
            "headSum += (double)std::abs(wf.peaks[(size_t)(i * (int)actualChannels + (int)ch)]);",
            body,
        )
        self.assertIn("for (uint32_t ch = 0; ch < actualChannels; ++ch)", body)
        self.assertNotIn(
            "for (ma_uint32 ch = 0; ch < actualChannels; ++ch)", body
        )
        self.assertIn(start, worker)
        self.assertIn(end, worker)

    def test_cached_waveform_samples_per_peak_uses_standard_snapshot_type(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        start = (
            "const int cachedPeakCount = "
            "(int)wf.peaks.size() / (std::max)(1, fileChannels);"
        )
        end = "// DB detail"
        body = worker.split(start, 1)[1].split(end, 1)[0]
        self.assertIn("if (cachedPeakCount > 0)", body)
        self.assertIn("std::lock_guard<std::mutex> lk(m_mutex);", body)
        self.assertIn("const uint64_t tf = m_fileInfo.totalFrames;", body)
        self.assertNotIn("const ma_uint64 tf = m_fileInfo.totalFrames;", body)
        self.assertIn(
            "wf.samplesPerPeak = "
            "(int)(tf / static_cast<uint64_t>(cachedPeakCount));",
            body,
        )
        self.assertNotIn(
            "wf.samplesPerPeak = (int)(tf / (ma_uint64)cachedPeakCount);", body
        )
        self.assertIn("if (wf.samplesPerPeak < 1) wf.samplesPerPeak = 1;", body)
        self.assertIn(start, worker)
        self.assertIn(end, worker)

    def test_trailing_silence_scan_uses_standard_channel_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        self.assertIn(
            "const uint32_t actualChannels = decoderFormat.channels;", worker
        )
        start = "int lastSignalPeak = completedPeaks - 1;"
        end = "const int signalPeaks = lastSignalPeak + 1;"
        body = worker.split(start, 1)[1].split(end, 1)[0]
        self.assertIn("for (int i = completedPeaks - 1; i >= 0; --i)", body)
        self.assertIn("bool hasSignal = false;", body)
        self.assertIn(
            "if (std::abs(wf.peaks[(size_t)(i * (int)actualChannels + (int)ch)]) > kSilenceThreshold) {",
            body,
        )
        self.assertIn("hasSignal = true;", body)
        self.assertIn("if (hasSignal) { lastSignalPeak = i; break; }", body)
        self.assertIn("if (i == 0) lastSignalPeak = 0;", body)
        self.assertIn("for (uint32_t ch = 0; ch < actualChannels; ++ch)", body)
        self.assertNotIn(
            "for (ma_uint32 ch = 0; ch < actualChannels; ++ch)", body
        )
        self.assertLess(body.index("hasSignal = true;"), body.index("break;"))
        self.assertIn(start, worker)
        self.assertIn(end, worker)

    def test_heuristic_silence_stretch_uses_standard_channel_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        self.assertIn(
            "const uint32_t actualChannels = decoderFormat.channels;", worker
        )
        start = "const size_t srcPeaks = (size_t)signalPeaks;"
        end = "if (needRms) wf.rms = std::move(stretchedRms);"
        body = worker.split(start, 1)[1].split(end, 1)[0]
        self.assertIn(
            "const size_t dstPeaks = (size_t)requestedPeaksSnapshot;", body
        )
        self.assertEqual(body.count("dstPeaks * (size_t)actualChannels"), 3)
        self.assertIn("if (needRms) stretchedRms.assign", body)
        self.assertIn(
            "const bool hasMinPeaks = (wf.minPeaks.size() == wf.peaks.size());",
            body,
        )
        self.assertIn("for (size_t dst = 0; dst < dstPeaks; ++dst)", body)
        self.assertIn("const size_t src = dst * srcPeaks / dstPeaks;", body)
        self.assertIn("for (uint32_t ch = 0; ch < actualChannels; ++ch)", body)
        self.assertNotIn(
            "for (ma_uint32 ch = 0; ch < actualChannels; ++ch)", body
        )
        self.assertIn("stretched[dst * actualChannels + ch] =", body)
        self.assertIn("wf.peaks[src * actualChannels + ch];", body)
        self.assertIn("if (hasMinPeaks)", body)
        self.assertIn("stretchedMin[dst * actualChannels + ch] =", body)
        self.assertIn("wf.minPeaks[src * actualChannels + ch];", body)
        self.assertIn("if (needRms) {", body)
        self.assertIn("stretchedRms[dst * actualChannels + ch] =", body)
        self.assertIn("wf.rms[src * actualChannels + ch];", body)
        self.assertIn("wf.peaks = std::move(stretched);", body)
        self.assertIn("wf.minPeaks = std::move(stretchedMin);", body)
        self.assertLess(
            body.index("stretchedRms[dst * actualChannels + ch] ="),
            body.index("wf.peaks = std::move(stretched);"),
        )
        self.assertLess(
            body.index("wf.peaks = std::move(stretched);"),
            body.index("wf.minPeaks = std::move(stretchedMin);"),
        )
        self.assertIn(start, worker)
        self.assertIn(end, worker)

    def test_waveform_publish_diagnostic_uses_standard_frame_type(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        start = "// Alignment diagnostic: compare waveform basis vs playback basis"
        end = "const int finalGeneratedPeaks ="
        body = worker.split(start, 1)[1].split(end, 1)[0]
        self.assertIn(
            "const int wfCh = p->numChannels > 0 ? p->numChannels : 1;", body
        )
        self.assertIn("const int wfPeaks = (int)p->peaks.size() / wfCh;", body)
        self.assertIn(
            "const bool hasMinPeaks = !p->minPeaks.empty() && p->minPeaks.size() == p->peaks.size();",
            body,
        )
        self.assertIn('DIAG_ACTION_FMT("AudioAlign",', body)
        self.assertIn(
            "WaveformPublish: path=%s spp=%d peaks=%d wfTotalFrames=%llu fileInfoTotalFrames=%llu",
            body,
        )
        self.assertIn("p->samplesPerPeak, wfPeaks,", body)
        self.assertIn("(unsigned long long)wfTotalFrames,", body)
        self.assertIn("(unsigned long long)m_fileInfo.totalFrames,", body)
        self.assertLess(body.index("const int wfCh ="), body.index("const int wfPeaks ="))
        self.assertLess(
            body.index("const int wfPeaks ="), body.index("const bool hasMinPeaks =")
        )
        self.assertLess(
            body.index("const bool hasMinPeaks ="), body.index('DIAG_ACTION_FMT("AudioAlign",')
        )
        expected = (
            "const uint64_t wfTotalFrames = "
            "static_cast<uint64_t>(p->samplesPerPeak) * "
            "static_cast<uint64_t>(wfPeaks);"
        )
        old = (
            "const ma_uint64 wfTotalFrames = "
            "(ma_uint64)p->samplesPerPeak * (ma_uint64)wfPeaks;"
        )
        self.assertIn(expected, body)
        self.assertNotIn(old, body)
        self.assertIn(start, worker)
        self.assertIn(end, worker)

    def test_cache_hit_stretch_uses_standard_real_frame_arithmetic(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        start = "// Inspect cached detail for zero-filled tails caused by an"
        end = 'normalizeWaveformTailPeaksForStableDisplay("cache_hit");'
        body = worker.split(start, 1)[1].split(end, 1)[0]
        self.assertIn("const int ch = wf.numChannels > 0 ? wf.numChannels : 1;", body)
        self.assertIn("const int totalPeaks = (int)wf.peaks.size() / ch;", body)
        self.assertIn("const int signalPeaks = lastNonZeroPeak + 1;", body)
        self.assertIn(
            "if (kEnableHeuristicSilenceStretch &&\n"
            "                            signalPeaks > 0 && signalPeaks < (int)(totalPeaks * 0.85)) {",
            body,
        )
        self.assertIn("const bool hasMinPeaks = (wf.minPeaks.size() == wf.peaks.size());", body)
        self.assertIn("const bool hasRms = (wf.rms.size() == wf.peaks.size());", body)
        self.assertIn("for (int dst = 0; dst < totalPeaks; ++dst)", body)
        self.assertIn(
            "int src = (int)((int64_t)dst * (int64_t)signalPeaks / (int64_t)totalPeaks);",
            body,
        )
        self.assertIn("stretched[dstIdx] = wf.peaks[srcIdx];", body)
        self.assertIn("if (hasMinPeaks) stretchedMin[dstIdx] = wf.minPeaks[srcIdx];", body)
        self.assertIn("if (hasRms) stretchedRms[dstIdx] = wf.rms[srcIdx];", body)
        self.assertIn("wf.peaks = std::move(stretched);", body)
        self.assertIn("if (hasMinPeaks) wf.minPeaks = std::move(stretchedMin);", body)
        self.assertIn("if (hasRms) wf.rms = std::move(stretchedRms);", body)
        self.assertIn("if (wf.samplesPerPeak < 1) wf.samplesPerPeak = 1;", body)
        self.assertIn('DIAG_ACTION_FMT("Waveform",', body)
        self.assertIn(
            "CacheHitStretch: path=%s signalPeaks=%d totalPeaks=%d realFrames=%llu",
            body,
        )
        self.assertIn("path.c_str(), signalPeaks, totalPeaks, (unsigned long long)realFrames);", body)
        self.assertLess(body.index("wf.peaks = std::move(stretched);"), body.index("realFrames ="))
        self.assertLess(body.index("realFrames ="), body.index("wf.samplesPerPeak ="))
        self.assertLess(body.index("wf.samplesPerPeak ="), body.index("DIAG_ACTION_FMT("))
        expected_real = (
            "const uint64_t realFrames = "
            "static_cast<uint64_t>(wf.samplesPerPeak) * "
            "static_cast<uint64_t>(signalPeaks);"
        )
        expected_spp = "wf.samplesPerPeak = (int)(realFrames / static_cast<uint64_t>(totalPeaks));"
        old_real = "const ma_uint64 realFrames = (ma_uint64)wf.samplesPerPeak * (ma_uint64)signalPeaks;"
        old_spp = "wf.samplesPerPeak = (int)(realFrames / (ma_uint64)totalPeaks);"
        self.assertIn(expected_real, body)
        self.assertIn(expected_spp, body)
        self.assertNotIn(old_real, body)
        self.assertNotIn(old_spp, body)
        self.assertIn(start, worker)
        self.assertIn(end, worker)

    def test_playback_spp64_uses_standard_local_frame_type(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        start = "if (extracted.decodedFrames > 0 && extracted.sampleRate > 0) {"
        end = "// Push-mode decoders may reveal frame count only after detail"
        body = worker.split(start, 1)[1].split(end, 1)[0]
        self.assertIn(
            "const int completed = (int)wf.peaks.size() / (std::max)(1, wf.numChannels);",
            worker,
        )
        self.assertIn(
            "const uint64_t playbackFrames = ConvertFrameCountToSampleRateBasis(",
            body,
        )
        self.assertIn("extracted.decodedFrames,", body)
        self.assertIn("extracted.sampleRate,", body)
        self.assertIn("playbackSampleRate);", body)
        self.assertIn("if (playbackFrames > 0) {", body)
        self.assertIn("m_fileInfo.totalFrames = playbackFrames;", body)
        self.assertIn("if (completed > 0) {", body)
        self.assertIn("spp64 > static_cast<uint64_t>(INT_MAX)", body)
        self.assertIn("std::max<uint64_t>(1, spp64)", body)
        self.assertNotIn("spp64 > (ma_uint64)INT_MAX", body)
        self.assertNotIn("std::max<ma_uint64>(1, spp64)", body)
        self.assertLess(body.index("if (completed > 0) {"), body.index("spp64"))
        self.assertLess(body.index("spp64"), body.index("wf.samplesPerPeak ="))
        expected = "const uint64_t spp64 = playbackFrames / static_cast<uint64_t>(completed);"
        old = "const ma_uint64 spp64 = playbackFrames / (ma_uint64)completed;"
        self.assertIn(expected, body)
        self.assertNotIn(old, body)
        self.assertIn(start, worker)
        self.assertIn(end, worker)

    def test_native_detail_spp64_uses_standard_local_frame_type(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        start = "if (completedPeaks > 0) {"
        end = "m_fileInfo.durationSeconds = (float)decodedDurationSec;"
        body = worker.split(start, 1)[1].split(end, 1)[0]
        self.assertIn(
            "const uint64_t playbackFrames = ConvertFrameCountToSampleRateBasis(",
            worker,
        )
        self.assertIn("m_fileInfo.totalFrames = playbackFrames;", worker)
        self.assertIn("if (completedPeaks > 0) {", worker)
        self.assertIn(
            "const uint64_t spp64 =\n"
            "                                            playbackFrames / static_cast<uint64_t>(completedPeaks);",
            body,
        )
        self.assertIn("spp64 > static_cast<uint64_t>(INT_MAX)", body)
        self.assertIn("std::max<uint64_t>(1, spp64)", body)
        self.assertIn("wf.samplesPerPeak =", body)
        self.assertNotIn("const ma_uint64 spp64 = playbackFrames / (ma_uint64)completedPeaks;", body)
        self.assertNotIn("spp64 > (ma_uint64)INT_MAX", body)
        self.assertNotIn("std::max<ma_uint64>(1, spp64)", body)
        self.assertLess(body.index("const uint64_t spp64"), body.index("wf.samplesPerPeak ="))

    def test_seek_lod_detail_spp64_uses_standard_local_frame_type(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        worker = source.split("void AudioEngine::waveformWorkerLoop()", 1)[1].split(
            "bool AudioEngine::loadFile(", 1
        )[0]
        start = "                        } else {\n                            req = peakIdx;"
        end = "                        const uint32_t sr ="
        body = worker.split(start, 1)[1].split(end, 1)[0]
        self.assertIn("uint64_t realTotalFrames =", worker)
        self.assertIn("req = requestedPeaksSnapshot;", worker)
        self.assertIn(
            "const uint64_t spp64 = realTotalFrames / static_cast<uint64_t>((std::max)(1, req));",
            worker,
        )
        self.assertIn("spp64 > static_cast<uint64_t>(INT_MAX)", body)
        self.assertIn("std::max<uint64_t>(1, spp64)", body)
        self.assertIn("wf.samplesPerPeak =", body)
        self.assertNotIn("const ma_uint64 spp64 = realTotalFrames / (ma_uint64)(std::max)(1, req);", worker)
        self.assertNotIn("spp64 > (ma_uint64)INT_MAX", body)
        self.assertNotIn("std::max)((ma_uint64)1, spp64)", body)

    def test_quick_waveform_uses_standard_internal_loop_indices(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::buildQuickWaveform(", 1)[1].split(
            "void AudioEngine::waveformWorkerLoop()", 1
        )[0]
        self.assertEqual(body.count("for (uint32_t ch = 0;"), 3)
        self.assertIn("for (uint64_t i = 0; i < framesRead; i++)", body)
        self.assertNotIn("for (ma_uint32 ch = 0;", body)
        self.assertNotIn("for (ma_uint64 i = 0; i < framesRead; i++)", body)

    def test_file_quick_waveform_uses_standard_internal_loop_indices(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::generateQuickWaveformForFile(", 1)[1].split(
            "bool AudioEngine::decodeFullFileToMemory", 1
        )[0]
        self.assertEqual(body.count("for (uint32_t ch = 0;"), 3)
        self.assertEqual(body.count("for (uint64_t i = 0;"), 2)
        self.assertNotIn("for (ma_uint32 ch = 0;", body)
        self.assertNotIn("for (ma_uint64 i = 0;", body)

    def test_full_file_waveform_keeps_api_count_and_uses_standard_loop_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::generateWaveformForFile(", 1)[1].split(
            "bool AudioEngine::generateWaveformFromOpenDecoder(", 1
        )[0]
        self.assertIn("generateWaveformFromOpenDecoder(", body)

        helper = source.split("bool AudioEngine::generateWaveformFromOpenDecoder(", 1)[1].split(
            "bool AudioEngine::generateQuickWaveformForFile", 1
        )[0]
        self.assertIn("Platform::AudioDataSource::ReadPcmFrames(", helper)
        self.assertIn("uint64_t framesRead = readResult.framesRead;", helper)
        self.assertIn("for (uint64_t frame = 0; frame < framesRead; ++frame)", helper)
        self.assertNotIn("for (ma_uint64 frame = 0;", helper)

    def test_interleaved_decode_reserve_uses_standard_internal_frame_type(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::DecodeFileToInterleavedF32(", 1)[1].split(
            "bool AudioEngine::computeWaveformAndLoudnessFromSamples(", 1
        )[0]
        self.assertIn("Platform::AudioDataSource::QueryLength(", body)
        self.assertIn("uint64_t totalFrames =", body)
        self.assertIn("static constexpr uint64_t kChunk = 8192;", body)
        self.assertIn("uint64_t framesRead = readResult.framesRead;", body)
        self.assertIn(
            "totalFrames <\n"
            "            static_cast<uint64_t>(std::numeric_limits<size_t>::max()) /\n"
            "                static_cast<uint64_t>(channels)",
            body,
        )
        self.assertIn(
            "const uint64_t reserveFrames = std::min<uint64_t>(\n"
            "            static_cast<uint64_t>(totalFrames), static_cast<uint64_t>(kChunk) * 256);",
            body,
        )
        self.assertIn(
            "outSamples.reserve((size_t)reserveFrames * (size_t)channels);", body
        )
        self.assertNotIn(
            "totalFrames < (ma_uint64)std::numeric_limits<size_t>::max() / (ma_uint64)channels",
            body,
        )
        self.assertNotIn("const ma_uint64 reserveFrames =", body)

    def test_reverse_buffer_keeps_decoder_channel_type_and_uses_standard_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::loadReversedAudio()", 1)[1].split(
            "bool AudioEngine::reloadWithReverseState", 1
        )[0]
        self.assertIn("std::uint32_t channels = m_forceMono ? 1 : 2;", body)
        self.assertIn("for (uint32_t ch = 0; ch < channels; ch++)", body)
        self.assertNotIn("for (ma_uint32 ch = 0; ch < channels; ch++)", body)

    def test_export_reverse_buffer_uses_standard_internal_indices(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::exportRegion(", 1)[1].split(
            "bool AudioEngine::hasAudioAdjustments", 1
        )[0]
        self.assertIn("uint64_t framesRead = 0;", body)
        self.assertIn("Platform::AudioDataSource::ReadPcmFrames(", body)
        self.assertIn("framesRead = readResult.framesRead;", body)
        self.assertIn("for (uint64_t i = 0; i < framesRead / 2; i++)", body)
        self.assertIn("for (uint32_t ch = 0; ch < outChannels; ch++)", body)
        self.assertNotIn("for (ma_uint64 i = 0; i < framesRead / 2; i++)", body)
        self.assertNotIn("for (ma_uint32 ch = 0; ch < outChannels; ch++)", body)

    def test_single_channel_export_keeps_api_count_and_uses_standard_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::exportSingleChannel(", 1)[1]
        self.assertIn("Platform::AudioDataSource::ReadPcmFrames(", body)
        self.assertIn("uint64_t framesRead = readResult.framesRead;", body)
        self.assertIn("for (uint64_t f = 0; f < framesRead; ++f)", body)
        self.assertNotIn("for (ma_uint64 f = 0; f < framesRead; ++f)", body)

    def test_processed_export_keeps_api_count_and_uses_standard_pan_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::exportFileProcessed(", 1)[1].split(
            "bool AudioEngine::isPlaying", 1
        )[0]
        self.assertIn("uint64_t framesRead = 0;", body)
        self.assertIn("for (uint64_t f = 0; f < newFrames; ++f)", body)
        self.assertNotIn("for (ma_uint64 f = 0; f < newFrames; ++f)", body)
        self.assertIn(
            "const uint64_t newGlobalStart = totalInputConsumed + pendingInputFrames;",
            body,
        )
        self.assertNotIn(
            "ma_uint64 newGlobalStart = totalInputConsumed + pendingInputFrames;",
            body,
        )

    def test_export_drain_keeps_frame_counters_and_uses_standard_iteration_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::exportRegion(", 1)[1].split(
            "bool AudioEngine::hasAudioAdjustments", 1
        )[0]
        self.assertIn("const uint64_t maxDrainIterations = 32;", body)
        self.assertIn("uint64_t totalDrainFrames = 0;", body)
        self.assertIn("for (uint64_t iter = 0; iter < maxDrainIterations", body)
        self.assertNotIn("for (ma_uint64 iter = 0; iter < maxDrainIterations", body)

    def test_region_pan_keeps_new_frames_and_uses_standard_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::exportRegion(", 1)[1].split(
            "bool AudioEngine::hasAudioAdjustments", 1
        )[0]
        self.assertIn("uint64_t newFrames = framesRead;", body)
        self.assertIn("uint64_t totalInputConsumed = 0;", body)
        self.assertIn("uint64_t pendingInputFrames = 0;", body)
        self.assertIn("for (uint64_t f = 0; f < newFrames; ++f)", body)
        self.assertNotIn("for (ma_uint64 f = 0; f < newFrames; ++f)", body)
        self.assertIn(
            "const uint64_t newGlobalStart = totalInputConsumed + pendingInputFrames;",
            body,
        )
        self.assertNotIn(
            "ma_uint64 newGlobalStart = totalInputConsumed + pendingInputFrames;",
            body,
        )

    def test_output_device_enumeration_keeps_api_count_and_uses_standard_index(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("std::vector<OutputDeviceInfo> AudioEngine::enumerateOutputDevices(", 1)[1].split(
            "bool AudioEngine::switchOutputDevice", 1
        )[0]
        self.assertIn("Platform::AudioPlayback::EnumerateOutputDevices(", body)
        self.assertIn("for (const auto& device : devices)", body)
        self.assertNotIn("ma_device_info", body)
        self.assertNotIn("for (ma_uint32 i = 0; i < playbackCount; i++)", body)

    def test_playback_frame_cursor_state_uses_standard_uint64(self):
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        for start, end in (
            ("void AudioEngine::setChannelSolo", "void AudioEngine::setMono"),
            ("void AudioEngine::setMono", "void AudioEngine::setPitch"),
            ("bool AudioEngine::setReverse", "bool AudioEngine::loadReversedAudio"),
            ("void AudioEngine::seekWorkerLoop", "void AudioEngine::seekToPosition"),
            ("void AudioEngine::seekToPosition", "bool AudioEngine::isPlaying"),
            ("float AudioEngine::getPlaybackPosition", "float AudioEngine::getOutputLatencyInSeconds"),
            ("bool AudioEngine::switchOutputDevice", "bool AudioEngine::exportSingleChannel"),
        ):
            body = source.split(start, 1)[1].split(end, 1)[0]
            self.assertNotIn("ma_uint64", body)


if __name__ == "__main__":
    unittest.main()
