#include "ViewportPanel.h"

#include "Goknar/Camera.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Renderer/RenderTarget.h"
#include "Goknar/Renderer/Texture.h"

#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/EditorUtils.h"

#include "Controllers/FreeCameraController.h"
#include "Objects/FreeCameraObject.h"


ViewportPanel::ViewportPanel(EditorHUD* hud) : 
	IEditorPanel("Viewport", hud)
{
	size_ = Vector2(1024, 768);
}

ViewportPanel::~ViewportPanel()
{
}

void ViewportPanel::Init()
{
	Camera* renderTargetCamera = EditorContext::Get()->viewportCameraObject->GetCameraComponent()->GetCamera();
	EditorContext::Get()->viewportRenderTarget->SetCamera(renderTargetCamera);

    debugPanel_ = static_cast<DebugPanel*>(hud_->GetPanel<DebugPanel>());
}

void ViewportPanel::Draw()
{
	if (!ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	bool isHovered = ImGui::IsItemHovered() || ImGui::IsWindowHovered();
	isHovered |= debugPanel_->IsItemHovered();

	EditorContext::Get()->viewportCameraObject->GetController()->SetIsActive(isHovered);

	ImVec2 newViewportSize = ImGui::GetContentRegionAvail();

	if (newViewportSize.x <= 0.0f || newViewportSize.y <= 0.0f)
	{
		ImGui::End();
		return;
	}

	if (size_.x != newViewportSize.x || size_.y != newViewportSize.y)
	{
		size_ = EditorUtils::ToVector2(newViewportSize);
		EditorContext::Get()->viewportRenderTarget->SetFrameSize({ newViewportSize.x, newViewportSize.y });
	}

	ImVec2 currentPos = ImGui::GetWindowPos();
	ImVec2 currentSize = ImGui::GetWindowSize();

	position_ = Vector2(currentPos.x, currentPos.y);
	size_ = Vector2(currentSize.x, currentSize.y);

	Texture* renderTargetTexture = EditorContext::Get()->viewportRenderTarget->GetTexture();

	ImGui::SetCursorPos(ImVec2{ 0.f, 0.f });
	ImGui::Image(
		(ImTextureID)(intptr_t)renderTargetTexture->GetRendererTextureId(),
		EditorUtils::ToImVec2(size_),
		ImVec2{ 0.f, 1.f },
		ImVec2{ 1.f, 0.f }
	);

	EditorUtils::DrawWorldAxis(EditorContext::Get()->viewportCameraObject->GetCameraComponent()->GetCamera());

    if (showDebugOverlay_ && debugPanel_)
    {
        debugPanel_->DrawOverlay(position_, size_);
    }

	ImGui::End();
}