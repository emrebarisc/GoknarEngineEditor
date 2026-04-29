#pragma once

#include <array>
#include <chrono>

#include "imgui.h"

class EditorDoubleClickController
{
public:
	void OnCursorMove(const ImVec2& cursorPosition);
	void OnMouseButtonPressed(ImGuiMouseButton button, const ImVec2& cursorPosition);
	void ClearFrameState();

	bool WasMouseDoubleClicked(ImGuiMouseButton button) const;

private:
	struct ButtonState
	{
		bool hasPendingClick{ false };
		bool wasDoubleClickedThisFrame{ false };
		double lastClickTimeSeconds{ 0.0 };
		ImVec2 lastClickPosition{ 0.0f, 0.0f };
	};

	static constexpr double doubleClickIntervalSeconds_{ 0.30 };
	static constexpr float doubleClickDistancePixels_{ 6.0f };

	double GetCurrentTimeSeconds() const;
	bool IsValidButton(ImGuiMouseButton button) const;

	ImVec2 cursorPosition_{ 0.0f, 0.0f };
	std::array<ButtonState, ImGuiMouseButton_COUNT> buttonStates_{};
};
