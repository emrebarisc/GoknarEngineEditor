#pragma once

#include <string>

namespace EditorConfigUtils
{
	constexpr float DefaultGammaCorrection = 2.2f;

	float GetGammaCorrection(float defaultGammaCorrection = DefaultGammaCorrection);
	bool SetGammaCorrection(float gammaCorrection);

	bool SetEditorConfigValue(const std::string& sectionName, const std::string& keyName, const std::string& value);
	bool SetEditorConfigValue(const std::string& filePath, const std::string& sectionName, const std::string& keyName, const std::string& value);
	bool SetCurrentProject(const std::string& currentProjectName, const std::string& currentProjectPath);
}
