#pragma once

#include "EditorPanel.h"
#include "Goknar/Math/GoknarMath.h"

class RenderTarget;
class MeshViewerCameraObject;
class ObjectBase;
class StaticMeshComponent;
class StaticMesh;

class MeshViewerPanel : public IEditorPanel
{
public:
	MeshViewerPanel(EditorHUD* hud);
	virtual ~MeshViewerPanel();

	virtual void Init() override;
	virtual void Draw() override;

	void SetTargetStaticMesh(StaticMesh* staticMesh);

private:
	RenderTarget* renderTarget_{ nullptr };
	MeshViewerCameraObject* cameraObject_{ nullptr };
	ObjectBase* viewedObject_{ nullptr };
	
	StaticMeshComponent* staticMeshComponent_{ nullptr };

	Vector2 size_;
};