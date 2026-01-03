#pragma once

#include <string>

#include "imgui.h"

#include "Goknar/Math/GoknarMath.h"

class EditorHUD;

class IEditorPanel
{
public:
	IEditorPanel() = delete;
	IEditorPanel(const std::string& title, EditorHUD* hud) : 
		title_(title), 
		hud_(hud), 
		isOpen_(true)
	{

	}
	virtual ~IEditorPanel() = default;

	virtual void Init() {}
	virtual void Draw() = 0;

	void SetIsOpen(bool isOpen)
	{
		isOpen_ = isOpen;
	}

	bool GetIsOpen() const
	{
		return isOpen_;
	}

	const std::string& GetTitle() const { return title_; }

protected:
	inline bool IsCursorOn();

	std::string title_{ "" };

	Vector2 position_{ Vector2::ZeroVector };
	Vector2 size_{ Vector2::ZeroVector };

	EditorHUD* hud_{ nullptr };

	bool isOpen_{ true };
};

bool IEditorPanel::IsCursorOn()
{
	ImVec2 windowPos = ImGui::GetWindowPos();
	ImVec2 windowSize = ImGui::GetWindowSize();

	ImVec2 mousePos = ImGui::GetMousePos();

	bool isMouseInside = mousePos.x >= windowPos.x && mousePos.x <= windowPos.x + windowSize.x &&
		mousePos.y >= windowPos.y && mousePos.y <= windowPos.y + windowSize.y;

	return isMouseInside;
}