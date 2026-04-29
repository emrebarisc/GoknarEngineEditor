#include "EditorAssetPathUtils.h"

#include <filesystem>

#include "Goknar/Core.h"

namespace
{
	std::string Normalize(const std::string& path)
	{
		if (path.empty())
		{
			return "";
		}

		std::string normalized = std::filesystem::path(path).lexically_normal().generic_string();
		while (normalized.find("//") != std::string::npos)
		{
			normalized.replace(normalized.find("//"), 2, "/");
		}

		if (normalized.rfind("./", 0) == 0)
		{
			normalized = normalized.substr(2);
		}

		return normalized;
	}

	bool StartsWith(const std::string& value, const std::string& prefix)
	{
		return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
	}

	std::string EnsureTrailingSlash(const std::string& path)
	{
		if (path.empty())
		{
			return path;
		}

		return path.back() == '/' ? path : path + "/";
	}
}

std::string EditorAssetPathUtils::NormalizePath(const std::string& path)
{
	return Normalize(path);
}

std::string EditorAssetPathUtils::GetProjectRootPath()
{
	if (!ProjectDir.empty())
	{
		return EnsureTrailingSlash(Normalize(ProjectDir));
	}

	const std::string normalizedContentDir = Normalize(ContentDir);
	if (!normalizedContentDir.empty())
	{
		std::filesystem::path contentPath(normalizedContentDir);
		if (!contentPath.is_absolute())
		{
			contentPath = std::filesystem::path(std::filesystem::current_path()) / contentPath;
		}

		contentPath = contentPath.lexically_normal();
		if (contentPath.filename() == "Content")
		{
			return EnsureTrailingSlash(contentPath.parent_path().generic_string());
		}
	}

	return EnsureTrailingSlash(std::filesystem::current_path().lexically_normal().generic_string());
}

std::string EditorAssetPathUtils::GetContentRootPath()
{
	const std::string normalizedContentDir = Normalize(ContentDir);
	if (!normalizedContentDir.empty())
	{
		std::filesystem::path contentPath(normalizedContentDir);
		if (!contentPath.is_absolute())
		{
			contentPath = std::filesystem::path(GetProjectRootPath()) / contentPath;
		}

		return EnsureTrailingSlash(contentPath.lexically_normal().generic_string());
	}

	return GetProjectRootPath() + "Content/";
}

std::string EditorAssetPathUtils::GetEditorRootPath()
{
	return GetProjectRootPath() + "Editor/";
}

std::string EditorAssetPathUtils::ToContentRelativePath(const std::string& assetPath)
{
	std::string normalizedAssetPath = Normalize(assetPath);
	if (normalizedAssetPath.empty())
	{
		return normalizedAssetPath;
	}

	const std::string contentRoot = GetContentRootPath();
	if (StartsWith(normalizedAssetPath, contentRoot))
	{
		return normalizedAssetPath.substr(contentRoot.size());
	}

	const std::string projectRoot = GetProjectRootPath();
	const std::string contentRelativePrefix = "Content/";
	if (StartsWith(normalizedAssetPath, projectRoot + contentRelativePrefix))
	{
		return normalizedAssetPath.substr((projectRoot + contentRelativePrefix).size());
	}

	if (StartsWith(normalizedAssetPath, contentRelativePrefix))
	{
		return normalizedAssetPath.substr(contentRelativePrefix.size());
	}

	const size_t contentPos = normalizedAssetPath.find("/Content/");
	if (contentPos != std::string::npos)
	{
		return normalizedAssetPath.substr(contentPos + 9);
	}

	return normalizedAssetPath;
}

std::string EditorAssetPathUtils::ToEditorReflectionPath(const std::string& assetPath)
{
	return Normalize(GetEditorRootPath() + ToContentRelativePath(assetPath));
}

bool EditorAssetPathUtils::EnsureDirectoryForFile(const std::string& filepath)
{
	std::filesystem::path filePath(Normalize(filepath));
	std::filesystem::path directory = filePath.parent_path();
	if (directory.empty())
	{
		return true;
	}

	std::error_code errorCode;
	std::filesystem::create_directories(directory, errorCode);
	return !errorCode;
}
