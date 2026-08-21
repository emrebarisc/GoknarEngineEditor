#include "MeshViewerCameraObject.h"

#include "Goknar/Camera.h"
#include "Goknar/Components/CameraComponent.h"

#include "Controllers/MeshViewerCameraController.h"

MeshViewerCameraObject::MeshViewerCameraObject() : ObjectBase()
{
	cameraComponent_ = AddSubComponent<CameraComponent>();
	cameraComponent_->SetCameraFollowsComponentRotation(true);
	cameraComponent_->GetCamera()->SetNearDistance(0.0001f);
	SetRootComponent(cameraComponent_);

	controller_ = new MeshViewerCameraController(this);
}

MeshViewerCameraObject::~MeshViewerCameraObject()
{
	controller_->Destroy();
	controller_ = nullptr;
}

void MeshViewerCameraObject::BeginGame()
{
	ObjectBase::BeginGame();
	controller_->ResetView();
}
