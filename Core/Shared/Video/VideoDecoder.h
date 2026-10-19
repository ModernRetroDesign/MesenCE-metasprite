#pragma once
#include "pch.h"
#include "Utilities/SimpleLock.h"
#include "Utilities/AutoResetEvent.h"
#include "Shared/SettingTypes.h"
#include "Shared/RenderedFrame.h"
#include "Shared/Video/MetaSpriteCompositor.h"
#include "Shared/Video/DecorativeWidescreen.h"

class BaseVideoFilter;
class ScaleFilter;
class RotateFilter;
class IRenderingDevice;
class Emulator;
class Serializer;

class VideoDecoder
{
private:
	Emulator* _emu;

	ConsoleType _consoleType = ConsoleType::Snes;

	unique_ptr<thread> _decodeThread;

	SimpleLock _stopStartLock;
	AutoResetEvent _waitForFrame;

	atomic<bool> _frameChanged;
	atomic<bool> _stopFlag;
	atomic<bool> _resetBlendHistory { false };
	SimpleLock _rewindBlendLock;
	vector<uint32_t> _pendingRewindBlendHistory;
	atomic<uint64_t> _replacementRevision {0}, _decodedReplacementRevision {UINT64_MAX};
	uint32_t _frameCount = 0;
	bool _forceFilterUpdate = false;

	double _lastAspectRatio = 0.0;
	atomic<double> _decorativeAspectRatio { 0.0 };

	FrameInfo _baseFrameSize = {};
	FrameInfo _lastFrameSize = {};
	RenderedFrame _frame = {};
	atomic<bool> _textPreviewRequested { false };
	SimpleLock _textPreviewLock;
	vector<uint32_t> _textPreviewPixels;
	FrameInfo _textPreviewSize = {};
	uint32_t* _textPreviewSource = nullptr; // Valid until the next decode, protected by _textPreviewLock.
	FrameInfo _textPreviewSourceSize = {}, _textPreviewNativeSize = {};
	OverscanDimensions _textPreviewCrop = {};
	void CaptureTextPreview();
	MetaSpriteCompositor _metaSpriteCompositor;
	DecorativeWidescreen _decorativeWidescreen;

	VideoFilterType _videoFilterType = VideoFilterType::None;
	unique_ptr<BaseVideoFilter> _videoFilter;
	unique_ptr<ScaleFilter> _scaleFilter;
	unique_ptr<RotateFilter> _rotateFilter;

	void UpdateVideoFilter();

	void DecodeThread();

public:
	void RequestTextPreview() { auto lock = _textPreviewLock.AcquireSafe(); _textPreviewPixels.clear(); if(_textPreviewSource) { CaptureTextPreview(); } else { _textPreviewRequested = true; } }
	uint32_t ReadTextPreview(uint32_t* pixels, uint32_t capacity, FrameInfo& size) {
		auto lock = _textPreviewLock.AcquireSafe(); size = _textPreviewSize;
		uint32_t count = (uint32_t)_textPreviewPixels.size();
		if(pixels && capacity >= count) { std::copy(_textPreviewPixels.begin(), _textPreviewPixels.end(), pixels); }
		return count;
	}
	void SetMetaSpriteGroups(std::vector<MetaSpriteReplacement> groups) { _metaSpriteCompositor.SetGroups(std::move(groups)); _replacementRevision++; ResetFrameBlendHistory(); }
	void SetMetaBackgroundLayers(std::vector<MetaBackgroundReplacement> layers) { _metaSpriteCompositor.SetBackgroundLayers(std::move(layers)); _replacementRevision++; ResetFrameBlendHistory(); }
	void SetMetaReplacements(std::vector<MetaSpriteReplacement> groups, std::vector<MetaBackgroundReplacement> layers) { _metaSpriteCompositor.SetReplacements(std::move(groups), std::move(layers)); _replacementRevision++; ResetFrameBlendHistory(); }
	bool HasMetaReplacements() { return _metaSpriteCompositor.HasReplacements(); }
	bool HasMetaTextProfiles() { return _metaSpriteCompositor.HasTextProfiles(); }
	uint8_t GetMetaTextLayerMask() { return _metaSpriteCompositor.TextLayerMask(); }
	void SetAllMetaReplacements(std::vector<MetaSpriteReplacement> groups,std::vector<MetaBackgroundReplacement> layers,std::vector<MetaTextReplacement> texts) { _metaSpriteCompositor.SetAllReplacements(std::move(groups),std::move(layers),std::move(texts)); _replacementRevision++; ResetFrameBlendHistory(); }
	void CaptureMetaSpriteMemory(const MetaSpriteFrame& frame, const std::function<bool(uint32_t,uint8_t&)>& readByte, bool timing = true) { _metaSpriteCompositor.CaptureMemorySnapshot(frame,readByte,timing); }
	bool CaptureSnesFieldCheckpoint(const MetaSpriteFrame& frame,std::vector<uint8_t>& bytes,uint64_t& revision);
	bool RestoreSnesFieldCheckpoint(const std::vector<uint8_t>& bytes,uint64_t revision,std::shared_ptr<MetaSpriteFrame>& frame);
	void SerializeReplacementAnimation(Serializer& s);
	void ResetReplacementAnimation() { _metaSpriteCompositor.RestoreAnimationState({}); }
	bool HasMetaBackgroundLayers() { return _metaSpriteCompositor.HasBackgroundLayers(); }
	VideoDecoder(Emulator* console);
	~VideoDecoder();

	void Init();

	void DecodeFrame(bool synchronous = false);
	void TakeScreenshot(string romName = "");
	void TakeScreenshot(std::stringstream& stream);

	void ForceFilterUpdate() { _forceFilterUpdate = true; }
	void ResetFrameBlendHistory() { auto lock=_rewindBlendLock.AcquireSafe(); _pendingRewindBlendHistory.clear(); _resetBlendHistory.store(true); }
	vector<uint32_t> CaptureRewindBlendHistory();
	void RestoreRewindBlendHistory(vector<uint32_t> history) { auto lock=_rewindBlendLock.AcquireSafe(); _pendingRewindBlendHistory=std::move(history); }

	uint32_t GetFrameCount();
	FrameInfo GetBaseFrameInfo(bool removeOverscan);
	FrameInfo GetFrameInfo();
	double GetLastFrameScale() { return _frame.Scale; }
	double GetDecorativeAspectRatio() { return _decorativeAspectRatio.load(); }

	void UpdateFrame(RenderedFrame frame, bool sync, bool forRewind);

	void WaitForAsyncFrameDecode();

	bool IsRunning();
	void StartThread();
	void StopThread();
};
