#pragma once

#include "Goknar/Core.h"
#include "Goknar/Delegates/MulticastDelegate.h"

class Game;

class GOKNAR_API MaterialInitializer
{
public:
	MaterialInitializer();

protected:
    
private:
	void Skills_InitializeTargetObjectMaterials();
	void Skills_InitializeFireBurstCastedObjectMaterials();
	void Environment_InitializePortalMaterials();
	void Environment_InitializeGrassMaterials();
	void Environment_InitializeMushroomMaterials();
};