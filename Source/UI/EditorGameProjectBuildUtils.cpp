#include "EditorGameProjectBuildUtils.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Goknar/Log.h"

#ifdef GOKNAR_PLATFORM_WINDOWS
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#else
	#include <limits.h>
	#include <sys/types.h>
	#include <unistd.h>
	#if defined(__APPLE__)
		#include <mach-o/dyld.h>
	#endif
#endif

#ifndef EDITOR_GAME_PROJECT_BUILD_STAMP_PATH
	#define EDITOR_GAME_PROJECT_BUILD_STAMP_PATH ""
#endif

#ifndef EDITOR_BUILD_DIRECTORY_PATH
	#define EDITOR_BUILD_DIRECTORY_PATH ""
#endif

#ifndef EDITOR_SOURCE_DIRECTORY_PATH
	#define EDITOR_SOURCE_DIRECTORY_PATH ""
#endif

#ifndef EDITOR_BUILD_CONFIG_NAME
	#define EDITOR_BUILD_CONFIG_NAME "Debug"
#endif

#ifndef EDITOR_EXECUTABLE_TARGET_NAME
	#define EDITOR_EXECUTABLE_TARGET_NAME "GoknarEngineEditor"
#endif

#ifndef EDITOR_RESTART_HELPER_EXECUTABLE_NAME
	#ifdef GOKNAR_PLATFORM_WINDOWS
		#define EDITOR_RESTART_HELPER_EXECUTABLE_NAME "EditorGameProjectBuildHelper.exe"
	#else
		#define EDITOR_RESTART_HELPER_EXECUTABLE_NAME "EditorGameProjectBuildHelper"
	#endif
#endif

namespace
{
	constexpr const char* kEditorConfigRelativePath = "Config/EditorConfig.ini";
	constexpr const char* kCurrentProjectPathKey = "CurrentProjectPath";
	constexpr const char* kCompiledProjectPathKey = "CompiledProjectPath";

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

	std::string NormalizePath(const std::string& path)
	{
		if (path.empty())
		{
			return "";
		}

		std::string normalizedPath = std::filesystem::path(path).lexically_normal().generic_string();
		while (3 < normalizedPath.size() && normalizedPath.back() == '/')
		{
			normalizedPath.pop_back();
		}
		return normalizedPath;
	}

	std::string ReadConfigValue(const std::string& filePath, const std::string& key)
	{
		std::ifstream configFile(filePath);
		if (!configFile.is_open())
		{
			return "";
		}

		std::string line;
		while (std::getline(configFile, line))
		{
			const size_t equalsIndex = line.find('=');
			if (equalsIndex == std::string::npos)
			{
				continue;
			}

			if (Trim(line.substr(0, equalsIndex)) == key)
			{
				return Trim(line.substr(equalsIndex + 1));
			}
		}

		return "";
	}

	std::string GetEditorSourceDirectory()
	{
		const std::string configuredSourceDirectory = NormalizePath(EDITOR_SOURCE_DIRECTORY_PATH);
		if (!configuredSourceDirectory.empty())
		{
			return configuredSourceDirectory;
		}

		return NormalizePath(std::filesystem::current_path().generic_string());
	}

	std::string BuildEditorConfigPath()
	{
		return NormalizePath((std::filesystem::path(GetEditorSourceDirectory()) / kEditorConfigRelativePath).generic_string());
	}

#ifdef GOKNAR_PLATFORM_WINDOWS
	std::string QuoteWindowsCommandArgument(const std::string& value)
	{
		if (value.empty())
		{
			return "\"\"";
		}

		if (value.find_first_of(" \t\n\v\"") == std::string::npos)
		{
			return value;
		}

		std::string quotedValue = "\"";
		size_t backslashCount = 0;
		for (const char character : value)
		{
			if (character == '\\')
			{
				++backslashCount;
				continue;
			}

			if (character == '"')
			{
				quotedValue.append(backslashCount * 2 + 1, '\\');
				quotedValue += character;
				backslashCount = 0;
				continue;
			}

			quotedValue.append(backslashCount, '\\');
			backslashCount = 0;
			quotedValue += character;
		}

		quotedValue.append(backslashCount * 2, '\\');
		quotedValue += '"';
		return quotedValue;
	}

	std::string BuildWindowsCommandLine(const std::string& executablePath, const std::vector<std::string>& arguments)
	{
		std::string commandLine = QuoteWindowsCommandArgument(executablePath);
		for (const std::string& argument : arguments)
		{
			commandLine += " ";
			commandLine += QuoteWindowsCommandArgument(argument);
		}
		return commandLine;
	}

	std::string GetExecutablePath()
	{
		std::vector<char> executablePath(MAX_PATH);
		DWORD copiedCharacterCount = GetModuleFileNameA(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
		while (copiedCharacterCount == executablePath.size())
		{
			executablePath.resize(executablePath.size() * 2);
			copiedCharacterCount = GetModuleFileNameA(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
		}

		if (copiedCharacterCount == 0)
		{
			return "";
		}

		return NormalizePath(std::string(executablePath.data(), copiedCharacterCount));
	}

	bool LaunchDetachedProcess(const std::string& executablePath, const std::vector<std::string>& arguments, const std::string& workingDirectory, bool createConsole)
	{
		if (executablePath.empty())
		{
			return false;
		}

		std::string commandLine = BuildWindowsCommandLine(executablePath, arguments);
		STARTUPINFOA startupInfo{};
		startupInfo.cb = sizeof(startupInfo);

		PROCESS_INFORMATION processInfo{};
		const DWORD creationFlags = CREATE_NEW_PROCESS_GROUP | (createConsole ? CREATE_NEW_CONSOLE : 0);
		const BOOL processCreated = CreateProcessA(
			executablePath.c_str(),
			commandLine.data(),
			nullptr,
			nullptr,
			FALSE,
			creationFlags,
			nullptr,
			workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
			&startupInfo,
			&processInfo);

		if (!processCreated)
		{
			return false;
		}

		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return true;
	}

	std::string GetCurrentProcessIdString()
	{
		return std::to_string(GetCurrentProcessId());
	}
#else
	std::string GetExecutablePath()
	{
	#if defined(__APPLE__)
		uint32_t executablePathSize = 0;
		_NSGetExecutablePath(nullptr, &executablePathSize);
		if (executablePathSize == 0)
		{
			return "";
		}

		std::vector<char> executablePath(executablePathSize);
		if (_NSGetExecutablePath(executablePath.data(), &executablePathSize) != 0)
		{
			return "";
		}

		return NormalizePath(std::filesystem::weakly_canonical(executablePath.data()).generic_string());
	#elif defined(__linux__)
		std::vector<char> executablePath(PATH_MAX);
		ssize_t copiedCharacterCount = readlink("/proc/self/exe", executablePath.data(), executablePath.size());
		while (copiedCharacterCount == static_cast<ssize_t>(executablePath.size()))
		{
			executablePath.resize(executablePath.size() * 2);
			copiedCharacterCount = readlink("/proc/self/exe", executablePath.data(), executablePath.size());
		}

		if (copiedCharacterCount <= 0)
		{
			return "";
		}

		return NormalizePath(std::string(executablePath.data(), static_cast<size_t>(copiedCharacterCount)));
	#else
		return "";
	#endif
	}

	bool LaunchDetachedProcess(const std::string& executablePath, const std::vector<std::string>& arguments, const std::string& workingDirectory, bool)
	{
		const pid_t childProcessId = fork();
		if (childProcessId < 0)
		{
			return false;
		}

		if (childProcessId == 0)
		{
			setsid();
			if (!workingDirectory.empty())
			{
				chdir(workingDirectory.c_str());
			}

			std::vector<char*> processArguments;
			processArguments.reserve(arguments.size() + 2);
			processArguments.push_back(const_cast<char*>(executablePath.c_str()));
			for (const std::string& argument : arguments)
			{
				processArguments.push_back(const_cast<char*>(argument.c_str()));
			}
			processArguments.push_back(nullptr);

			execv(executablePath.c_str(), processArguments.data());
			_exit(127);
		}

		return true;
	}

	std::string GetCurrentProcessIdString()
	{
		return std::to_string(getpid());
	}
#endif

	std::string GetExecutableDirectory()
	{
		const std::string executablePath = GetExecutablePath();
		if (executablePath.empty())
		{
			return "";
		}

		return NormalizePath(std::filesystem::path(executablePath).parent_path().generic_string());
	}
}

std::string EditorGameProjectBuildUtils::GetCurrentEditorConfigProjectRoot()
{
	return NormalizePath(ReadConfigValue(GetEditorConfigPath(), kCurrentProjectPathKey));
}

std::string EditorGameProjectBuildUtils::GetEditorSourceRoot()
{
	return GetEditorSourceDirectory();
}

std::string EditorGameProjectBuildUtils::GetEditorConfigPath()
{
	return BuildEditorConfigPath();
}

std::string EditorGameProjectBuildUtils::GetCompiledGameEditorProjectRoot()
{
	return NormalizePath(ReadConfigValue(EDITOR_GAME_PROJECT_BUILD_STAMP_PATH, kCompiledProjectPathKey));
}

bool EditorGameProjectBuildUtils::IsCompiledGameEditorProjectCurrent(const std::string& currentProjectRoot)
{
	const std::string normalizedCurrentProjectRoot = NormalizePath(currentProjectRoot.empty() ? GetCurrentEditorConfigProjectRoot() : currentProjectRoot);
	const std::string compiledProjectRoot = GetCompiledGameEditorProjectRoot();
	return !normalizedCurrentProjectRoot.empty() && normalizedCurrentProjectRoot == compiledProjectRoot;
}

bool EditorGameProjectBuildUtils::RestartEditor(bool rebuildGameEditorProject)
{
	const std::string executablePath = GetExecutablePath();
	const std::string executableDirectory = GetExecutableDirectory();
	if (executablePath.empty() || executableDirectory.empty())
	{
		return false;
	}

	if (!rebuildGameEditorProject)
	{
		return LaunchDetachedProcess(executablePath, {}, executableDirectory, false);
	}

	const std::string editorSourceDirectory = GetEditorSourceDirectory();
	const std::string editorBuildDirectory = NormalizePath(EDITOR_BUILD_DIRECTORY_PATH);
	if (editorSourceDirectory.empty() || editorBuildDirectory.empty())
	{
		return false;
	}

	const std::string helperExecutablePath = NormalizePath((std::filesystem::path(executableDirectory) / EDITOR_RESTART_HELPER_EXECUTABLE_NAME).generic_string());
	if (!std::filesystem::exists(helperExecutablePath))
	{
		GOKNAR_CORE_ERROR("Editor build helper executable was not found at %s.", helperExecutablePath.c_str());
		return false;
	}

	std::vector<std::string> helperArguments =
	{
		"--parent-pid", GetCurrentProcessIdString(),
		"--editor-exe", executablePath,
		"--editor-working-dir", executableDirectory,
		"--source-dir", editorSourceDirectory,
		"--build-dir", editorBuildDirectory,
		"--config", EDITOR_BUILD_CONFIG_NAME,
		"--target", EDITOR_EXECUTABLE_TARGET_NAME
	};

	const std::string currentProjectRoot = GetCurrentEditorConfigProjectRoot();
	if (!currentProjectRoot.empty())
	{
		helperArguments.push_back("--project-path");
		helperArguments.push_back(currentProjectRoot);
	}

	return LaunchDetachedProcess(helperExecutablePath, helperArguments, editorSourceDirectory, true);
}
