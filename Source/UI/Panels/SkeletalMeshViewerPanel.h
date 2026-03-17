#pragma once

#include "EditorPanel.h"
#include "Goknar/Math/GoknarMath.h"

class RenderTarget;
class MeshViewerCameraObject;
class ObjectBase;
class SkeletalMeshComponent;
class SkeletalMesh;

class SkeletalMeshViewerPanel : public IEditorPanel
{
public:
	SkeletalMeshViewerPanel(EditorHUD* hud);
	virtual ~SkeletalMeshViewerPanel();

	virtual void Init() override;
	virtual void Draw() override;

	void SetTargetSkeletalMesh(SkeletalMesh* skeletalMesh);

private:
	RenderTarget* renderTarget_{ nullptr };
	MeshViewerCameraObject* cameraObject_{ nullptr };
	ObjectBase* viewedObject_{ nullptr };

	SkeletalMeshComponent* skeletalMeshComponent_{ nullptr };

	Vector2 size_;
};