#include "EditorDoubleClickController.h"

void EditorDoubleClickController::OnCursorMove(const ImVec2& cursorPosition)
{
	cursorPosition_ = cursorPosition;
}

void EditorDoubleClickController::OnMouseButtonPressed(ImGuiMouseButton button, const ImVec2& cursorPosition)
{
	if (!IsValidButton(button))
	{
		return;
	}

	cursorPosition_ = cursorPosition;

	ButtonState& buttonState = buttonStates_[button];
	const double currentTimeSeconds = GetCurrentTimeSeconds();
	const double timeSinceLastClickSeconds = currentTimeSeconds - buttonState.lastClickTimeSeconds;

	const float deltaX = cursorPosition_.x - buttonState.lastClickPosition.x;
	const float deltaY = cursorPosition_.y - buttonState.lastClickPosition.y;
	const float distanceSquared = deltaX * deltaX + deltaY * deltaY;
	const float doubleClickDistanceSquared = doubleClickDistancePixels_ * doubleClickDistancePixels_;

	const bool isDoubleClick = buttonState.hasPendingClick &&
		timeSinceLastClickSeconds <= doubleClickIntervalSeconds_ &&
		distanceSquared <= doubleClickDistanceSquared;

	buttonState.wasDoubleClickedThisFrame = isDoubleClick;

	if (isDoubleClick)
	{
		buttonState.hasPendingClick = false;
		return;
	}

	buttonState.hasPendingClick = true;
	buttonState.lastClickTimeSeconds = currentTimeSeconds;
	buttonState.lastClickPosition = cursorPosition_;
}

void EditorDoubleClickController::ClearFrameState()
{
	for (ButtonState& buttonState : buttonStates_)
	{
		buttonState.wasDoubleClickedThisFrame = false;
	}
}

bool EditorDoubleClickController::WasMouseDoubleClicked(ImGuiMouseButton button) const
{
	return IsValidButton(button) && buttonStates_[button].wasDoubleClickedThisFrame;
}

double EditorDoubleClickController::GetCurrentTimeSeconds() const
{
	using Clock = std::chrono::steady_clock;

	return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

bool EditorDoubleClickController::IsValidButton(ImGuiMouseButton button) const
{
	return 0 <= button && button < ImGuiMouseButton_COUNT;
}
