#pragma once

#include <string>

namespace EditorGameProjectBuildUtils
{
	std::string GetEditorSourceRoot();
	std::string GetEditorConfigPath();
	std::string GetCurrentEditorConfigProjectRoot();
	std::string GetCompiledGameEditorProjectRoot();
	bool IsCompiledGameEditorProjectCurrent(const std::string& currentProjectRoot);
	bool RestartEditor(bool rebuildGameEditorProject);
}
