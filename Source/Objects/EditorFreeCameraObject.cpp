#include "EditorFreeCameraObject.h"

#include "Goknar/Engine.h"
#include "Goknar/Camera.h"
#include "Goknar/Managers/WindowManager.h"

#include "Components/CameraComponent.h"
#include "Controllers/EditorFreeCameraController.h"

EditorFreeCameraObject::EditorFreeCameraObject() : ObjectBase()
{
	cameraComponent_ = AddSubComponent<CameraComponent>();
	cameraComponent_->SetCameraFollowsComponentRotation(true);
	SetRootComponent(cameraComponent_);

	freeCameraController_ = new EditorFreeCameraController(this);
}

void EditorFreeCameraObject::BeginGame()
{
	WindowManager* windowManager = engine->GetWindowManager();
	Camera* camera = cameraComponent_->GetCamera();
	camera->SetImageWidth(windowManager->GetWindowSize().x);
	camera->SetImageHeight(windowManager->GetWindowSize().y);

	SetWorldPosition(Vector3{ -15.f, -16.f, 12.5f });
	SetWorldRotation(Quaternion::FromEulerDegrees({ 5.f, 20.f, 25.f }));
}

void EditorFreeCameraObject::Tick(float)
{

}
