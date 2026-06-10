#pragma once

#ifdef GOKNAR_DEBUG

#include "EditorPanel.h"

#include "Goknar/Profiling/Profiler.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

class ProfilerPanel : public IEditorPanel
{
public:
	ProfilerPanel(EditorHUD* hud);
	virtual void Draw() override;

private:
	void CaptureEvents();
	void ClearEvents();
	void DrawToolbar();
	void DrawSummary() const;
	void DrawTimeline();
	void DrawHeavyEvents();
	void ExportCurrentCapture();
	void OpenImportFileBrowser();
	void ImportTraceFile(const std::string& path);
	void MergeThreads(const std::vector<Goknar::Debug::ProfileThreadInfo>& threads);
	void TrimStoredEvents();
	void EnsureEventVisible(const Goknar::Debug::ProfileEvent& event);
	void ZoomToEvent(const Goknar::Debug::ProfileEvent& event);
	void OpenEventSourceFile(const Goknar::Debug::ProfileEvent& event);
	void PanTimeline(float deltaPixels, float timelineWidth, std::uint64_t captureStartNs, std::uint64_t captureEndNs);
	void ZoomTimeline(float scale, double anchorNormalized, std::uint64_t captureStartNs, std::uint64_t captureEndNs);
	void ResetZoom();

	Goknar::Debug::ProfileSnapshot BuildSnapshotFromStoredEvents() const;
	const Goknar::Debug::ProfileThreadInfo* FindThreadInfo(std::uint32_t threadIndex) const;
	bool GetCapturedTimeRange(std::uint64_t& outStartNs, std::uint64_t& outEndNs) const;
	std::string GetThreadName(std::uint32_t threadIndex) const;

	std::vector<Goknar::Debug::ProfileEvent> events_{};
	std::vector<Goknar::Debug::ProfileThreadInfo> threads_{};
	std::deque<std::string> importedEventNames_{};
	std::deque<std::string> importedEventFiles_{};
	std::string exportStatus_{};

	std::uint64_t zoomStartNs_{ 0 };
	std::uint64_t zoomEndNs_{ 0 };
	int selectedEventIndex_{ -1 };
	float timelineScrollY_{ 0.0f };
	float minVisibleDurationUs_{ 5.0f };
	bool autoCapture_{ true };
	bool hasZoom_{ false };
	bool isPanningTimeline_{ false };
	bool isWaitingForOneFrameCapture_{ false };
};

#endif
