#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef GOKNAR_PLATFORM_WINDOWS
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#else
	#include <signal.h>
	#include <sys/types.h>
	#include <sys/wait.h>
	#include <unistd.h>
#endif

namespace
{
	struct BuildHelperArguments
	{
		unsigned long parentProcessId{ 0 };
		std::string editorExecutablePath;
		std::string editorWorkingDirectory;
		std::string sourceDirectory;
		std::string buildDirectory;
		std::string configName{ "Debug" };
		std::string targetName{ "GoknarEngineEditor" };
		std::string projectRootPath;
	};

	bool ReadValueArgument(int argc, char** argv, int& argumentIndex, std::string& outValue)
	{
		if (argumentIndex + 1 >= argc)
		{
			return false;
		}

		outValue = argv[++argumentIndex];
		return true;
	}

	bool ParseArguments(int argc, char** argv, BuildHelperArguments& outArguments)
	{
		for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex)
		{
			const std::string argument = argv[argumentIndex];
			std::string value;
			if (argument == "--parent-pid")
			{
				if (!ReadValueArgument(argc, argv, argumentIndex, value))
				{
					return false;
				}
				outArguments.parentProcessId = std::strtoul(value.c_str(), nullptr, 10);
			}
			else if (argument == "--editor-exe")
			{
				if (!ReadValueArgument(argc, argv, argumentIndex, outArguments.editorExecutablePath))
				{
					return false;
				}
			}
			else if (argument == "--editor-working-dir")
			{
				if (!ReadValueArgument(argc, argv, argumentIndex, outArguments.editorWorkingDirectory))
				{
					return false;
				}
			}
			else if (argument == "--source-dir")
			{
				if (!ReadValueArgument(argc, argv, argumentIndex, outArguments.sourceDirectory))
				{
					return false;
				}
			}
			else if (argument == "--build-dir")
			{
				if (!ReadValueArgument(argc, argv, argumentIndex, outArguments.buildDirectory))
				{
					return false;
				}
			}
			else if (argument == "--config")
			{
				if (!ReadValueArgument(argc, argv, argumentIndex, outArguments.configName))
				{
					return false;
				}
			}
			else if (argument == "--target")
			{
				if (!ReadValueArgument(argc, argv, argumentIndex, outArguments.targetName))
				{
					return false;
				}
			}
			else if (argument == "--project-path")
			{
				if (!ReadValueArgument(argc, argv, argumentIndex, outArguments.projectRootPath))
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}

		return outArguments.parentProcessId != 0 &&
			!outArguments.editorExecutablePath.empty() &&
			!outArguments.editorWorkingDirectory.empty() &&
			!outArguments.sourceDirectory.empty() &&
			!outArguments.buildDirectory.empty();
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

	bool RunProcess(const std::string& executablePath, const std::vector<std::string>& arguments, const std::string& workingDirectory)
	{
		std::string commandLine = BuildWindowsCommandLine(executablePath, arguments);
		STARTUPINFOA startupInfo{};
		startupInfo.cb = sizeof(startupInfo);

		PROCESS_INFORMATION processInfo{};
		const BOOL processCreated = CreateProcessA(
			nullptr,
			commandLine.data(),
			nullptr,
			nullptr,
			FALSE,
			0,
			nullptr,
			workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
			&startupInfo,
			&processInfo);

		if (!processCreated)
		{
			std::cerr << "Failed to start process: " << executablePath << "\n";
			return false;
		}

		WaitForSingleObject(processInfo.hProcess, INFINITE);

		DWORD exitCode = 1;
		GetExitCodeProcess(processInfo.hProcess, &exitCode);
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);

		return exitCode == 0;
	}

	bool LaunchDetachedProcess(const std::string& executablePath, const std::string& workingDirectory)
	{
		std::string commandLine = QuoteWindowsCommandArgument(executablePath);
		STARTUPINFOA startupInfo{};
		startupInfo.cb = sizeof(startupInfo);

		PROCESS_INFORMATION processInfo{};
		const BOOL processCreated = CreateProcessA(
			executablePath.c_str(),
			commandLine.data(),
			nullptr,
			nullptr,
			FALSE,
			CREATE_NEW_PROCESS_GROUP,
			nullptr,
			workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
			&startupInfo,
			&processInfo);

		if (!processCreated)
		{
			std::cerr << "Failed to relaunch editor: " << executablePath << "\n";
			return false;
		}

		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return true;
	}

	void WaitForParentExit(unsigned long parentProcessId)
	{
		HANDLE parentProcessHandle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(parentProcessId));
		if (!parentProcessHandle)
		{
			return;
		}

		WaitForSingleObject(parentProcessHandle, INFINITE);
		CloseHandle(parentProcessHandle);
	}
#else
	bool RunProcess(const std::string& executablePath, const std::vector<std::string>& arguments, const std::string& workingDirectory)
	{
		const pid_t childProcessId = fork();
		if (childProcessId < 0)
		{
			std::cerr << "Failed to fork process: " << executablePath << "\n";
			return false;
		}

		if (childProcessId == 0)
		{
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

			execvp(executablePath.c_str(), processArguments.data());
			_exit(127);
		}

		int processStatus = 0;
		while (waitpid(childProcessId, &processStatus, 0) == -1)
		{
			if (errno != EINTR)
			{
				return false;
			}
		}

		return WIFEXITED(processStatus) && WEXITSTATUS(processStatus) == 0;
	}

	bool LaunchDetachedProcess(const std::string& executablePath, const std::string& workingDirectory)
	{
		const pid_t childProcessId = fork();
		if (childProcessId < 0)
		{
			std::cerr << "Failed to fork editor relaunch process.\n";
			return false;
		}

		if (childProcessId == 0)
		{
			setsid();
			if (!workingDirectory.empty())
			{
				chdir(workingDirectory.c_str());
			}

			execl(executablePath.c_str(), executablePath.c_str(), nullptr);
			_exit(127);
		}

		return true;
	}

	void WaitForParentExit(unsigned long parentProcessId)
	{
		for (;;)
		{
			if (kill(static_cast<pid_t>(parentProcessId), 0) == -1 && errno != EPERM)
			{
				return;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
#endif
}

int main(int argc, char** argv)
{
	BuildHelperArguments arguments;
	if (!ParseArguments(argc, argv, arguments))
	{
		std::cerr << "Invalid arguments for EditorGameProjectBuildHelper.\n";
		return 2;
	}

	WaitForParentExit(arguments.parentProcessId);

	std::vector<std::string> configureArguments =
	{
		"-S", arguments.sourceDirectory,
		"-B", arguments.buildDirectory
	};
	if (!arguments.projectRootPath.empty())
	{
		configureArguments.push_back("-DCURRENT_PROJECT_PATH=" + arguments.projectRootPath);
	}

	if (!RunProcess("cmake", configureArguments, arguments.sourceDirectory))
	{
		return 1;
	}

	std::vector<std::string> buildArguments =
	{
		"--build", arguments.buildDirectory,
		"--config", arguments.configName,
		"--target", arguments.targetName,
		"--parallel", "4"
	};

	if (!RunProcess("cmake", buildArguments, arguments.sourceDirectory))
	{
		return 1;
	}

	if (!LaunchDetachedProcess(arguments.editorExecutablePath, arguments.editorWorkingDirectory))
	{
		return 1;
	}

	return 0;
}
