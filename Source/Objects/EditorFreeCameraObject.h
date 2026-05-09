#pragma once

#include "Goknar/Core.h"
#include "Goknar/ObjectBase.h"

class CameraComponent;
class EditorFreeCameraController;

class GOKNAR_API EditorFreeCameraObject : public ObjectBase
{
public:
	EditorFreeCameraObject();

	EditorFreeCameraController* GetController() const 
	{
		return freeCameraController_;
	}

	CameraComponent* GetCameraComponent() const
	{
		return cameraComponent_;
	}

protected:
	virtual void BeginGame() override;
	virtual void Tick(float deltaTime) override;

	EditorFreeCameraController* freeCameraController_{ nullptr };
	CameraComponent* cameraComponent_{ nullptr };
private:
};
