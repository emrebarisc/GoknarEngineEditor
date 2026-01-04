#include "EditorContext.h"

#include "imgui.h"

#include "Objects/FreeCameraObject.h"
#include "Controllers/FreeCameraController.h"

#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Renderer/RenderTarget.h"

EditorContext::EditorContext()
{
	imguiContext_ = ImGui::CreateContext();

	viewportCameraObject = new FreeCameraObject();
	viewportCameraObject->SetName("__Editor__ViewportCamera");
	viewportCameraObject->GetFreeCameraController()->SetName("__Editor__FreeCameraController");

	Camera* viewportCamera = viewportCameraObject->GetCameraComponent()->GetCamera();
	viewportCamera->SetCameraType(CameraType::RenderTarget);

	viewportRenderTarget = new RenderTarget();
	viewportRenderTarget->SetCamera(viewportCamera);
	viewportRenderTarget->Init();
}

EditorContext::~EditorContext()
{
	delete viewportRenderTarget;

	viewportCameraObject->Destroy();

	ImGui::DestroyContext(imguiContext_);
}

void EditorContext::SetCameraMovement(bool value)
{
	viewportCameraObject->GetFreeCameraController()->SetIsActive(value);
}
