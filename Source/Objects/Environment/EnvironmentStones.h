#ifndef __ENVIRONMENTSTONES_H__
#define __ENVIRONMENTSTONES_H__

#include "Goknar/Core.h"
#include "Goknar/Physics/RigidBody.h"

class StaticMeshComponent;
class MovingTriangleMeshCollisionComponent;

class GOKNAR_API EnvironmentStone : public RigidBody
{
public:
	EnvironmentStone();

protected:
	StaticMeshComponent* staticMeshComponent_;
	MovingTriangleMeshCollisionComponent* collisionComponent_;

private:
};

class GOKNAR_API EnvironmentStone_01 : public EnvironmentStone
{
public:
	EnvironmentStone_01();
};

class GOKNAR_API EnvironmentStone_02 : public EnvironmentStone
{
public:
	EnvironmentStone_02();
};

class GOKNAR_API EnvironmentStone_03 : public EnvironmentStone
{
public:
	EnvironmentStone_03();
};

class GOKNAR_API EnvironmentStone_04 : public EnvironmentStone
{
public:
	EnvironmentStone_04();
};

class GOKNAR_API EnvironmentLStone_01 : public EnvironmentStone
{
public:
	EnvironmentLStone_01();
};

class GOKNAR_API EnvironmentLStone_02 : public EnvironmentStone
{
public:
	EnvironmentLStone_02();
};

class GOKNAR_API EnvironmentLStone_03 : public EnvironmentStone
{
public:
	EnvironmentLStone_03();
};

class GOKNAR_API EnvironmentLStone_04 : public EnvironmentStone
{
public:
	EnvironmentLStone_04();
};

class GOKNAR_API EnvironmentLStone_05 : public EnvironmentStone
{
public:
	EnvironmentLStone_05();
};

class GOKNAR_API EnvironmentLStone_06 : public EnvironmentStone
{
public:
	EnvironmentLStone_06();
};

#endif