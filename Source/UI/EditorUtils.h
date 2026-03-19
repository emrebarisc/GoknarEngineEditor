#pragma once

#include "imgui.h"

#include <string>

#include "Goknar/Camera.h"
#include "Goknar/Engine.h"
#include "Goknar/Math/GoknarMath.h"
#include "Goknar/Managers/ResourceManager.h"


#include "EditorCore.h"
#include "Goknar/Contents/Content.h"

namespace
{
	template<typename Tag>
	struct StolenMember {
		static typename Tag::type ptr;
	};
	template<typename Tag>
	typename Tag::type StolenMember<Tag>::ptr;

	template<typename Tag, typename Tag::type p>
	struct StealMember {
		struct Filler {
			Filler() { StolenMember<Tag>::ptr = p; }
		};
		static Filler filler_obj;
	};
	template<typename Tag, typename Tag::type p>
	typename StealMember<Tag, p>::Filler StealMember<Tag, p>::filler_obj;

	struct ResourceManager_LoadContent {
		using type = Content* (ResourceManager::*)(const std::string&);
	};
	template class StealMember<ResourceManager_LoadContent, &ResourceManager::LoadContent>;
}

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

	static void DrawWorldAxis(Camera* camera, float padding = 40.0f, float axisLength = 35.0f);

	template<typename T>
	static T* GetEditorContent(const std::string& path);
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

static void EditorUtils::DrawWorldAxis(Camera* camera, float padding/* = 40.0f*/, float axisLength/* = 35.0f*/)
{
	if (!camera)
	{
		return;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 winPos = ImGui::GetWindowPos();
	ImVec2 winSize = ImGui::GetWindowSize();

	// Anchor to the bottom-left of the current ImGui window
	ImVec2 origin = ImVec2(winPos.x + padding, winPos.y + winSize.y - padding);

	Vector3 left = camera->GetLeftVector();
	Vector3 up = camera->GetUpVector();

	ImVec2 endX = ImVec2(origin.x + (-left.x * axisLength), origin.y + (-up.x * axisLength));
	ImVec2 endY = ImVec2(origin.x + (-left.y * axisLength), origin.y + (-up.y * axisLength));
	ImVec2 endZ = ImVec2(origin.x + (-left.z * axisLength), origin.y + (-up.z * axisLength));

	drawList->AddLine(origin, endX, IM_COL32(255, 50, 50, 255), 2.5f);
	drawList->AddLine(origin, endY, IM_COL32(50, 255, 50, 255), 2.5f);
	drawList->AddLine(origin, endZ, IM_COL32(50, 150, 255, 255), 2.5f);

	drawList->AddText(endX, IM_COL32(255, 100, 100, 255), "X");
	drawList->AddText(endY, IM_COL32(100, 255, 100, 255), "Y");
	drawList->AddText(endZ, IM_COL32(100, 150, 255, 255), "Z");
}

template<typename T>
static T* EditorUtils::GetEditorContent(const std::string& path)
{
	std::string fullpath = EditorContentDir + path;

	T* result = engine->GetResourceManager()->GetResourceContainer()->GetContent<T>(fullpath);

	if (!result)
	{
		auto loadContentPtr = StolenMember<ResourceManager_LoadContent>::ptr;
		result = dynamic_cast<T*>((engine->GetResourceManager()->*loadContentPtr)(fullpath));
	}

	return result;
}