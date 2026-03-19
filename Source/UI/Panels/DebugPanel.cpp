#include "DebugPanel.h"

#include "imgui.h"

#include "Goknar/Engine.h"
#include "Goknar/Renderer/Renderer.h"

#include "Objects/FreeCameraObject.h"
#include "UI/EditorContext.h"

DebugPanel::DebugPanel(EditorHUD* hud) : 
	IEditorPanel("Debug", hud)
{
	isOpen_ = true;

	engine->GetRenderer()->countDrawCalls = true;
}

void DebugPanel::Draw()
{
}

void DebugPanel::DrawOverlay(const Vector2& viewportPos, const Vector2& viewportSize)
{
    const float padding = 10.0f;
    const float titleBarOffset = 25.0f;

    ImVec2 overlayPos = ImVec2(
        viewportPos.x + viewportSize.x - padding,
        viewportPos.y + padding + titleBarOffset
    );

    ImGui::SetNextWindowPos(overlayPos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);

    ImGuiWindowFlags overlayFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;


    if (!ImGui::Begin("DebugOverlay", nullptr, overlayFlags))
    {
        ImGui::End();
        return;
    }

    ImVec2 currentPos = ImGui::GetWindowPos();
    ImVec2 currentSize = ImGui::GetWindowSize();

    position_ = Vector2(currentPos.x, currentPos.y);
    size_ = Vector2(currentSize.x, currentSize.y);

    ImGui::Text("Draw call count: %d", engine->GetRenderer()->drawCallCount);
    ImGui::Separator();

    auto* cameraObj = EditorContext::Get()->viewportCameraObject;
    ImGui::Text("Position: %s", cameraObj->GetWorldPosition().ToString().c_str());
    ImGui::Text("Rotation: %s", cameraObj->GetWorldRotation().ToEulerDegrees().ToString().c_str());
    ImGui::Text("Forward Vector: %s", cameraObj->GetForwardVector().ToString().c_str());
    ImGui::Text("Left Vector: %s", cameraObj->GetLeftVector().ToString().c_str());
    ImGui::Text("Up Vector: %s", cameraObj->GetUpVector().ToString().c_str());

    ImGui::End();
}