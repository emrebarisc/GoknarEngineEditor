#pragma once

#include "Goknar/Core.h"
#include "Goknar/Physics/RigidBody.h"

class MultipleCollisionComponent;
class StaticMeshComponent;
class PointLightComponent;
class RigidBody;

class GOKNAR_API Portal : public RigidBody
{
public:
	Portal();
	~Portal();

	virtual void BeginGame() override;
	virtual void Tick(float deltaTime) override;

protected:
	virtual void OnOverlapBegin(PhysicsObject* otherObject, class CollisionComponent* otherComponent, const Vector3& hitPosition, const Vector3& hitNormal);

	StaticMeshComponent* outerStaticMeshComponent_{ nullptr };
	StaticMeshComponent* portalStaticMeshComponent_{ nullptr };
	MultipleCollisionComponent* multipleCollisionComponent_{ nullptr };
	PointLightComponent* lightComponent_{ nullptr };
private:

};