#include "ProfilerPanel.h"

#ifdef GOKNAR_DEBUG

#include "imgui.h"

#include "Goknar/Core.h"

#include "UI/EditorHUD.h"
#include "UI/EditorSourceCodeUtils.h"
#include "UI/Panels/SystemFileBrowserPanel.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace
{
	constexpr std::size_t MaxStoredProfilerEvents = 250000;
	constexpr std::size_t MaxProfilerTableRows = 1024;
	constexpr std::size_t MaxTimelineDrawEvents = 8000;
	constexpr float MinTimelineEventPixelWidth = 0.75f;
	constexpr std::uint32_t MaxVisibleDepth = 8;

	constexpr ImGuiID ProfilerEventColumnScope = 0;
	constexpr ImGuiID ProfilerEventColumnDuration = 1;
	constexpr ImGuiID ProfilerEventColumnThread = 2;
	constexpr ImGuiID ProfilerEventColumnFrame = 3;
	constexpr ImGuiID ProfilerEventColumnDepth = 4;

	double NsToMs(std::uint64_t nanoseconds)
	{
		return static_cast<double>(nanoseconds) / 1000000.0;
	}

	double NsToUs(std::uint64_t nanoseconds)
	{
		return static_cast<double>(nanoseconds) / 1000.0;
	}

	double Clamp01(double value)
	{
		return (std::max)(0.0, (std::min)(1.0, value));
	}

	ImU32 LerpColor(const ImVec4& from, const ImVec4& to, float alpha)
	{
		alpha = (std::max)(0.0f, (std::min)(1.0f, alpha));
		const float invAlpha = 1.0f - alpha;
		return ImGui::ColorConvertFloat4ToU32(ImVec4(
			from.x * invAlpha + to.x * alpha,
			from.y * invAlpha + to.y * alpha,
			from.z * invAlpha + to.z * alpha,
			1.0f));
	}

	ImU32 GetDurationColor(std::uint64_t durationNs, std::uint64_t maxDurationNs)
	{
		if (maxDurationNs == 0)
		{
			return IM_COL32(105, 190, 120, 255);
		}

		const float normalizedDuration = static_cast<float>(
			(std::min)(1.0, static_cast<double>(durationNs) / static_cast<double>(maxDurationNs)));
		const ImVec4 cool(0.18f, 0.68f, 0.38f, 1.0f);
		const ImVec4 warm(0.92f, 0.72f, 0.22f, 1.0f);
		const ImVec4 hot(0.92f, 0.22f, 0.18f, 1.0f);

		return normalizedDuration < 0.5f ?
			LerpColor(cool, warm, normalizedDuration * 2.0f) :
			LerpColor(warm, hot, (normalizedDuration - 0.5f) * 2.0f);
	}

	bool IsEventInRange(const Goknar::Debug::ProfileEvent& event, std::uint64_t startNs, std::uint64_t endNs)
	{
		return event.endTimeNs >= startNs && event.startTimeNs <= endNs;
	}

	const char* GetProfileEventName(const Goknar::Debug::ProfileEvent& event)
	{
		return event.name ? event.name : "Unnamed Scope";
	}

	std::string ToDisplayPath(std::filesystem::path path)
	{
		return path.make_preferred().string();
	}

	std::string ResolveProfilerSourcePath(const char* filePath)
	{
		if (!filePath || filePath[0] == '\0')
		{
			return {};
		}

		std::filesystem::path sourcePath(filePath);
		std::vector<std::filesystem::path> candidates;
		if (sourcePath.is_absolute())
		{
			candidates.push_back(sourcePath);
		}
		else
		{
			const std::filesystem::path currentPath = std::filesystem::current_path();
			candidates.push_back(currentPath / sourcePath);
			candidates.push_back(currentPath / "GoknarEngine" / sourcePath);
			candidates.push_back(currentPath / "GoknarEngine" / "Goknar" / sourcePath);
			if (!ProjectDir.empty())
			{
				candidates.push_back(std::filesystem::path(ProjectDir) / sourcePath);
			}
		}

		for (const std::filesystem::path& candidate : candidates)
		{
			std::error_code errorCode;
			if (std::filesystem::exists(candidate, errorCode) && !errorCode)
			{
				const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(candidate, errorCode);
				return ToDisplayPath(errorCode ? candidate : canonicalPath);
			}
		}

		return ToDisplayPath(sourcePath);
	}

	struct JsonValue
	{
		enum class Type
		{
			Null,
			Bool,
			Number,
			String,
			Array,
			Object
		};

		Type type{ Type::Null };
		bool boolValue{ false };
		double numberValue{ 0.0 };
		std::string stringValue{};
		std::vector<JsonValue> arrayValues{};
		std::unordered_map<std::string, JsonValue> objectValues{};
	};

	class JsonParser
	{
	public:
		explicit JsonParser(const std::string& text) :
			text_(text)
		{
		}

		bool Parse(JsonValue& outValue, std::string& outError)
		{
			SkipWhitespace();
			if (!ParseValue(outValue, outError))
			{
				return false;
			}

			SkipWhitespace();
			if (position_ != text_.size())
			{
				outError = "Unexpected trailing JSON content.";
				return false;
			}

			return true;
		}

	private:
		void SkipWhitespace()
		{
			while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_])))
			{
				++position_;
			}
		}

		bool Consume(char expected)
		{
			SkipWhitespace();
			if (position_ >= text_.size() || text_[position_] != expected)
			{
				return false;
			}

			++position_;
			return true;
		}

		bool ParseValue(JsonValue& outValue, std::string& outError)
		{
			SkipWhitespace();
			if (position_ >= text_.size())
			{
				outError = "Unexpected end of JSON.";
				return false;
			}

			switch (text_[position_])
			{
			case '{':
				return ParseObject(outValue, outError);
			case '[':
				return ParseArray(outValue, outError);
			case '"':
				outValue.type = JsonValue::Type::String;
				return ParseString(outValue.stringValue, outError);
			case 't':
				return ParseLiteral("true", JsonValue::Type::Bool, true, outValue, outError);
			case 'f':
				return ParseLiteral("false", JsonValue::Type::Bool, false, outValue, outError);
			case 'n':
				return ParseLiteral("null", JsonValue::Type::Null, false, outValue, outError);
			default:
				return ParseNumber(outValue, outError);
			}
		}

		bool ParseLiteral(const char* literal, JsonValue::Type type, bool boolValue, JsonValue& outValue, std::string& outError)
		{
			const std::size_t literalLength = std::strlen(literal);
			if (text_.compare(position_, literalLength, literal) != 0)
			{
				outError = "Invalid JSON literal.";
				return false;
			}

			position_ += literalLength;
			outValue.type = type;
			outValue.boolValue = boolValue;
			return true;
		}

		bool ParseObject(JsonValue& outValue, std::string& outError)
		{
			if (!Consume('{'))
			{
				outError = "Expected JSON object.";
				return false;
			}

			outValue = JsonValue{};
			outValue.type = JsonValue::Type::Object;
			SkipWhitespace();
			if (position_ < text_.size() && text_[position_] == '}')
			{
				++position_;
				return true;
			}

			while (position_ < text_.size())
			{
				std::string key;
				if (!ParseString(key, outError))
				{
					return false;
				}

				if (!Consume(':'))
				{
					outError = "Expected ':' after JSON object key.";
					return false;
				}

				JsonValue value;
				if (!ParseValue(value, outError))
				{
					return false;
				}

				outValue.objectValues[std::move(key)] = std::move(value);
				SkipWhitespace();
				if (position_ < text_.size() && text_[position_] == '}')
				{
					++position_;
					return true;
				}

				if (!Consume(','))
				{
					outError = "Expected ',' or '}' in JSON object.";
					return false;
				}
			}

			outError = "Unterminated JSON object.";
			return false;
		}

		bool ParseArray(JsonValue& outValue, std::string& outError)
		{
			if (!Consume('['))
			{
				outError = "Expected JSON array.";
				return false;
			}

			outValue = JsonValue{};
			outValue.type = JsonValue::Type::Array;
			SkipWhitespace();
			if (position_ < text_.size() && text_[position_] == ']')
			{
				++position_;
				return true;
			}

			while (position_ < text_.size())
			{
				JsonValue value;
				if (!ParseValue(value, outError))
				{
					return false;
				}

				outValue.arrayValues.push_back(std::move(value));
				SkipWhitespace();
				if (position_ < text_.size() && text_[position_] == ']')
				{
					++position_;
					return true;
				}

				if (!Consume(','))
				{
					outError = "Expected ',' or ']' in JSON array.";
					return false;
				}
			}

			outError = "Unterminated JSON array.";
			return false;
		}

		bool ParseString(std::string& outValue, std::string& outError)
		{
			SkipWhitespace();
			if (position_ >= text_.size() || text_[position_] != '"')
			{
				outError = "Expected JSON string.";
				return false;
			}

			++position_;
			outValue.clear();
			while (position_ < text_.size())
			{
				const char character = text_[position_++];
				if (character == '"')
				{
					return true;
				}

				if (character != '\\')
				{
					outValue.push_back(character);
					continue;
				}

				if (position_ >= text_.size())
				{
					outError = "Unterminated JSON string escape.";
					return false;
				}

				const char escapedCharacter = text_[position_++];
				switch (escapedCharacter)
				{
				case '"':
				case '\\':
				case '/':
					outValue.push_back(escapedCharacter);
					break;
				case 'b':
					outValue.push_back('\b');
					break;
				case 'f':
					outValue.push_back('\f');
					break;
				case 'n':
					outValue.push_back('\n');
					break;
				case 'r':
					outValue.push_back('\r');
					break;
				case 't':
					outValue.push_back('\t');
					break;
				case 'u':
					// The profiler writes ASCII scope names today. Preserve non-ASCII
					// escapes as '?' instead of failing the entire import.
					if (position_ + 4 > text_.size())
					{
						outError = "Invalid JSON unicode escape.";
						return false;
					}
					position_ += 4;
					outValue.push_back('?');
					break;
				default:
					outError = "Invalid JSON string escape.";
					return false;
				}
			}

			outError = "Unterminated JSON string.";
			return false;
		}

		bool ParseNumber(JsonValue& outValue, std::string& outError)
		{
			SkipWhitespace();
			const std::size_t startPosition = position_;

			if (position_ < text_.size() && text_[position_] == '-')
			{
				++position_;
			}

			while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_])))
			{
				++position_;
			}

			if (position_ < text_.size() && text_[position_] == '.')
			{
				++position_;
				while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_])))
				{
					++position_;
				}
			}

			if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E'))
			{
				++position_;
				if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-'))
				{
					++position_;
				}

				while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_])))
				{
					++position_;
				}
			}

			if (startPosition == position_)
			{
				outError = "Expected JSON number.";
				return false;
			}

			try
			{
				outValue = JsonValue{};
				outValue.type = JsonValue::Type::Number;
				outValue.numberValue = std::stod(text_.substr(startPosition, position_ - startPosition));
				return true;
			}
			catch (...)
			{
				outError = "Invalid JSON number.";
				return false;
			}
		}

		const std::string& text_;
		std::size_t position_{ 0 };
	};

	struct ImportedProfileEvent
	{
		std::string name{};
		std::string file{};
		Goknar::Debug::ProfileEvent event{};
	};

	struct ImportedTraceData
	{
		std::vector<ImportedProfileEvent> events{};
		std::vector<Goknar::Debug::ProfileThreadInfo> threads{};
	};

	const JsonValue* FindObjectValue(const JsonValue& objectValue, const char* key)
	{
		if (objectValue.type != JsonValue::Type::Object)
		{
			return nullptr;
		}

		const auto valueIt = objectValue.objectValues.find(key);
		return valueIt != objectValue.objectValues.end() ? &valueIt->second : nullptr;
	}

	bool GetStringField(const JsonValue& objectValue, const char* key, std::string& outValue)
	{
		const JsonValue* value = FindObjectValue(objectValue, key);
		if (!value || value->type != JsonValue::Type::String)
		{
			return false;
		}

		outValue = value->stringValue;
		return true;
	}

	bool GetNumberField(const JsonValue& objectValue, const char* key, double& outValue)
	{
		const JsonValue* value = FindObjectValue(objectValue, key);
		if (!value || value->type != JsonValue::Type::Number)
		{
			return false;
		}

		outValue = value->numberValue;
		return true;
	}

	std::uint64_t MicrosecondsToNanoseconds(double microseconds)
	{
		if (microseconds <= 0.0)
		{
			return 0;
		}

		return static_cast<std::uint64_t>(std::llround(microseconds * 1000.0));
	}

	bool ParseChromeTraceJson(const std::string& jsonText, ImportedTraceData& outTraceData, std::string& outError)
	{
		JsonValue root;
		JsonParser parser(jsonText);
		if (!parser.Parse(root, outError))
		{
			return false;
		}

		const JsonValue* traceEventsValue = FindObjectValue(root, "traceEvents");
		if (!traceEventsValue || traceEventsValue->type != JsonValue::Type::Array)
		{
			outError = "Trace file does not contain a traceEvents array.";
			return false;
		}

		std::unordered_map<std::uint64_t, std::string> threadNamesById;
		std::unordered_map<std::uint64_t, std::uint32_t> fallbackThreadIndicesById;
		std::unordered_map<std::uint32_t, Goknar::Debug::ProfileThreadInfo> threadsByIndex;
		std::uint32_t nextFallbackThreadIndex = 0;

		for (const JsonValue& traceEventValue : traceEventsValue->arrayValues)
		{
			if (traceEventValue.type != JsonValue::Type::Object)
			{
				continue;
			}

			std::string phase;
			std::string name;
			GetStringField(traceEventValue, "ph", phase);
			GetStringField(traceEventValue, "name", name);

			double threadIdNumber = 0.0;
			const bool hasThreadId = GetNumberField(traceEventValue, "tid", threadIdNumber);
			const std::uint64_t threadId = hasThreadId ? static_cast<std::uint64_t>(threadIdNumber) : 0;

			if (phase == "M" && name == "thread_name")
			{
				const JsonValue* argsValue = FindObjectValue(traceEventValue, "args");
				std::string threadName;
				if (argsValue && GetStringField(*argsValue, "name", threadName))
				{
					threadNamesById[threadId] = threadName;
				}
				continue;
			}

			if (phase != "X")
			{
				continue;
			}

			double timestampUs = 0.0;
			double durationUs = 0.0;
			if (name.empty() ||
				!GetNumberField(traceEventValue, "ts", timestampUs) ||
				!GetNumberField(traceEventValue, "dur", durationUs))
			{
				continue;
			}

			const JsonValue* argsValue = FindObjectValue(traceEventValue, "args");
			double frameIndexNumber = 0.0;
			double depthNumber = 0.0;
			double threadIndexNumber = -1.0;
			double lineNumber = 0.0;
			std::string filePath;
			if (argsValue)
			{
				GetNumberField(*argsValue, "frame", frameIndexNumber);
				GetNumberField(*argsValue, "depth", depthNumber);
				GetNumberField(*argsValue, "threadIndex", threadIndexNumber);
				GetStringField(*argsValue, "file", filePath);
				GetNumberField(*argsValue, "line", lineNumber);
			}

			std::uint32_t threadIndex = 0;
			if (threadIndexNumber >= 0.0)
			{
				threadIndex = static_cast<std::uint32_t>(threadIndexNumber);
			}
			else
			{
				auto fallbackIndexIt = fallbackThreadIndicesById.find(threadId);
				if (fallbackIndexIt == fallbackThreadIndicesById.end())
				{
					threadIndex = nextFallbackThreadIndex++;
					fallbackThreadIndicesById[threadId] = threadIndex;
				}
				else
				{
					threadIndex = fallbackIndexIt->second;
				}
			}

			ImportedProfileEvent importedEvent;
			importedEvent.name = name;
			importedEvent.file = std::move(filePath);
			importedEvent.event.name = nullptr;
			importedEvent.event.filePath = nullptr;
			importedEvent.event.startTimeNs = MicrosecondsToNanoseconds(timestampUs);
			importedEvent.event.durationNs = MicrosecondsToNanoseconds(durationUs);
			importedEvent.event.endTimeNs = importedEvent.event.startTimeNs + importedEvent.event.durationNs;
			importedEvent.event.threadId = threadId;
			importedEvent.event.frameIndex = frameIndexNumber > 0.0 ? static_cast<std::uint64_t>(frameIndexNumber) : 0;
			importedEvent.event.threadIndex = threadIndex;
			importedEvent.event.depth = depthNumber > 0.0 ? static_cast<std::uint32_t>(depthNumber) : 0;
			importedEvent.event.lineNumber = lineNumber > 0.0 ? static_cast<std::uint32_t>(lineNumber) : 0;
			outTraceData.events.push_back(std::move(importedEvent));

			Goknar::Debug::ProfileThreadInfo& threadInfo = threadsByIndex[threadIndex];
			threadInfo.index = threadIndex;
			threadInfo.id = threadId;
		}

		for (auto& [threadIndex, threadInfo] : threadsByIndex)
		{
			const auto threadNameIt = threadNamesById.find(threadInfo.id);
			threadInfo.name = threadNameIt != threadNamesById.end() ?
				threadNameIt->second :
				"Thread " + std::to_string(threadIndex);
			outTraceData.threads.push_back(threadInfo);
		}

		std::sort(
			outTraceData.events.begin(),
			outTraceData.events.end(),
			[](const ImportedProfileEvent& left, const ImportedProfileEvent& right)
			{
				return left.event.startTimeNs < right.event.startTimeNs;
			});

		std::sort(
			outTraceData.threads.begin(),
			outTraceData.threads.end(),
			[](const Goknar::Debug::ProfileThreadInfo& left, const Goknar::Debug::ProfileThreadInfo& right)
			{
				return left.index < right.index;
			});

		if (outTraceData.events.empty())
		{
			outError = "No duration events were found in the trace file.";
			return false;
		}

		return true;
	}
}

ProfilerPanel::ProfilerPanel(EditorHUD* hud) :
	IEditorPanel("Profiler", hud)
{
	isOpen_ = false;
	events_.reserve(32768);
}

void ProfilerPanel::Draw()
{
	if (!ImGui::Begin(title_.c_str(), &isOpen_))
	{
		ImGui::End();
		return;
	}

	const ImVec2 currentPos = ImGui::GetWindowPos();
	const ImVec2 currentSize = ImGui::GetWindowSize();
	position_ = Vector2(currentPos.x, currentPos.y);
	size_ = Vector2(currentSize.x, currentSize.y);

	DrawToolbar();

	const bool isOneFrameCapturePending = Goknar::Debug::Profiler::IsCaptureOneFramePending();
	const bool isCapturingOneFrame = Goknar::Debug::Profiler::IsCapturingOneFrame();
	if (isWaitingForOneFrameCapture_ && !isOneFrameCapturePending && !isCapturingOneFrame && !Goknar::Debug::Profiler::IsEnabled())
	{
		CaptureEvents();
		isWaitingForOneFrameCapture_ = false;
		exportStatus_ = "Captured one frame";
	}
	else if (autoCapture_ && Goknar::Debug::Profiler::IsEnabled() && !isCapturingOneFrame)
	{
		CaptureEvents();
	}

	DrawSummary();
	DrawTimeline();
	DrawHeavyEvents();

	ImGui::End();
}

void ProfilerPanel::CaptureEvents()
{
	Goknar::Debug::ProfileSnapshot snapshot = Goknar::Debug::Profiler::CaptureSnapshot();
	MergeThreads(snapshot.threads);

	if (!snapshot.events.empty())
	{
		events_.insert(events_.end(), snapshot.events.begin(), snapshot.events.end());
		TrimStoredEvents();
	}
}

void ProfilerPanel::ClearEvents()
{
	Goknar::Debug::Profiler::Clear();
	events_.clear();
	threads_.clear();
	importedEventNames_.clear();
	importedEventFiles_.clear();
	exportStatus_.clear();
	selectedEventIndex_ = -1;
	isWaitingForOneFrameCapture_ = false;
	ResetZoom();
}

void ProfilerPanel::DrawToolbar()
{
	bool isEnabled = Goknar::Debug::Profiler::IsEnabled();
	if (ImGui::Checkbox("Enabled", &isEnabled))
	{
		Goknar::Debug::Profiler::SetEnabled(isEnabled);
	}

	ImGui::SameLine();
	ImGui::Checkbox("Auto Flush", &autoCapture_);

	ImGui::SameLine();
	if (ImGui::Button("Flush"))
	{
		CaptureEvents();
	}

	ImGui::SameLine();
	if (ImGui::Button("Capture One Frame"))
	{
		events_.clear();
		threads_.clear();
		importedEventNames_.clear();
		importedEventFiles_.clear();
		exportStatus_ = "One-frame capture armed";
		selectedEventIndex_ = -1;
		isWaitingForOneFrameCapture_ = true;
		ResetZoom();
		Goknar::Debug::Profiler::CaptureOneFrame();
	}

	ImGui::SameLine();
	if (ImGui::Button("Clear"))
	{
		ClearEvents();
	}

	std::uint64_t captureStartNs = 0;
	std::uint64_t captureEndNs = 0;
	const bool hasCapturedRange = GetCapturedTimeRange(captureStartNs, captureEndNs);

	ImGui::SameLine();
	if (ImGui::Button("Zoom In") && hasCapturedRange)
	{
		ZoomTimeline(0.5f, 0.5, captureStartNs, captureEndNs);
	}

	ImGui::SameLine();
	if (ImGui::Button("Zoom Out") && hasCapturedRange)
	{
		ZoomTimeline(2.0f, 0.5, captureStartNs, captureEndNs);
	}

	ImGui::SameLine();
	if (ImGui::Button("Reset Zoom"))
	{
		ResetZoom();
	}

	ImGui::SameLine();
	if (ImGui::Button("Export Chrome Trace JSON"))
	{
		ExportCurrentCapture();
	}

	ImGui::SameLine();
	if (ImGui::Button("Import Chrome Trace JSON"))
	{
		OpenImportFileBrowser();
	}

	ImGui::SetNextItemWidth(180.0f);
	ImGui::SliderFloat("Min Visible Duration (us)", &minVisibleDurationUs_, 0.0f, 500.0f, "%.1f");

	if (!exportStatus_.empty())
	{
		ImGui::SameLine();
		ImGui::TextUnformatted(exportStatus_.c_str());
	}

	const bool isOneFrameCapturePending = Goknar::Debug::Profiler::IsCaptureOneFramePending();
	const bool isCapturingOneFrame = Goknar::Debug::Profiler::IsCapturingOneFrame();
	if (isOneFrameCapturePending || isCapturingOneFrame)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("%s", isOneFrameCapturePending ? "Waiting for next frame" : "Capturing frame");
	}
}

void ProfilerPanel::DrawSummary() const
{
	std::uint64_t totalDurationNs = 0;
	std::uint64_t maxDurationNs = 0;
	std::uint64_t firstTimestampNs = 0;
	std::uint64_t lastTimestampNs = 0;
	std::uint64_t lastFrameIndex = 0;

	for (const Goknar::Debug::ProfileEvent& event : events_)
	{
		totalDurationNs += event.durationNs;
		maxDurationNs = (std::max)(maxDurationNs, event.durationNs);
		lastFrameIndex = (std::max)(lastFrameIndex, event.frameIndex);

		if (firstTimestampNs == 0 || event.startTimeNs < firstTimestampNs)
		{
			firstTimestampNs = event.startTimeNs;
		}

		lastTimestampNs = (std::max)(lastTimestampNs, event.endTimeNs);
	}

	ImGui::Separator();
	ImGui::Text(
		"Events: %zu | Threads: %zu | Total captured: %.3f ms | Longest scope: %.3f ms | Frame: %llu",
		events_.size(),
		threads_.size(),
		NsToMs(totalDurationNs),
		NsToMs(maxDurationNs),
		static_cast<unsigned long long>(lastFrameIndex));

	if (firstTimestampNs != 0 && lastTimestampNs > firstTimestampNs)
	{
		ImGui::Text("Captured range: %.3f ms", NsToMs(lastTimestampNs - firstTimestampNs));
	}
}

void ProfilerPanel::DrawTimeline()
{
	if (events_.empty())
	{
		ImGui::Separator();
		ImGui::TextDisabled("No captured profiling events.");
		return;
	}

	std::uint64_t firstTimestampNs = std::numeric_limits<std::uint64_t>::max();
	std::uint64_t lastTimestampNs = 0;
	std::uint64_t maxDurationNs = 0;

	std::vector<std::uint32_t> threadRows;
	threadRows.reserve(threads_.size());
	for (const Goknar::Debug::ProfileThreadInfo& threadInfo : threads_)
	{
		threadRows.push_back(threadInfo.index);
	}

	for (const Goknar::Debug::ProfileEvent& event : events_)
	{
		firstTimestampNs = (std::min)(firstTimestampNs, event.startTimeNs);
		lastTimestampNs = (std::max)(lastTimestampNs, event.endTimeNs);
		maxDurationNs = (std::max)(maxDurationNs, event.durationNs);

		if (std::find(threadRows.begin(), threadRows.end(), event.threadIndex) == threadRows.end())
		{
			threadRows.push_back(event.threadIndex);
		}
	}

	if (firstTimestampNs == std::numeric_limits<std::uint64_t>::max())
	{
		firstTimestampNs = 0;
	}

	std::sort(threadRows.begin(), threadRows.end());
	const std::uint64_t viewStartNs = hasZoom_ ? zoomStartNs_ : firstTimestampNs;
	std::uint64_t viewEndNs = hasZoom_ ? zoomEndNs_ : lastTimestampNs;
	if (viewEndNs <= viewStartNs)
	{
		viewEndNs = viewStartNs + 1;
	}

	std::vector<int> heavyEventIndices;
	heavyEventIndices.reserve(events_.size());
	for (int eventIndex = 0; eventIndex < static_cast<int>(events_.size()); ++eventIndex)
	{
		if (IsEventInRange(events_[eventIndex], viewStartNs, viewEndNs))
		{
			heavyEventIndices.push_back(eventIndex);
		}
	}

	std::sort(
		heavyEventIndices.begin(),
		heavyEventIndices.end(),
		[this](int leftIndex, int rightIndex)
		{
			return events_[leftIndex].durationNs > events_[rightIndex].durationNs;
		});
	if (heavyEventIndices.size() > 8)
	{
		heavyEventIndices.resize(8);
	}

	ImGui::Separator();
	const float timelineHeight = 360.0f;
	std::size_t timelineVisibleEventCount = 0;
	std::size_t timelineDrawEventCount = 0;
	if (ImGui::BeginChild("ProfilerTimeline", ImVec2(0.0f, timelineHeight), true, ImGuiWindowFlags_HorizontalScrollbar))
	{
		const float labelWidth = 170.0f;
		const float rowHeight = 18.0f + static_cast<float>(MaxVisibleDepth) * 11.0f;
		const float canvasWidth = (std::max)(ImGui::GetContentRegionAvail().x, 760.0f);
		const float canvasHeight = (std::max)(timelineHeight - 28.0f, 28.0f + rowHeight * static_cast<float>(threadRows.size()));
		const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		const ImVec2 canvasSize(canvasWidth, canvasHeight);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const float timelineScrollYBeforeWheel = timelineScrollY_;
		float nextTimelineScrollY = ImGui::GetScrollY();

		ImGui::InvisibleButton("ProfilerTimelineCanvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
		const bool canvasHovered = ImGui::IsItemHovered();
		const bool canvasClicked = canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		const bool canvasDoubleClicked = canvasHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		const ImVec2 mousePos = ImGui::GetMousePos();
		if (canvasHovered || ImGui::IsItemActive())
		{
			ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
		}

		drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(24, 24, 27, 255));
		drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(62, 62, 68, 255));

		const float timelineLeft = canvasPos.x + labelWidth;
		const float timelineRight = canvasPos.x + canvasSize.x - 8.0f;
		const float timelineWidth = (std::max)(1.0f, timelineRight - timelineLeft);
		const double pixelsPerNs = static_cast<double>(timelineWidth) / static_cast<double>(viewEndNs - viewStartNs);

		if (canvasHovered && ImGui::GetIO().MouseWheel != 0.0f)
		{
			const double anchorNormalized = Clamp01(static_cast<double>(mousePos.x - timelineLeft) / static_cast<double>(timelineWidth));
			const float zoomScale = static_cast<float>(std::pow(0.85, static_cast<double>(ImGui::GetIO().MouseWheel)));
			ZoomTimeline(zoomScale, anchorNormalized, firstTimestampNs, lastTimestampNs);
			nextTimelineScrollY = (std::min)(timelineScrollYBeforeWheel, ImGui::GetScrollMaxY());
			ImGui::SetScrollY(nextTimelineScrollY);
		}

		if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
		{
			isPanningTimeline_ = true;
		}

		if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			isPanningTimeline_ = false;
		}

		if (isPanningTimeline_)
		{
			const ImVec2 dragDelta = ImGui::GetIO().MouseDelta;
			const float dragDeltaX = dragDelta.x;
			if (dragDeltaX != 0.0f)
			{
				PanTimeline(dragDeltaX, timelineWidth, firstTimestampNs, lastTimestampNs);
			}

			if (dragDelta.y != 0.0f)
			{
				const float newScrollY = (std::max)(0.0f, (std::min)(ImGui::GetScrollMaxY(), ImGui::GetScrollY() - dragDelta.y));
				ImGui::SetScrollY(newScrollY);
				nextTimelineScrollY = newScrollY;
			}

			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
		}

		timelineScrollY_ = nextTimelineScrollY;

		for (int markerIndex = 0; markerIndex <= 10; ++markerIndex)
		{
			const float alpha = static_cast<float>(markerIndex) / 10.0f;
			const float markerX = timelineLeft + timelineWidth * alpha;
			drawList->AddLine(ImVec2(markerX, canvasPos.y), ImVec2(markerX, canvasPos.y + canvasSize.y), IM_COL32(52, 52, 58, 255));

			const double markerMs = NsToMs(static_cast<std::uint64_t>(
				static_cast<double>(viewEndNs - viewStartNs) * static_cast<double>(alpha)));
			char markerLabel[32];
			std::snprintf(markerLabel, sizeof(markerLabel), "%.2f ms", markerMs);
			drawList->AddText(ImVec2(markerX + 3.0f, canvasPos.y + 4.0f), IM_COL32(160, 160, 168, 255), markerLabel);
		}

		std::unordered_map<std::uint32_t, int> threadRowByIndex;
		for (int rowIndex = 0; rowIndex < static_cast<int>(threadRows.size()); ++rowIndex)
		{
			threadRowByIndex[threadRows[rowIndex]] = rowIndex;
			const float rowY = canvasPos.y + 24.0f + rowHeight * static_cast<float>(rowIndex);
			drawList->AddRectFilled(
				ImVec2(canvasPos.x, rowY),
				ImVec2(canvasPos.x + canvasSize.x, rowY + rowHeight - 2.0f),
				rowIndex % 2 == 0 ? IM_COL32(31, 31, 36, 255) : IM_COL32(27, 27, 32, 255));

			const std::string threadName = GetThreadName(threadRows[rowIndex]);
			drawList->AddText(ImVec2(canvasPos.x + 8.0f, rowY + 6.0f), IM_COL32(220, 220, 225, 255), threadName.c_str());
		}

		auto isHeavyEvent = [&heavyEventIndices](int eventIndex)
		{
			return std::find(heavyEventIndices.begin(), heavyEventIndices.end(), eventIndex) != heavyEventIndices.end();
		};

		std::vector<int> pinnedTimelineEventIndices;
		std::vector<int> sampledTimelineEventIndices;
		pinnedTimelineEventIndices.reserve(heavyEventIndices.size() + 1);
		sampledTimelineEventIndices.reserve((std::min)(events_.size(), MaxTimelineDrawEvents));

		for (int eventIndex = 0; eventIndex < static_cast<int>(events_.size()); ++eventIndex)
		{
			const Goknar::Debug::ProfileEvent& event = events_[eventIndex];
			if (!IsEventInRange(event, viewStartNs, viewEndNs) || NsToUs(event.durationNs) < static_cast<double>(minVisibleDurationUs_))
			{
				continue;
			}

			auto rowIt = threadRowByIndex.find(event.threadIndex);
			if (rowIt == threadRowByIndex.end())
			{
				continue;
			}

			const double startOffsetNs = event.startTimeNs <= viewStartNs ? 0.0 : static_cast<double>(event.startTimeNs - viewStartNs);
			const double endOffsetNs = event.endTimeNs <= viewStartNs ? 0.0 : static_cast<double>(event.endTimeNs - viewStartNs);
			const float x0 = timelineLeft + static_cast<float>(startOffsetNs * pixelsPerNs);
			const float x1 = timelineLeft + static_cast<float>(endOffsetNs * pixelsPerNs);
			if (x1 < timelineLeft || x0 > timelineRight)
			{
				continue;
			}

			++timelineVisibleEventCount;

			const bool isPinned = selectedEventIndex_ == eventIndex || isHeavyEvent(eventIndex);
			if (x1 - x0 < MinTimelineEventPixelWidth && !isPinned)
			{
				continue;
			}

			if (isPinned)
			{
				pinnedTimelineEventIndices.push_back(eventIndex);
			}
			else
			{
				sampledTimelineEventIndices.push_back(eventIndex);
			}
		}

		if (pinnedTimelineEventIndices.size() < MaxTimelineDrawEvents)
		{
			const std::size_t remainingDrawSlots = MaxTimelineDrawEvents - pinnedTimelineEventIndices.size();
			if (sampledTimelineEventIndices.size() > remainingDrawSlots)
			{
				std::nth_element(
					sampledTimelineEventIndices.begin(),
					sampledTimelineEventIndices.begin() + remainingDrawSlots,
					sampledTimelineEventIndices.end(),
					[this](int leftIndex, int rightIndex)
					{
						return events_[leftIndex].durationNs > events_[rightIndex].durationNs;
					});
				sampledTimelineEventIndices.resize(remainingDrawSlots);
			}
		}
		else
		{
			sampledTimelineEventIndices.clear();
		}

		std::vector<int> timelineEventIndices;
		timelineEventIndices.reserve(pinnedTimelineEventIndices.size() + sampledTimelineEventIndices.size());
		timelineEventIndices.insert(timelineEventIndices.end(), pinnedTimelineEventIndices.begin(), pinnedTimelineEventIndices.end());
		timelineEventIndices.insert(timelineEventIndices.end(), sampledTimelineEventIndices.begin(), sampledTimelineEventIndices.end());
		std::sort(
			timelineEventIndices.begin(),
			timelineEventIndices.end(),
			[this, &threadRowByIndex](int leftIndex, int rightIndex)
			{
				const Goknar::Debug::ProfileEvent& left = events_[leftIndex];
				const Goknar::Debug::ProfileEvent& right = events_[rightIndex];
				const int leftRow = threadRowByIndex.at(left.threadIndex);
				const int rightRow = threadRowByIndex.at(right.threadIndex);
				if (leftRow != rightRow)
				{
					return leftRow < rightRow;
				}

				if (left.depth != right.depth)
				{
					return left.depth < right.depth;
				}

				if (left.startTimeNs != right.startTimeNs)
				{
					return left.startTimeNs < right.startTimeNs;
				}

				return leftIndex < rightIndex;
			});
		timelineDrawEventCount = timelineEventIndices.size();

		int hoveredEventIndex = -1;
		for (int eventIndex : timelineEventIndices)
		{
			const Goknar::Debug::ProfileEvent& event = events_[eventIndex];
			auto rowIt = threadRowByIndex.find(event.threadIndex);
			if (rowIt == threadRowByIndex.end())
			{
				continue;
			}

			const double startOffsetNs = event.startTimeNs <= viewStartNs ? 0.0 : static_cast<double>(event.startTimeNs - viewStartNs);
			const double endOffsetNs = event.endTimeNs <= viewStartNs ? 0.0 : static_cast<double>(event.endTimeNs - viewStartNs);
			const float x0 = timelineLeft + static_cast<float>(startOffsetNs * pixelsPerNs);
			float x1 = timelineLeft + static_cast<float>(endOffsetNs * pixelsPerNs);
			if (x1 < timelineLeft || x0 > timelineRight)
			{
				continue;
			}

			x1 = (std::max)(x1, x0 + 1.0f);
			const float clampedX0 = (std::max)(timelineLeft, x0);
			const float clampedX1 = (std::min)(timelineRight, x1);
			const std::uint32_t visibleDepth = (std::min)(event.depth, MaxVisibleDepth - 1);
			const float rowY = canvasPos.y + 24.0f + rowHeight * static_cast<float>(rowIt->second);
			const float y0 = rowY + 18.0f + static_cast<float>(visibleDepth) * 11.0f;
			const float y1 = y0 + 8.0f;
			const ImVec2 rectMin(clampedX0, y0);
			const ImVec2 rectMax(clampedX1, y1);
			const bool isHeavy = isHeavyEvent(eventIndex);
			const bool isSelected = selectedEventIndex_ == eventIndex;

			drawList->AddRectFilled(rectMin, rectMax, GetDurationColor(event.durationNs, maxDurationNs));
			if (isHeavy || isSelected)
			{
				drawList->AddRect(
					ImVec2(rectMin.x - 1.0f, rectMin.y - 1.0f),
					ImVec2(rectMax.x + 1.0f, rectMax.y + 1.0f),
					isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 220, 90, 255));
			}

			if (canvasHovered &&
				rectMin.x <= mousePos.x && mousePos.x <= rectMax.x &&
				rectMin.y <= mousePos.y && mousePos.y <= rectMax.y)
			{
				if (hoveredEventIndex < 0 || event.durationNs < events_[hoveredEventIndex].durationNs)
				{
					hoveredEventIndex = eventIndex;
				}
			}
		}

		if (hoveredEventIndex >= 0)
		{
			const Goknar::Debug::ProfileEvent& hoveredEvent = events_[hoveredEventIndex];
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(hoveredEvent.name ? hoveredEvent.name : "Unnamed Scope");
			ImGui::Text("Duration: %.3f ms", NsToMs(hoveredEvent.durationNs));
			ImGui::Text("Thread: %s", GetThreadName(hoveredEvent.threadIndex).c_str());
			ImGui::Text("Frame: %llu | Depth: %u",
				static_cast<unsigned long long>(hoveredEvent.frameIndex),
				hoveredEvent.depth);
			ImGui::EndTooltip();

			if (canvasDoubleClicked)
			{
				selectedEventIndex_ = hoveredEventIndex;
				ZoomToEvent(hoveredEvent);
			}
			else if (canvasClicked)
			{
				selectedEventIndex_ = hoveredEventIndex;
				EnsureEventVisible(hoveredEvent);
			}
		}
	}
	ImGui::EndChild();

	if (timelineVisibleEventCount > timelineDrawEventCount)
	{
		ImGui::TextDisabled(
			"Timeline drawing %zu of %zu visible events. Zoom in or raise min duration for more detail.",
			timelineDrawEventCount,
			timelineVisibleEventCount);
	}
}

void ProfilerPanel::DrawHeavyEvents()
{
	if (events_.empty())
	{
		return;
	}

	std::uint64_t viewStartNs = hasZoom_ ? zoomStartNs_ : 0;
	std::uint64_t viewEndNs = hasZoom_ ? zoomEndNs_ : std::numeric_limits<std::uint64_t>::max();
	std::vector<int> eventIndices;
	eventIndices.reserve(events_.size());
	for (int eventIndex = 0; eventIndex < static_cast<int>(events_.size()); ++eventIndex)
	{
		if (IsEventInRange(events_[eventIndex], viewStartNs, viewEndNs))
		{
			eventIndices.push_back(eventIndex);
		}
	}

	ImGui::Separator();
	const ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_Sortable;
	if (ImGui::BeginTable("ProfilerHeavyEvents", 5, tableFlags))
	{
		ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch, 0.0f, ProfilerEventColumnScope);
		ImGui::TableSetupColumn(
			"Duration",
			ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending,
			0.0f,
			ProfilerEventColumnDuration);
		ImGui::TableSetupColumn("Thread", ImGuiTableColumnFlags_WidthFixed, 120.0f, ProfilerEventColumnThread);
		ImGui::TableSetupColumn("Frame", ImGuiTableColumnFlags_WidthFixed, 70.0f, ProfilerEventColumnFrame);
		ImGui::TableSetupColumn("Depth", ImGuiTableColumnFlags_WidthFixed, 56.0f, ProfilerEventColumnDepth);
		ImGui::TableHeadersRow();

		std::unordered_map<std::uint32_t, std::string> threadNameCache;
		auto getThreadSortName = [this, &threadNameCache](std::uint32_t threadIndex) -> const std::string&
		{
			auto [threadIt, didInsert] = threadNameCache.emplace(threadIndex, std::string{});
			if (didInsert)
			{
				threadIt->second = GetThreadName(threadIndex);
			}

			return threadIt->second;
		};

		auto compareValues = [](auto left, auto right) -> int
		{
			if (left < right)
			{
				return -1;
			}

			if (right < left)
			{
				return 1;
			}

			return 0;
		};

		auto compareEventsByColumn =
			[&](const Goknar::Debug::ProfileEvent& left, const Goknar::Debug::ProfileEvent& right, ImGuiID columnUserId) -> int
		{
			switch (columnUserId)
			{
			case ProfilerEventColumnScope:
				return std::strcmp(GetProfileEventName(left), GetProfileEventName(right));
			case ProfilerEventColumnDuration:
				return compareValues(left.durationNs, right.durationNs);
			case ProfilerEventColumnThread:
			{
				const std::string& leftThreadName = getThreadSortName(left.threadIndex);
				const std::string& rightThreadName = getThreadSortName(right.threadIndex);
				const int threadNameCompare = leftThreadName.compare(rightThreadName);
				return threadNameCompare != 0 ? threadNameCompare : compareValues(left.threadIndex, right.threadIndex);
			}
			case ProfilerEventColumnFrame:
				return compareValues(left.frameIndex, right.frameIndex);
			case ProfilerEventColumnDepth:
				return compareValues(left.depth, right.depth);
			default:
				return compareValues(left.durationNs, right.durationNs);
			}
		};

		const ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
		std::sort(
			eventIndices.begin(),
			eventIndices.end(),
			[&, sortSpecs](int leftIndex, int rightIndex)
			{
				const Goknar::Debug::ProfileEvent& left = events_[leftIndex];
				const Goknar::Debug::ProfileEvent& right = events_[rightIndex];

				if (sortSpecs && sortSpecs->SpecsCount > 0)
				{
					for (int sortIndex = 0; sortIndex < sortSpecs->SpecsCount; ++sortIndex)
					{
						const ImGuiTableColumnSortSpecs& sortSpec = sortSpecs->Specs[sortIndex];
						if (sortSpec.SortDirection == ImGuiSortDirection_None)
						{
							continue;
						}

						const int comparison = compareEventsByColumn(left, right, sortSpec.ColumnUserID);
						if (comparison != 0)
						{
							return sortSpec.SortDirection == ImGuiSortDirection_Ascending ? comparison < 0 : comparison > 0;
						}
					}
				}

				const int durationCompare = compareValues(left.durationNs, right.durationNs);
				if (durationCompare != 0)
				{
					return durationCompare > 0;
				}

				const int startTimeCompare = compareValues(left.startTimeNs, right.startTimeNs);
				if (startTimeCompare != 0)
				{
					return startTimeCompare < 0;
				}

				return leftIndex < rightIndex;
			});

		if (eventIndices.size() > MaxProfilerTableRows)
		{
			eventIndices.resize(MaxProfilerTableRows);
		}

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(eventIndices.size()));
		while (clipper.Step())
		{
			for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex)
			{
				const int eventIndex = eventIndices[rowIndex];
				const Goknar::Debug::ProfileEvent& event = events_[eventIndex];
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				const bool isSelected = selectedEventIndex_ == eventIndex;
				ImGui::PushID(eventIndex);
				const ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick;
				if (ImGui::Selectable(GetProfileEventName(event), isSelected, selectableFlags))
				{
					selectedEventIndex_ = eventIndex;
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						OpenEventSourceFile(event);
					}
					else
					{
						EnsureEventVisible(event);
					}
				}
				ImGui::PopID();

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.3f ms", NsToMs(event.durationNs));

				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(GetThreadName(event.threadIndex).c_str());

				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%llu", static_cast<unsigned long long>(event.frameIndex));

				ImGui::TableSetColumnIndex(4);
				ImGui::Text("%u", event.depth);
			}
		}

		ImGui::EndTable();
	}
}

void ProfilerPanel::ExportCurrentCapture()
{
	CaptureEvents();

	Goknar::Debug::ProfileSnapshot snapshot = BuildSnapshotFromStoredEvents();
	std::filesystem::path outputDirectory = ProjectDir.empty() ? std::filesystem::current_path() : std::filesystem::path(ProjectDir);
	std::error_code errorCode;
	if (!std::filesystem::exists(outputDirectory, errorCode) || !std::filesystem::is_directory(outputDirectory, errorCode))
	{
		outputDirectory = std::filesystem::current_path();
	}

	const std::filesystem::path outputPath = outputDirectory / "ProfilerTrace.json";
	const bool didExport = Goknar::Debug::Profiler::WriteChromeTrace(outputPath.generic_string(), snapshot);
	exportStatus_ = didExport ? "Exported: " + outputPath.generic_string() : "Export failed";
}

void ProfilerPanel::OpenImportFileBrowser()
{
	SystemFileBrowserPanel* fileBrowser = hud_ ? hud_->GetPanel<SystemFileBrowserPanel>() : nullptr;
	if (!fileBrowser)
	{
		exportStatus_ = "Import failed: file browser unavailable";
		return;
	}

	const std::string initialDirectory = ProjectDir.empty() ? std::filesystem::current_path().string() : ProjectDir;
	fileBrowser->OpenFileSelector(
		Delegate<void(const std::string&)>::Create<ProfilerPanel, &ProfilerPanel::ImportTraceFile>(this),
		initialDirectory);
}

void ProfilerPanel::ImportTraceFile(const std::string& path)
{
	std::ifstream input(path, std::ios::in | std::ios::binary);
	if (!input.is_open())
	{
		exportStatus_ = "Import failed: could not open file";
		return;
	}

	std::stringstream buffer;
	buffer << input.rdbuf();

	ImportedTraceData importedTraceData;
	std::string importError;
	if (!ParseChromeTraceJson(buffer.str(), importedTraceData, importError))
	{
		exportStatus_ = "Import failed: " + importError;
		return;
	}

	Goknar::Debug::Profiler::SetEnabled(false);
	isWaitingForOneFrameCapture_ = false;
	events_.clear();
	threads_ = std::move(importedTraceData.threads);
	importedEventNames_.clear();
	importedEventFiles_.clear();
	importedEventNames_.resize(importedTraceData.events.size());
	importedEventFiles_.resize(importedTraceData.events.size());
	events_.reserve(importedTraceData.events.size());

	for (std::size_t eventIndex = 0; eventIndex < importedTraceData.events.size(); ++eventIndex)
	{
		importedEventNames_[eventIndex] = std::move(importedTraceData.events[eventIndex].name);
		importedEventFiles_[eventIndex] = std::move(importedTraceData.events[eventIndex].file);
		Goknar::Debug::ProfileEvent event = importedTraceData.events[eventIndex].event;
		event.name = importedEventNames_[eventIndex].c_str();
		event.filePath = importedEventFiles_[eventIndex].empty() ? nullptr : importedEventFiles_[eventIndex].c_str();
		events_.push_back(event);
	}

	selectedEventIndex_ = -1;
	ResetZoom();
	exportStatus_ = "Imported: " + path;
}

void ProfilerPanel::MergeThreads(const std::vector<Goknar::Debug::ProfileThreadInfo>& threads)
{
	for (const Goknar::Debug::ProfileThreadInfo& threadInfo : threads)
	{
		auto existingThreadIt = std::find_if(
			threads_.begin(),
			threads_.end(),
			[&threadInfo](const Goknar::Debug::ProfileThreadInfo& candidate)
			{
				return candidate.index == threadInfo.index;
			});

		if (existingThreadIt == threads_.end())
		{
			threads_.push_back(threadInfo);
		}
		else
		{
			*existingThreadIt = threadInfo;
		}
	}
}

void ProfilerPanel::TrimStoredEvents()
{
	if (events_.size() <= MaxStoredProfilerEvents)
	{
		return;
	}

	const std::size_t removeCount = events_.size() - MaxStoredProfilerEvents;
	events_.erase(events_.begin(), events_.begin() + static_cast<std::ptrdiff_t>(removeCount));
	selectedEventIndex_ = -1;
	ResetZoom();
}

void ProfilerPanel::EnsureEventVisible(const Goknar::Debug::ProfileEvent& event)
{
	std::uint64_t captureStartNs = 0;
	std::uint64_t captureEndNs = 0;
	if (!GetCapturedTimeRange(captureStartNs, captureEndNs))
	{
		return;
	}

	std::uint64_t viewStartNs = hasZoom_ ? zoomStartNs_ : captureStartNs;
	std::uint64_t viewEndNs = hasZoom_ ? zoomEndNs_ : captureEndNs;
	if (viewEndNs <= viewStartNs)
	{
		viewEndNs = viewStartNs + 1;
	}

	const std::uint64_t currentViewDurationNs = viewEndNs - viewStartNs;
	const std::uint64_t eventDurationNs = (std::max)(event.endTimeNs > event.startTimeNs ? event.endTimeNs - event.startTimeNs : 1, static_cast<std::uint64_t>(1));
	if (event.startTimeNs >= viewStartNs && event.endTimeNs <= viewEndNs && eventDurationNs <= currentViewDurationNs)
	{
		return;
	}

	const std::uint64_t captureDurationNs = captureEndNs - captureStartNs;
	std::uint64_t newViewDurationNs = currentViewDurationNs;
	if (newViewDurationNs < eventDurationNs)
	{
		newViewDurationNs = eventDurationNs;
	}

	newViewDurationNs = (std::min)(newViewDurationNs, captureDurationNs);

	std::uint64_t newViewStartNs = viewStartNs;
	if (event.startTimeNs < newViewStartNs)
	{
		newViewStartNs = event.startTimeNs;
	}

	if (event.endTimeNs > newViewStartNs + newViewDurationNs)
	{
		newViewStartNs = event.endTimeNs > newViewDurationNs ? event.endTimeNs - newViewDurationNs : captureStartNs;
	}

	if (newViewStartNs < captureStartNs)
	{
		newViewStartNs = captureStartNs;
	}

	if (captureEndNs > newViewDurationNs && newViewStartNs > captureEndNs - newViewDurationNs)
	{
		newViewStartNs = captureEndNs - newViewDurationNs;
	}

	if (newViewDurationNs >= captureDurationNs)
	{
		ResetZoom();
		return;
	}

	zoomStartNs_ = newViewStartNs;
	zoomEndNs_ = zoomStartNs_ + newViewDurationNs;
	hasZoom_ = true;
}

void ProfilerPanel::ZoomToEvent(const Goknar::Debug::ProfileEvent& event)
{
	std::uint64_t captureStartNs = 0;
	std::uint64_t captureEndNs = 0;
	if (!GetCapturedTimeRange(captureStartNs, captureEndNs))
	{
		return;
	}

	std::uint64_t eventStartNs = (std::max)(event.startTimeNs, captureStartNs);
	std::uint64_t eventEndNs = (std::min)(event.endTimeNs, captureEndNs);
	if (eventEndNs <= eventStartNs)
	{
		eventEndNs = eventStartNs + 1;
	}

	if (eventEndNs > captureEndNs)
	{
		eventEndNs = captureEndNs;
		eventStartNs = eventEndNs > 1 ? eventEndNs - 1 : captureStartNs;
	}

	zoomStartNs_ = eventStartNs;
	zoomEndNs_ = eventEndNs;
	hasZoom_ = true;
}

void ProfilerPanel::OpenEventSourceFile(const Goknar::Debug::ProfileEvent& event)
{
	const std::string sourceFilePath = ResolveProfilerSourcePath(event.filePath);
	if (sourceFilePath.empty())
	{
		exportStatus_ = "Source location unavailable for selected event";
		return;
	}

	const int lineNumber = event.lineNumber > 0 ? static_cast<int>(event.lineNumber) : 0;
	if (EditorSourceCodeUtils::OpenSourceFile(sourceFilePath, lineNumber))
	{
		exportStatus_ = "Opened source: " + sourceFilePath;
	}
	else
	{
		exportStatus_ = "Failed to open source: " + sourceFilePath;
	}
}

void ProfilerPanel::PanTimeline(float deltaPixels, float timelineWidth, std::uint64_t captureStartNs, std::uint64_t captureEndNs)
{
	if (timelineWidth <= 0.0f || captureEndNs <= captureStartNs)
	{
		return;
	}

	if (!hasZoom_)
	{
		zoomStartNs_ = captureStartNs;
		zoomEndNs_ = captureEndNs;
		hasZoom_ = true;
	}

	const std::uint64_t viewDurationNs = zoomEndNs_ > zoomStartNs_ ? zoomEndNs_ - zoomStartNs_ : 1;
	const std::uint64_t captureDurationNs = captureEndNs - captureStartNs;
	if (captureDurationNs <= viewDurationNs)
	{
		zoomStartNs_ = captureStartNs;
		zoomEndNs_ = captureEndNs;
		return;
	}

	const double nanosecondsPerPixel = static_cast<double>(viewDurationNs) / static_cast<double>(timelineWidth);
	const double deltaNsDouble = -static_cast<double>(deltaPixels) * nanosecondsPerPixel;
	const std::int64_t deltaNs = static_cast<std::int64_t>(deltaNsDouble);
	const std::int64_t minStartNs = static_cast<std::int64_t>(captureStartNs);
	const std::int64_t maxStartNs = static_cast<std::int64_t>(captureEndNs - viewDurationNs);
	std::int64_t newStartNs = static_cast<std::int64_t>(zoomStartNs_) + deltaNs;
	newStartNs = (std::max)(minStartNs, (std::min)(maxStartNs, newStartNs));

	zoomStartNs_ = static_cast<std::uint64_t>(newStartNs);
	zoomEndNs_ = zoomStartNs_ + viewDurationNs;
}

void ProfilerPanel::ZoomTimeline(float scale, double anchorNormalized, std::uint64_t captureStartNs, std::uint64_t captureEndNs)
{
	if (scale <= 0.0f || captureEndNs <= captureStartNs)
	{
		return;
	}

	const std::uint64_t captureDurationNs = captureEndNs - captureStartNs;
	std::uint64_t viewStartNs = hasZoom_ ? zoomStartNs_ : captureStartNs;
	std::uint64_t viewEndNs = hasZoom_ ? zoomEndNs_ : captureEndNs;
	if (viewEndNs <= viewStartNs)
	{
		viewEndNs = viewStartNs + 1;
	}

	const double currentViewDurationNs = static_cast<double>(viewEndNs - viewStartNs);
	const double minViewDurationNs = 1000.0;
	double newViewDurationNs = currentViewDurationNs * static_cast<double>(scale);
	newViewDurationNs = (std::max)(minViewDurationNs, (std::min)(static_cast<double>(captureDurationNs), newViewDurationNs));

	if (newViewDurationNs >= static_cast<double>(captureDurationNs))
	{
		ResetZoom();
		return;
	}

	anchorNormalized = Clamp01(anchorNormalized);
	const double anchorTimeNs = static_cast<double>(viewStartNs) + currentViewDurationNs * anchorNormalized;
	double newViewStartNs = anchorTimeNs - newViewDurationNs * anchorNormalized;
	const double minStartNs = static_cast<double>(captureStartNs);
	const double maxStartNs = static_cast<double>(captureEndNs) - newViewDurationNs;
	newViewStartNs = (std::max)(minStartNs, (std::min)(maxStartNs, newViewStartNs));

	zoomStartNs_ = static_cast<std::uint64_t>(newViewStartNs);
	zoomEndNs_ = zoomStartNs_ + static_cast<std::uint64_t>(newViewDurationNs);
	if (zoomEndNs_ <= zoomStartNs_)
	{
		zoomEndNs_ = zoomStartNs_ + 1;
	}
	hasZoom_ = true;
}

void ProfilerPanel::ResetZoom()
{
	hasZoom_ = false;
	zoomStartNs_ = 0;
	zoomEndNs_ = 0;
	isPanningTimeline_ = false;
}

Goknar::Debug::ProfileSnapshot ProfilerPanel::BuildSnapshotFromStoredEvents() const
{
	Goknar::Debug::ProfileSnapshot snapshot;
	snapshot.events = events_;
	snapshot.threads = threads_;

	for (const Goknar::Debug::ProfileEvent& event : snapshot.events)
	{
		if (snapshot.firstTimestampNs == 0 || event.startTimeNs < snapshot.firstTimestampNs)
		{
			snapshot.firstTimestampNs = event.startTimeNs;
		}

		snapshot.lastTimestampNs = (std::max)(snapshot.lastTimestampNs, event.endTimeNs);
		snapshot.frameIndex = (std::max)(snapshot.frameIndex, event.frameIndex);
	}

	return snapshot;
}

const Goknar::Debug::ProfileThreadInfo* ProfilerPanel::FindThreadInfo(std::uint32_t threadIndex) const
{
	auto threadIt = std::find_if(
		threads_.begin(),
		threads_.end(),
		[threadIndex](const Goknar::Debug::ProfileThreadInfo& threadInfo)
		{
			return threadInfo.index == threadIndex;
		});

	return threadIt != threads_.end() ? &(*threadIt) : nullptr;
}

bool ProfilerPanel::GetCapturedTimeRange(std::uint64_t& outStartNs, std::uint64_t& outEndNs) const
{
	if (events_.empty())
	{
		return false;
	}

	outStartNs = std::numeric_limits<std::uint64_t>::max();
	outEndNs = 0;
	for (const Goknar::Debug::ProfileEvent& event : events_)
	{
		outStartNs = (std::min)(outStartNs, event.startTimeNs);
		outEndNs = (std::max)(outEndNs, event.endTimeNs);
	}

	if (outStartNs == std::numeric_limits<std::uint64_t>::max())
	{
		outStartNs = 0;
	}

	if (outEndNs <= outStartNs)
	{
		outEndNs = outStartNs + 1;
	}

	return true;
}

std::string ProfilerPanel::GetThreadName(std::uint32_t threadIndex) const
{
	const Goknar::Debug::ProfileThreadInfo* threadInfo = FindThreadInfo(threadIndex);
	if (threadInfo)
	{
		return threadInfo->name;
	}

	return "Thread " + std::to_string(threadIndex);
}

#endif
