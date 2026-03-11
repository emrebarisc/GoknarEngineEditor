#pragma once

#include "Goknar/Core.h"
#include "Goknar/Delegates/MulticastDelegate.h"

class Game;

class GOKNAR_API MaterialInitializer
{
public:
	MaterialInitializer() = delete;

	static void Init();

protected:

private:
	static void Skills_InitializeTargetObjectMaterials();
	static void Skills_InitializeFireBurstCastedObjectMaterials();
	static void Skills_InitializeSlashMaterials();
	static void Environment_InitializePortalMaterials();
	static void Environment_InitializeGrassMaterials();
	static void Environment_InitializeMushroomMaterials();
	static void Environment_InitializePondMaterials();

	static void DefaultSceneAssets();
};