#include "EditorConfigUtils.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include "Goknar/Managers/ConfigManager.h"
#include "UI/EditorGameProjectBuildUtils.h"

namespace
{
	constexpr const char* kEditorConfigSection = "Editor";
	constexpr const char* kGammaCorrectionKey = "GammaCorrection";
	constexpr const char* kCurrentProjectKey = "CurrentProject";
	constexpr const char* kCurrentProjectPathKey = "CurrentProjectPath";

	std::string Trim(const std::string& value)
	{
		const size_t firstNonWhitespace = value.find_first_not_of(" \t\r\n");
		if (firstNonWhitespace == std::string::npos)
		{
			return "";
		}

		const size_t lastNonWhitespace = value.find_last_not_of(" \t\r\n");
		return value.substr(firstNonWhitespace, lastNonWhitespace - firstNonWhitespace + 1);
	}

	std::string EnsureTrailingSlash(const std::string& path)
	{
		std::string normalizedPath = std::filesystem::path(path).lexically_normal().generic_string();
		if (!normalizedPath.empty() && normalizedPath.back() != '/')
		{
			normalizedPath += '/';
		}

		return normalizedPath;
	}

	std::string FormatFloat(float value)
	{
		std::ostringstream stream;
		stream << std::fixed << std::setprecision(3) << value;
		std::string formattedValue = stream.str();

		while (formattedValue.size() > 1 && formattedValue.back() == '0')
		{
			formattedValue.pop_back();
		}

		if (!formattedValue.empty() && formattedValue.back() == '.')
		{
			formattedValue.pop_back();
		}

		return formattedValue;
	}
}

float EditorConfigUtils::GetGammaCorrection(float defaultGammaCorrection)
{
	ConfigManager editorConfig;
	if (!editorConfig.ReadFile(EditorGameProjectBuildUtils::GetEditorConfigPath()))
	{
		return defaultGammaCorrection;
	}

	return editorConfig.GetFloat(kEditorConfigSection, kGammaCorrectionKey, defaultGammaCorrection);
}

bool EditorConfigUtils::SetGammaCorrection(float gammaCorrection)
{
	if (gammaCorrection <= 0.f)
	{
		return false;
	}

	return SetEditorConfigValue(kEditorConfigSection, kGammaCorrectionKey, FormatFloat(gammaCorrection));
}

bool EditorConfigUtils::SetEditorConfigValue(const std::string& sectionName, const std::string& keyName, const std::string& value)
{
	return SetEditorConfigValue(EditorGameProjectBuildUtils::GetEditorConfigPath(), sectionName, keyName, value);
}

bool EditorConfigUtils::SetEditorConfigValue(const std::string& filePath, const std::string& sectionName, const std::string& keyName, const std::string& value)
{
	std::vector<std::string> lines;
	{
		std::ifstream configFile(filePath);
		std::string line;
		while (std::getline(configFile, line))
		{
			lines.push_back(line);
		}
	}

	bool isInTargetSection = false;
	bool didFindTargetSection = false;
	size_t insertIndex = lines.size();

	for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
	{
		const std::string trimmedLine = Trim(lines[lineIndex]);
		if (trimmedLine.empty() || trimmedLine[0] == ';' || trimmedLine[0] == '#')
		{
			continue;
		}

		if (trimmedLine.front() == '[' && trimmedLine.back() == ']')
		{
			if (isInTargetSection)
			{
				insertIndex = lineIndex;
			}

			const std::string currentSectionName = Trim(trimmedLine.substr(1, trimmedLine.size() - 2));
			isInTargetSection = currentSectionName == sectionName;
			didFindTargetSection = didFindTargetSection || isInTargetSection;
			continue;
		}

		if (!isInTargetSection)
		{
			continue;
		}

		insertIndex = lineIndex + 1;
		const size_t equalsIndex = lines[lineIndex].find('=');
		if (equalsIndex == std::string::npos)
		{
			continue;
		}

		const std::string currentKey = Trim(lines[lineIndex].substr(0, equalsIndex));
		if (currentKey == keyName)
		{
			size_t valueStartIndex = equalsIndex + 1;
			while (valueStartIndex < lines[lineIndex].size() && std::isspace(static_cast<unsigned char>(lines[lineIndex][valueStartIndex])))
			{
				++valueStartIndex;
			}

			lines[lineIndex] = lines[lineIndex].substr(0, valueStartIndex) + value;
			insertIndex = lines.size();
			didFindTargetSection = true;
			goto writeConfig;
		}
	}

	if (!didFindTargetSection)
	{
		if (!lines.empty() && !lines.back().empty())
		{
			lines.push_back("");
		}

		lines.push_back("[" + sectionName + "]");
		lines.push_back(keyName + "=" + value);
	}
	else
	{
		lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insertIndex), keyName + "=" + value);
	}

writeConfig:
	std::error_code errorCode;
	const std::filesystem::path parentPath = std::filesystem::path(filePath).parent_path();
	if (!parentPath.empty())
	{
		std::filesystem::create_directories(parentPath, errorCode);
	}

	std::ofstream configFile(filePath);
	if (!configFile.is_open())
	{
		return false;
	}

	for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
	{
		configFile << lines[lineIndex];
		if (lineIndex + 1 < lines.size())
		{
			configFile << "\n";
		}
	}

	return true;
}

bool EditorConfigUtils::SetCurrentProject(const std::string& currentProjectName, const std::string& currentProjectPath)
{
	bool didSucceed = true;
	if (!currentProjectName.empty())
	{
		didSucceed &= SetEditorConfigValue(kEditorConfigSection, kCurrentProjectKey, currentProjectName);
	}

	if (!currentProjectPath.empty())
	{
		didSucceed &= SetEditorConfigValue(kEditorConfigSection, kCurrentProjectPathKey, EnsureTrailingSlash(currentProjectPath));
	}

	return didSucceed;
}
