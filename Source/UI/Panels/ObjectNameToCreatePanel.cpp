#include "ObjectNameToCreatePanel.h"

#include "imgui.h"

#include "Goknar/Engine.h"
#include "Goknar/Managers/InputManager.h"
#include "Goknar/Managers/WindowManager.h"

#include "UI/EditorContext.h"

void ObjectNameToCreatePanel::Draw()
{	
	double x, y;
	engine->GetInputManager()->GetCursorPosition(engine->GetWindowManager()->GetMainWindow(), x, y);
	ImGui::SetNextWindowPos({ (float)x, (float)y });

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBackground;
	windowFlags |= ImGuiWindowFlags_NoTitleBar;
	windowFlags |= ImGuiWindowFlags_AlwaysAutoResize;
	windowFlags |= ImGuiWindowFlags_NoResize;

	ImGui::Begin(title_.c_str(), &isOpen_, windowFlags);

	ImGui::Text(EditorContext::Get()->objectToCreateName.c_str());
	
	ImGui::End();
}