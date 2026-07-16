#pragma once

#include <string>

class Scene;

namespace EditorSceneSerializer
{
	bool OpenScene(const std::string& path);
	void SaveScene(Scene* scene, const std::string& filePath);
}
