#include "EditorContext.h"

#include "imgui.h"

#include "Objects/FreeCameraObject.h"
#include "Controllers/FreeCameraController.h"

#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Renderer/RenderTarget.h"

EditorContext::EditorContext()
{
	imguiContext_ = ImGui::CreateContext();

	viewportCamera = new FreeCameraObject();
	viewportCamera->SetName("__Editor__ViewportCamera");
	viewportCamera->GetFreeCameraController()->SetName("__Editor__FreeCameraController");

	viewportRenderTarget = new RenderTarget();
	viewportRenderTarget->SetCamera(viewportCamera->GetCameraComponent()->GetCamera());
	viewportRenderTarget->Init();
}

EditorContext::~EditorContext()
{
	delete viewportRenderTarget;

	viewportCamera->Destroy();

	ImGui::DestroyContext(imguiContext_);
}

void EditorContext::SetCameraMovement(bool value)
{
	viewportCamera->GetFreeCameraController()->SetIsActive(value);
}
