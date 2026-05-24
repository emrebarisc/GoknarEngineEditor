#include "AIDebugPanel.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <variant>

#include "imgui.h"

#include "UI/EditorWidgets.h"

AIDebugPanel::AIDebugPanel(EditorHUD* hud) :
	IEditorPanel("AI Debug", hud)
{
	isOpen_ = false;
}

void AIDebugPanel::Draw()
{
	if (!isOpen_)
	{
		return;
	}
	ImGui::Begin(title_.c_str(), &isOpen_);

	ImGui::End();
}