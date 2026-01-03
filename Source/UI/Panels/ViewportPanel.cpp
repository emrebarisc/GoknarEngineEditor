#include "ViewportPanel.h"

#include "Goknar/Camera.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Renderer/RenderTarget.h"
#include "Goknar/Renderer/Texture.h"

#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/EditorUtils.h"

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
	cameraObject_ = new FreeCameraObject();
	cameraObject_->SetWorldPosition(Vector3(0.f, 0.f, 10.f));
	EditorContext::Get()->viewportCamera = cameraObject_;

	Camera* renderTargetCamera = EditorContext::Get()->viewportCamera->GetCameraComponent()->GetCamera();
	renderTargetCamera->SetCameraType(CameraType::RenderTarget);
	EditorContext::Get()->viewportRenderTarget->SetCamera(renderTargetCamera);
}

void ViewportPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImVec2 newViewportSize = ImGui::GetWindowSize();

	if (size_.x != newViewportSize.x || size_.y != newViewportSize.y)
	{
		size_ = EditorUtils::ToVector2(newViewportSize);
		EditorContext::Get()->viewportRenderTarget->SetFrameSize({ newViewportSize.x, newViewportSize.y });
	}

	position_ = EditorUtils::ToVector2(ImGui::GetWindowPos());

	Texture* renderTargetTexture = EditorContext::Get()->viewportRenderTarget->GetTexture();

	ImGui::SetCursorPos(ImVec2{ 0.f, 0.f });
	ImGui::Image(
		(ImTextureID)(intptr_t)renderTargetTexture->GetRendererTextureId(),
		EditorUtils::ToImVec2(size_),
		ImVec2{ 0.f, 1.f },
		ImVec2{ 1.f, 0.f }
	);

	EditorContext::Get()->SetCameraMovement(IsCursorOn());

	ImGui::End();
}