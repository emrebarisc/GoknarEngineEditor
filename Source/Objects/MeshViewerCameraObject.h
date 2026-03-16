#pragma once

#include "Goknar/ObjectBase.h"

class CameraComponent;
class MeshViewerCameraController;

class MeshViewerCameraObject : public ObjectBase
{
public:
	MeshViewerCameraObject();

	MeshViewerCameraController* GetController() const { return controller_; }
	CameraComponent* GetCameraComponent() const { return cameraComponent_; }

protected:
	virtual void BeginGame() override;

private:
	CameraComponent* cameraComponent_{ nullptr };
	MeshViewerCameraController* controller_{ nullptr };
};