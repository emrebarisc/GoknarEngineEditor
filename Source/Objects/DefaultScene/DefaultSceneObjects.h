#pragma once

#include "Goknar/Core.h"
#include "Goknar/Physics/RigidBody.h"

class StaticMeshComponent;
class NonMovingTriangleMeshCollisionComponent;

class GOKNAR_API DefaultSceneArch : public RigidBody
{
public:
	DefaultSceneArch();

protected:
	StaticMeshComponent* staticMeshComponent_;
	NonMovingTriangleMeshCollisionComponent* collisionComponent_;
private:
};

class GOKNAR_API DefaultSceneCube : public RigidBody
{
public:
	DefaultSceneCube();

protected:
	StaticMeshComponent* staticMeshComponent_;
	NonMovingTriangleMeshCollisionComponent* collisionComponent_;
private:
};

class GOKNAR_API DefaultSceneRadialStaircase : public RigidBody
{
public:
	DefaultSceneRadialStaircase();

protected:
	StaticMeshComponent* staticMeshComponent_;
	NonMovingTriangleMeshCollisionComponent* collisionComponent_;
private:
};

class GOKNAR_API DefaultSceneRadialStaircaseMirrored : public RigidBody
{
public:
	DefaultSceneRadialStaircaseMirrored();

protected:
	StaticMeshComponent* staticMeshComponent_;
	NonMovingTriangleMeshCollisionComponent* collisionComponent_;
private:
};

class GOKNAR_API DefaultSceneRoundCorner : public RigidBody
{
public:
	DefaultSceneRoundCorner();

protected:
	StaticMeshComponent* staticMeshComponent_;
	NonMovingTriangleMeshCollisionComponent* collisionComponent_;
private:
};