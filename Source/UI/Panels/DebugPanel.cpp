#include "DebugPanel.h"

#include "imgui.h"

#include "Goknar/Engine.h"
#include "Goknar/Renderer/Renderer.h"

#include "Objects/FreeCameraObject.h"
#include "UI/EditorContext.h"

DebugPanel::DebugPanel(EditorHUD* hud) : 
	IEditorPanel("Debug", hud)
{
	engine->GetRenderer()->countDrawCalls = true;
}

void DebugPanel::Draw()
{
	ImGui::Begin("DrawInfo", &isOpen_, 
		ImGuiWindowFlags_NoBackground | 
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoResize);

	ImGui::Text((std::string("Draw call count: ") + std::to_string(engine->GetRenderer()->drawCallCount)).c_str());

	ImGui::Text((std::string("Position: ") + EditorContext::Get()->viewportCamera->GetWorldPosition().ToString()).c_str());
	ImGui::Text((std::string("Rotation: ") + EditorContext::Get()->viewportCamera->GetWorldRotation().ToEulerDegrees().ToString()).c_str());
	ImGui::Text((std::string("Forward Vector: ") + EditorContext::Get()->viewportCamera->GetForwardVector().ToString()).c_str());
	ImGui::Text((std::string("Left Vector: ") + EditorContext::Get()->viewportCamera->GetLeftVector().ToString()).c_str());
	ImGui::Text((std::string("Up Vector: ") + EditorContext::Get()->viewportCamera->GetUpVector().ToString()).c_str());

	ImGui::End();
}