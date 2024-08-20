#ifndef __ENVIRONMENTPLANTS_H__
#define __ENVIRONMENTPLANTS_H__

#include "Goknar/Core.h"
#include "Goknar/ObjectBase.h"

class StaticMeshComponent;

class GOKNAR_API EnvironmentPlant : public ObjectBase
{
public:
	EnvironmentPlant();

protected:
	StaticMeshComponent* staticMeshComponent_;

	void BeginGame() override;

private:
};

class GOKNAR_API EnvironmentGrass_01 : public EnvironmentPlant
{
public:
	EnvironmentGrass_01();
};

class GOKNAR_API EnvironmentMushrooms_01 : public EnvironmentPlant
{
public:
	EnvironmentMushrooms_01();
};

#endif