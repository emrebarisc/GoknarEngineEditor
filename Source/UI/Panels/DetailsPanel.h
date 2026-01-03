#pragma once

#include "EditorPanel.h"

#include <unordered_map>
#include <functional>

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
	void DrawTransform(ObjectBase* obj);
	void DrawObjectDetails(ObjectBase* obj);
	void DrawPhysicsDetails(PhysicsObject* obj);
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

	std::unordered_map<std::string, std::function<void(ObjectBase*)>> objectReflections_;
	std::unordered_map<std::string, std::function<void(PhysicsObject*)>> physicsReflections_;
};