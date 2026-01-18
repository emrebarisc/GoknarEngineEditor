#pragma once

#include "EditorPanel.h"
#include "UI/EditorContext.h"

#include <functional>
#include <unordered_map>
#include <string>

class ObjectBase;
class PhysicsObject;

class Component;
class StaticMeshComponent;
class BoxCollisionComponent;
class SphereCollisionComponent;
class CapsuleCollisionComponent;
class MovingTriangleMeshCollisionComponent;
class NonMovingTriangleMeshCollisionComponent;

class DetailsPanel : public IEditorPanel
{
public:
	DetailsPanel(EditorHUD* hud);
	virtual void Draw() override;

private:
	void DrawTransform();
	void DrawObjectDetails();
	void DrawPhysicsDetails();
	void DrawDirectionalLightDetails();
	void DrawPointLightDetails();
	void DrawSpotLightDetails();

	void DrawComponentDetails(ObjectBase* owner, Component* component);
	void DrawStaticMeshComponentDetails(StaticMeshComponent* component);
	void DrawBoxCollisionComponentDetails(BoxCollisionComponent* boxCollisionComponent);
	void DrawSphereCollisionComponentDetails(SphereCollisionComponent* sphereCollisionComponent);
	void DrawCapsuleCollisionComponentDetails(CapsuleCollisionComponent* capsuleCollisionComponent);
	void DrawMovingTriangleMeshCollisionComponentDetails(MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent);
	void DrawNonMovingTriangleMeshCollisionComponentDetails(NonMovingTriangleMeshCollisionComponent* nonMovingTriangleMeshCollisionComponent);

	void DrawAddComponentOptions(ObjectBase* object);

	void SetupReflections();

	void OnAssetSelected(const std::string& path);

	void* assetSelectionComponent_;
	EditorComponentType assetSelectionComponentType_;

	std::unordered_map<std::string, std::function<void(ObjectBase*)>> objectReflections_;
	std::unordered_map<std::string, std::function<void(PhysicsObject*)>> physicsReflections_;
};