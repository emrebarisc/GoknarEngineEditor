#pragma once

#include "imgui.h"

#include "Goknar/Math/GoknarMath.h"

#include "UI/EditorContext.h"

namespace EditorUtils
{
	static inline Vector2 ToVector2(const ImVec2& value)
	{
		return {value.x, value.y};
	}

	static inline ImVec2 ToImVec2(const Vector2& value)
	{
		return {value.x, value.y};
	}

	static inline ImVec2 ToImVec2(const Vector2i& value)
	{
		return {(float)value.x, (float)value.y};
	}

	static bool IsCursorInCurrentWindow()
	{
		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();

		ImVec2 mousePos = ImGui::GetMousePos();

		bool isMouseInside = mousePos.x >= windowPos.x && mousePos.x <= windowPos.x + windowSize.x &&
			mousePos.y >= windowPos.y && mousePos.y <= windowPos.y + windowSize.y;

		return isMouseInside;
	}
}

static inline ImVec2 operator*(const ImVec2& lhs, float rhs)
{
	return ImVec2(lhs.x * rhs, lhs.y * rhs);
}

static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
{
	return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}

static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs)
{
	return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}