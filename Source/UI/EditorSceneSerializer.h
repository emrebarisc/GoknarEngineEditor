#pragma once

#include <string>

class Scene;
class DirectionalLight;
class SpotLight;
struct Vector3;

namespace EditorSceneSerializer
{
	bool OpenScene(const std::string& path);
	void SaveScene(Scene* scene, const std::string& filePath);
	bool GetAuthoredDirection(const DirectionalLight* light, Vector3& outDirection);
	bool GetAuthoredDirection(const SpotLight* light, Vector3& outDirection);
	void SetAuthoredDirection(const DirectionalLight* light, const Vector3& direction);
	void SetAuthoredDirection(const SpotLight* light, const Vector3& direction);
}
