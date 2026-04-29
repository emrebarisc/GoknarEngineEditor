#pragma once

#include <string>

namespace EditorAssetPathUtils
{
	std::string NormalizePath(const std::string& path);
	std::string GetProjectRootPath();
	std::string GetContentRootPath();
	std::string GetEditorRootPath();
	std::string ToContentRelativePath(const std::string& assetPath);
	std::string ToEditorReflectionPath(const std::string& assetPath);
	bool EnsureDirectoryForFile(const std::string& filepath);
}
