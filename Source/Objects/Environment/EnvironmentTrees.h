#ifndef __ENVIRONMENTTREES_H__
#define __ENVIRONMENTTREES_H__

#include "Goknar/Core.h"
#include "Goknar/Physics/RigidBody.h"

class StaticMeshComponent;
class CapsuleCollisionComponent;

class GOKNAR_API EnvironmentTree : public RigidBody
{
public:
	EnvironmentTree();

protected:
	StaticMeshComponent* staticMeshComponent_;
	CapsuleCollisionComponent* collisionComponent_;

	void BeginGame() override;

private:
};

class GOKNAR_API EnvironmentTree_01 : public EnvironmentTree
{
public:
	EnvironmentTree_01();
};

#endif