#include "EditorSourceCodeUtils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifdef GOKNAR_PLATFORM_WINDOWS
#include <cwctype>
#include <objbase.h>
#include <oleauto.h>
#include <shellapi.h>
#include <windows.h>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Shell32.lib")
#endif

#include "Goknar/Managers/ConfigManager.h"
#include "UI/EditorAssetPathUtils.h"

namespace
{
	constexpr const char* kEditorConfigPath = "Config/EditorConfig.ini";
	constexpr const char* kEditorConfigSection = "Editor";
	constexpr const char* kSourceCodeEditorKey = "SourceCodeEditor";

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

	std::string ToLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		return value;
	}

	std::filesystem::path NormalizeFilesystemPath(const std::filesystem::path& path)
	{
		std::error_code errorCode;
		std::filesystem::path absolutePath = std::filesystem::absolute(path, errorCode);
		if (errorCode)
		{
			absolutePath = path;
		}

		std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(absolutePath, errorCode);
		if (!errorCode)
		{
			return canonicalPath;
		}

		return absolutePath.lexically_normal();
	}

	std::string ReadProjectNameFromBuildConfig(const std::filesystem::path& projectRoot)
	{
		std::ifstream buildConfig(projectRoot / "Config" / "Build.ini");
		if (!buildConfig.is_open())
		{
			return "";
		}

		std::string line;
		while (std::getline(buildConfig, line))
		{
			const size_t equalsIndex = line.find('=');
			if (equalsIndex == std::string::npos)
			{
				continue;
			}

			const std::string key = Trim(line.substr(0, equalsIndex));
			if (key == "ProjectName")
			{
				return Trim(line.substr(equalsIndex + 1));
			}
		}

		return "";
	}

	std::string FindProjectSolutionPath()
	{
		const std::filesystem::path projectRoot = NormalizeFilesystemPath(EditorAssetPathUtils::GetProjectRootPath());
		std::string projectName = ReadProjectNameFromBuildConfig(projectRoot);
		if (projectName.empty())
		{
			projectName = projectRoot.filename().generic_string();
		}

		std::vector<std::filesystem::path> candidates;
		if (!projectName.empty())
		{
			candidates.push_back(projectRoot / "Build_Debug" / (projectName + ".sln"));
			candidates.push_back(projectRoot / "Build_Release" / (projectName + ".sln"));
			candidates.push_back(projectRoot / "Build_RelWithDebInfo" / (projectName + ".sln"));
			candidates.push_back(projectRoot / "Build" / (projectName + ".sln"));
			candidates.push_back(projectRoot / (projectName + ".sln"));
		}

		for (const std::filesystem::path& candidate : candidates)
		{
			if (std::filesystem::exists(candidate))
			{
				return NormalizeFilesystemPath(candidate).generic_string();
			}
		}

		std::error_code errorCode;
		if (!std::filesystem::exists(projectRoot, errorCode) || errorCode)
		{
			return "";
		}

		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(projectRoot, std::filesystem::directory_options::skip_permission_denied, errorCode))
		{
			if (errorCode)
			{
				break;
			}

			const std::filesystem::path entryPath = entry.path();
			if (entry.is_regular_file(errorCode) && ToLower(entryPath.extension().generic_string()) == ".sln")
			{
				return NormalizeFilesystemPath(entryPath).generic_string();
			}

			if (!entry.is_directory(errorCode))
			{
				continue;
			}

			const std::string directoryName = entryPath.filename().generic_string();
			if (directoryName.rfind("Build", 0) != 0)
			{
				continue;
			}

			const std::filesystem::path namedSolution = entryPath / (projectName + ".sln");
			if (!projectName.empty() && std::filesystem::exists(namedSolution))
			{
				return NormalizeFilesystemPath(namedSolution).generic_string();
			}

			for (const std::filesystem::directory_entry& buildEntry : std::filesystem::directory_iterator(entryPath, std::filesystem::directory_options::skip_permission_denied, errorCode))
			{
				if (errorCode)
				{
					break;
				}

				if (buildEntry.is_regular_file(errorCode) && ToLower(buildEntry.path().extension().generic_string()) == ".sln")
				{
					return NormalizeFilesystemPath(buildEntry.path()).generic_string();
				}
			}
		}

		return "";
	}

	bool SetConfigValue(const std::string& filePath, const std::string& sectionName, const std::string& keyName, const std::string& value)
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

	std::string QuoteShellPath(const std::string& path)
	{
		std::string quotedPath = "'";
		for (char character : path)
		{
			if (character == '\'')
			{
				quotedPath += "'\\''";
			}
			else
			{
				quotedPath += character;
			}
		}

		quotedPath += "'";
		return quotedPath;
	}

#ifdef GOKNAR_PLATFORM_WINDOWS
	class ScopedComInitializer
	{
	public:
		ScopedComInitializer()
		{
			result_ = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		}

		~ScopedComInitializer()
		{
			if (SUCCEEDED(result_))
			{
				CoUninitialize();
			}
		}

		bool IsUsable() const
		{
			return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
		}

	private:
		HRESULT result_{ E_FAIL };
	};

	std::wstring ToNativeWidePath(const std::string& path)
	{
		std::wstring widePath = NormalizeFilesystemPath(path).wstring();
		std::replace(widePath.begin(), widePath.end(), L'/', L'\\');
		return widePath;
	}

	std::wstring ToComparablePath(const std::wstring& path)
	{
		std::wstring comparablePath = NormalizeFilesystemPath(std::filesystem::path(path)).wstring();
		std::replace(comparablePath.begin(), comparablePath.end(), L'/', L'\\');
		while (!comparablePath.empty() && (comparablePath.back() == L'\\' || comparablePath.back() == L'/'))
		{
			comparablePath.pop_back();
		}

		std::transform(comparablePath.begin(), comparablePath.end(), comparablePath.begin(), [](wchar_t character)
			{
				return static_cast<wchar_t>(std::towlower(character));
			});
		return comparablePath;
	}

	bool AreSamePath(const std::wstring& left, const std::wstring& right)
	{
		return ToComparablePath(left) == ToComparablePath(right);
	}

	bool TryGetDispatchId(IDispatch* dispatch, const wchar_t* name, DISPID& dispatchId)
	{
		if (!dispatch)
		{
			return false;
		}

		LPOLESTR mutableName = const_cast<LPOLESTR>(name);
		return SUCCEEDED(dispatch->GetIDsOfNames(IID_NULL, &mutableName, 1, LOCALE_USER_DEFAULT, &dispatchId));
	}

	bool GetDispatchProperty(IDispatch* dispatch, const wchar_t* propertyName, VARIANT& result)
	{
		VariantInit(&result);

		DISPID dispatchId;
		if (!TryGetDispatchId(dispatch, propertyName, dispatchId))
		{
			return false;
		}

		DISPPARAMS noArguments = { nullptr, nullptr, 0, 0 };
		return SUCCEEDED(dispatch->Invoke(dispatchId, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &noArguments, &result, nullptr, nullptr));
	}

	IDispatch* GetDispatchProperty(IDispatch* dispatch, const wchar_t* propertyName)
	{
		VARIANT result;
		if (!GetDispatchProperty(dispatch, propertyName, result))
		{
			return nullptr;
		}

		IDispatch* propertyDispatch = nullptr;
		if (result.vt == VT_DISPATCH && result.pdispVal)
		{
			propertyDispatch = result.pdispVal;
			propertyDispatch->AddRef();
		}
		else if (result.vt == VT_UNKNOWN && result.punkVal)
		{
			result.punkVal->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&propertyDispatch));
		}

		VariantClear(&result);
		return propertyDispatch;
	}

	std::wstring GetStringProperty(IDispatch* dispatch, const wchar_t* propertyName)
	{
		VARIANT result;
		if (!GetDispatchProperty(dispatch, propertyName, result))
		{
			return L"";
		}

		std::wstring value;
		if (result.vt == VT_BSTR && result.bstrVal)
		{
			value = result.bstrVal;
		}

		VariantClear(&result);
		return value;
	}

	long GetLongProperty(IDispatch* dispatch, const wchar_t* propertyName)
	{
		VARIANT result;
		if (!GetDispatchProperty(dispatch, propertyName, result))
		{
			return 0;
		}

		long value = 0;
		if (result.vt == VT_I4 || result.vt == VT_INT)
		{
			value = result.lVal;
		}

		VariantClear(&result);
		return value;
	}

	bool PutBoolProperty(IDispatch* dispatch, const wchar_t* propertyName, bool value)
	{
		DISPID dispatchId;
		if (!TryGetDispatchId(dispatch, propertyName, dispatchId))
		{
			return false;
		}

		VARIANT argument;
		VariantInit(&argument);
		argument.vt = VT_BOOL;
		argument.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;

		DISPID namedArgument = DISPID_PROPERTYPUT;
		DISPPARAMS parameters = { &argument, &namedArgument, 1, 1 };
		const bool succeeded = SUCCEEDED(dispatch->Invoke(dispatchId, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &parameters, nullptr, nullptr, nullptr));
		VariantClear(&argument);
		return succeeded;
	}

	bool InvokeMethod(IDispatch* dispatch, const wchar_t* methodName, VARIANTARG* arguments = nullptr, UINT argumentCount = 0, VARIANT* result = nullptr)
	{
		DISPID dispatchId;
		if (!TryGetDispatchId(dispatch, methodName, dispatchId))
		{
			return false;
		}

		VARIANT localResult;
		if (!result)
		{
			VariantInit(&localResult);
			result = &localResult;
		}

		DISPPARAMS parameters = { arguments, nullptr, argumentCount, 0 };
		const bool succeeded = SUCCEEDED(dispatch->Invoke(dispatchId, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &parameters, result, nullptr, nullptr));
		if (result == &localResult)
		{
			VariantClear(&localResult);
		}

		return succeeded;
	}

	bool InvokeMethodWithString(IDispatch* dispatch, const wchar_t* methodName, const std::wstring& argument, VARIANT* result = nullptr)
	{
		VARIANTARG methodArgument;
		VariantInit(&methodArgument);
		methodArgument.vt = VT_BSTR;
		methodArgument.bstrVal = SysAllocString(argument.c_str());

		const bool succeeded = InvokeMethod(dispatch, methodName, &methodArgument, 1, result);
		VariantClear(&methodArgument);
		return succeeded;
	}

	bool InvokeMethodWithInt(IDispatch* dispatch, const wchar_t* methodName, int argument)
	{
		VARIANTARG methodArgument;
		VariantInit(&methodArgument);
		methodArgument.vt = VT_I4;
		methodArgument.lVal = argument;

		return InvokeMethod(dispatch, methodName, &methodArgument, 1);
	}

	void ActivateVisualStudio(IDispatch* dte)
	{
		IDispatch* mainWindow = GetDispatchProperty(dte, L"MainWindow");
		if (!mainWindow)
		{
			return;
		}

		PutBoolProperty(mainWindow, L"Visible", true);
		InvokeMethod(mainWindow, L"Activate");

		const long windowHandle = GetLongProperty(mainWindow, L"HWnd");
		if (windowHandle != 0)
		{
			HWND hwnd = reinterpret_cast<HWND>(static_cast<intptr_t>(windowHandle));
			ShowWindow(hwnd, SW_SHOWNORMAL);
			SetForegroundWindow(hwnd);
		}

		mainWindow->Release();
	}

	std::wstring GetVisualStudioSolutionFullName(IDispatch* dte)
	{
		IDispatch* solution = GetDispatchProperty(dte, L"Solution");
		if (!solution)
		{
			return L"";
		}

		const std::wstring fullName = GetStringProperty(solution, L"FullName");
		solution->Release();
		return fullName;
	}

	bool HasVisualStudioProgId()
	{
		const wchar_t* visualStudioProgIds[] =
		{
			L"VisualStudio.DTE.17.0",
			L"VisualStudio.DTE.16.0",
			L"VisualStudio.DTE.15.0",
			L"VisualStudio.DTE.14.0",
			L"VisualStudio.DTE.12.0",
			L"VisualStudio.DTE"
		};

		for (const wchar_t* progId : visualStudioProgIds)
		{
			CLSID classId;
			if (SUCCEEDED(CLSIDFromProgID(progId, &classId)))
			{
				return true;
			}
		}

		return false;
	}

	IDispatch* CreateVisualStudioDte()
	{
		const wchar_t* visualStudioProgIds[] =
		{
			L"VisualStudio.DTE.17.0",
			L"VisualStudio.DTE.16.0",
			L"VisualStudio.DTE.15.0",
			L"VisualStudio.DTE.14.0",
			L"VisualStudio.DTE.12.0",
			L"VisualStudio.DTE"
		};

		for (const wchar_t* progId : visualStudioProgIds)
		{
			CLSID classId;
			if (FAILED(CLSIDFromProgID(progId, &classId)))
			{
				continue;
			}

			IDispatch* dte = nullptr;
			if (SUCCEEDED(CoCreateInstance(classId, nullptr, CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void**>(&dte))) && dte)
			{
				return dte;
			}
		}

		return nullptr;
	}

	bool NavigateVisualStudioToLine(IDispatch* dte, int lineNumber)
	{
		if (!dte || lineNumber <= 0)
		{
			return true;
		}

		IDispatch* activeDocument = GetDispatchProperty(dte, L"ActiveDocument");
		if (!activeDocument)
		{
			return false;
		}

		IDispatch* selection = GetDispatchProperty(activeDocument, L"Selection");
		activeDocument->Release();
		if (!selection)
		{
			return false;
		}

		const bool didNavigate = InvokeMethodWithInt(selection, L"GotoLine", lineNumber);
		selection->Release();
		return didNavigate;
	}

	bool OpenFileInVisualStudio(IDispatch* dte, const std::wstring& sourceFilePath, int lineNumber)
	{
		if (!dte)
		{
			return false;
		}

		PutBoolProperty(dte, L"UserControl", true);
		ActivateVisualStudio(dte);

		IDispatch* itemOperations = GetDispatchProperty(dte, L"ItemOperations");
		if (!itemOperations)
		{
			return false;
		}

		VARIANT result;
		VariantInit(&result);
		const bool didOpenFile = InvokeMethodWithString(itemOperations, L"OpenFile", sourceFilePath, &result);
		itemOperations->Release();

		if (didOpenFile && result.vt == VT_DISPATCH && result.pdispVal)
		{
			InvokeMethod(result.pdispVal, L"Activate");
		}
		VariantClear(&result);

		if (didOpenFile)
		{
			NavigateVisualStudioToLine(dte, lineNumber);
		}

		ActivateVisualStudio(dte);
		return didOpenFile;
	}

	bool TryOpenFileInRunningVisualStudio(const std::wstring& solutionPath, const std::wstring& sourceFilePath, int lineNumber)
	{
		IRunningObjectTable* runningObjectTable = nullptr;
		if (FAILED(GetRunningObjectTable(0, &runningObjectTable)))
		{
			return false;
		}

		IEnumMoniker* monikerEnumerator = nullptr;
		if (FAILED(runningObjectTable->EnumRunning(&monikerEnumerator)) || !monikerEnumerator)
		{
			runningObjectTable->Release();
			return false;
		}

		IBindCtx* bindContext = nullptr;
		CreateBindCtx(0, &bindContext);

		bool didOpenFile = false;
		IMoniker* moniker = nullptr;
		ULONG fetched = 0;
		while (!didOpenFile && monikerEnumerator->Next(1, &moniker, &fetched) == S_OK)
		{
			LPOLESTR displayName = nullptr;
			if (bindContext && SUCCEEDED(moniker->GetDisplayName(bindContext, nullptr, &displayName)) && displayName)
			{
				const bool isVisualStudio = std::wstring(displayName).find(L"VisualStudio.DTE") != std::wstring::npos;
				CoTaskMemFree(displayName);
				displayName = nullptr;

				if (isVisualStudio)
				{
					IUnknown* unknown = nullptr;
					if (SUCCEEDED(runningObjectTable->GetObject(moniker, &unknown)) && unknown)
					{
						IDispatch* dte = nullptr;
						if (SUCCEEDED(unknown->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&dte))) && dte)
						{
							const std::wstring runningSolutionPath = GetVisualStudioSolutionFullName(dte);
							if (!runningSolutionPath.empty() && AreSamePath(runningSolutionPath, solutionPath))
							{
								didOpenFile = OpenFileInVisualStudio(dte, sourceFilePath, lineNumber);
							}

							dte->Release();
						}
						unknown->Release();
					}
				}
			}

			moniker->Release();
			moniker = nullptr;
		}

		if (bindContext)
		{
			bindContext->Release();
		}
		monikerEnumerator->Release();
		runningObjectTable->Release();

		return didOpenFile;
	}

	bool OpenSolutionAndFileInNewVisualStudio(const std::wstring& solutionPath, const std::wstring& sourceFilePath, int lineNumber)
	{
		IDispatch* dte = CreateVisualStudioDte();
		if (!dte)
		{
			return false;
		}

		PutBoolProperty(dte, L"UserControl", true);
		ActivateVisualStudio(dte);

		bool didOpenSolution = false;
		IDispatch* solution = GetDispatchProperty(dte, L"Solution");
		if (solution)
		{
			didOpenSolution = InvokeMethodWithString(solution, L"Open", solutionPath);
			solution->Release();
		}

		const bool didOpenFile = didOpenSolution && OpenFileInVisualStudio(dte, sourceFilePath, lineNumber);
		dte->Release();
		return didOpenFile;
	}

	bool OpenSourceFileInVisualStudio(const std::string& filePath, int lineNumber)
	{
		const std::string solutionPath = FindProjectSolutionPath();
		if (solutionPath.empty() || !HasVisualStudioProgId())
		{
			return false;
		}

		ScopedComInitializer comInitializer;
		if (!comInitializer.IsUsable())
		{
			return false;
		}

		const std::wstring nativeSolutionPath = ToNativeWidePath(solutionPath);
		const std::wstring nativeFilePath = ToNativeWidePath(filePath);
		return TryOpenFileInRunningVisualStudio(nativeSolutionPath, nativeFilePath, lineNumber) ||
			OpenSolutionAndFileInNewVisualStudio(nativeSolutionPath, nativeFilePath, lineNumber);
	}

	std::wstring GetEnvironmentVariableWide(const wchar_t* name)
	{
		const DWORD requiredSize = GetEnvironmentVariableW(name, nullptr, 0);
		if (requiredSize == 0)
		{
			return L"";
		}

		std::wstring value(requiredSize, L'\0');
		GetEnvironmentVariableW(name, value.data(), requiredSize);
		if (!value.empty() && value.back() == L'\0')
		{
			value.pop_back();
		}
		return value;
	}

	bool TrySearchPathExecutable(const wchar_t* executableName, std::wstring& executablePath)
	{
		wchar_t buffer[MAX_PATH]{ 0 };
		const DWORD result = SearchPathW(nullptr, executableName, nullptr, MAX_PATH, buffer, nullptr);
		if (result == 0 || result >= MAX_PATH)
		{
			return false;
		}

		executablePath = buffer;
		return true;
	}

	void AddCandidateIfPresent(std::vector<std::wstring>& candidates, const std::wstring& path)
	{
		if (path.empty())
		{
			return;
		}

		std::error_code errorCode;
		if (std::filesystem::exists(std::filesystem::path(path), errorCode) && !errorCode)
		{
			candidates.push_back(path);
		}
	}

	std::wstring FindVisualStudioCodeExecutablePath()
	{
		std::wstring executablePath;
		if (TrySearchPathExecutable(L"code.cmd", executablePath) || TrySearchPathExecutable(L"code.exe", executablePath))
		{
			return executablePath;
		}

		std::vector<std::wstring> candidates;
		const std::wstring localAppData = GetEnvironmentVariableWide(L"LOCALAPPDATA");
		const std::wstring programFiles = GetEnvironmentVariableWide(L"ProgramFiles");
		const std::wstring programFilesX86 = GetEnvironmentVariableWide(L"ProgramFiles(x86)");

		AddCandidateIfPresent(candidates, localAppData + L"\\Programs\\Microsoft VS Code\\Code.exe");
		AddCandidateIfPresent(candidates, programFiles + L"\\Microsoft VS Code\\Code.exe");
		AddCandidateIfPresent(candidates, programFilesX86 + L"\\Microsoft VS Code\\Code.exe");
		AddCandidateIfPresent(candidates, localAppData + L"\\Programs\\Microsoft VS Code Insiders\\Code - Insiders.exe");
		AddCandidateIfPresent(candidates, programFiles + L"\\Microsoft VS Code Insiders\\Code - Insiders.exe");

		return candidates.empty() ? L"" : candidates.front();
	}

	bool OpenSourceFileInVisualStudioCode(const std::string& filePath, int lineNumber)
	{
		const std::wstring executablePath = FindVisualStudioCodeExecutablePath();
		if (executablePath.empty())
		{
			return false;
		}

		std::wstring sourcePathArgument = ToNativeWidePath(filePath);
		if (lineNumber > 0)
		{
			sourcePathArgument += L":" + std::to_wstring(lineNumber);
		}

		const std::wstring arguments = L"-r -g \"" + sourcePathArgument + L"\"";
		return reinterpret_cast<intptr_t>(ShellExecuteW(nullptr, L"open", executablePath.c_str(), arguments.c_str(), nullptr, SW_SHOWNORMAL)) > 32;
	}

	bool OpenWithSystemDefault(const std::string& filePath)
	{
		return reinterpret_cast<intptr_t>(ShellExecuteW(nullptr, L"open", ToNativeWidePath(filePath).c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
	}
#else
	bool IsCommandAvailable(const std::string& command)
	{
		const std::string checkCommand = "command -v " + command + " >/dev/null 2>&1";
		return std::system(checkCommand.c_str()) == 0;
	}

	std::string GetVisualStudioCodeCommand()
	{
		const char* candidates[] =
		{
			"code",
			"code-insiders",
			"codium"
		};

		for (const char* candidate : candidates)
		{
			if (IsCommandAvailable(candidate))
			{
				return candidate;
			}
		}

		return "";
	}

	bool OpenSourceFileInVisualStudio(const std::string& filePath, int lineNumber)
	{
		(void)filePath;
		(void)lineNumber;
		return false;
	}

	bool OpenSourceFileInVisualStudioCode(const std::string& filePath, int lineNumber)
	{
		const std::string commandName = GetVisualStudioCodeCommand();
		if (commandName.empty())
		{
			return false;
		}

		std::string sourcePathArgument = NormalizeFilesystemPath(filePath).generic_string();
		if (lineNumber > 0)
		{
			sourcePathArgument += ":" + std::to_string(lineNumber);
		}

		const std::string command = commandName + " -r -g " + QuoteShellPath(sourcePathArgument) + " >/dev/null 2>&1 &";
		return std::system(command.c_str()) == 0;
	}

	bool OpenWithSystemDefault(const std::string& filePath)
	{
#ifdef __APPLE__
		const std::string command = "open " + QuoteShellPath(NormalizeFilesystemPath(filePath).generic_string());
#else
		const std::string command = "xdg-open " + QuoteShellPath(NormalizeFilesystemPath(filePath).generic_string()) + " >/dev/null 2>&1 &";
#endif
		return std::system(command.c_str()) == 0;
	}
#endif

	bool TryOpenSourceFile(EditorSourceCodeUtils::SourceCodeEditor editor, const std::string& filePath, int lineNumber)
	{
		switch (editor)
		{
		case EditorSourceCodeUtils::SourceCodeEditor::VisualStudio:
			return OpenSourceFileInVisualStudio(filePath, lineNumber);
		case EditorSourceCodeUtils::SourceCodeEditor::VisualStudioCode:
			return OpenSourceFileInVisualStudioCode(filePath, lineNumber);
		case EditorSourceCodeUtils::SourceCodeEditor::SystemDefault:
			return OpenWithSystemDefault(filePath);
		case EditorSourceCodeUtils::SourceCodeEditor::Auto:
		default:
			return false;
		}
	}

	std::vector<EditorSourceCodeUtils::SourceCodeEditor> GetFallbackOrder(EditorSourceCodeUtils::SourceCodeEditor preferredEditor)
	{
		using SourceCodeEditor = EditorSourceCodeUtils::SourceCodeEditor;
		switch (preferredEditor)
		{
		case SourceCodeEditor::VisualStudio:
			return { SourceCodeEditor::VisualStudio, SourceCodeEditor::VisualStudioCode, SourceCodeEditor::SystemDefault };
		case SourceCodeEditor::VisualStudioCode:
			return { SourceCodeEditor::VisualStudioCode, SourceCodeEditor::VisualStudio, SourceCodeEditor::SystemDefault };
		case SourceCodeEditor::SystemDefault:
			return { SourceCodeEditor::SystemDefault };
		case SourceCodeEditor::Auto:
		default:
			return { SourceCodeEditor::VisualStudio, SourceCodeEditor::VisualStudioCode, SourceCodeEditor::SystemDefault };
		}
	}
}

std::vector<EditorSourceCodeUtils::SourceCodeEditorOption> EditorSourceCodeUtils::GetSourceCodeEditorOptions()
{
	return
	{
		{ SourceCodeEditor::Auto, "Auto", "Auto", true },
		{ SourceCodeEditor::VisualStudio, "Visual Studio", "VisualStudio", IsSourceCodeEditorAvailable(SourceCodeEditor::VisualStudio) },
		{ SourceCodeEditor::VisualStudioCode, "Visual Studio Code", "VisualStudioCode", IsSourceCodeEditorAvailable(SourceCodeEditor::VisualStudioCode) },
		{ SourceCodeEditor::SystemDefault, "System Default", "SystemDefault", true }
	};
}

EditorSourceCodeUtils::SourceCodeEditor EditorSourceCodeUtils::GetPreferredSourceCodeEditor()
{
	ConfigManager editorConfig;
	if (editorConfig.ReadFile(kEditorConfigPath))
	{
		return SourceCodeEditorFromConfigValue(editorConfig.GetString(kEditorConfigSection, kSourceCodeEditorKey, "Auto"));
	}

	return SourceCodeEditor::Auto;
}

bool EditorSourceCodeUtils::SetPreferredSourceCodeEditor(SourceCodeEditor editor)
{
	return SetConfigValue(kEditorConfigPath, kEditorConfigSection, kSourceCodeEditorKey, GetSourceCodeEditorConfigValue(editor));
}

bool EditorSourceCodeUtils::IsSourceCodeEditorAvailable(SourceCodeEditor editor)
{
	switch (editor)
	{
	case SourceCodeEditor::Auto:
	case SourceCodeEditor::SystemDefault:
		return true;
	case SourceCodeEditor::VisualStudio:
#ifdef GOKNAR_PLATFORM_WINDOWS
		return HasVisualStudioProgId() && !FindProjectSolutionPath().empty();
#else
		return false;
#endif
	case SourceCodeEditor::VisualStudioCode:
#ifdef GOKNAR_PLATFORM_WINDOWS
		return !FindVisualStudioCodeExecutablePath().empty();
#else
		return !GetVisualStudioCodeCommand().empty();
#endif
	default:
		return false;
	}
}

const char* EditorSourceCodeUtils::GetSourceCodeEditorLabel(SourceCodeEditor editor)
{
	switch (editor)
	{
	case SourceCodeEditor::Auto:
		return "Auto";
	case SourceCodeEditor::VisualStudio:
		return "Visual Studio";
	case SourceCodeEditor::VisualStudioCode:
		return "Visual Studio Code";
	case SourceCodeEditor::SystemDefault:
		return "System Default";
	default:
		return "Auto";
	}
}

const char* EditorSourceCodeUtils::GetSourceCodeEditorConfigValue(SourceCodeEditor editor)
{
	switch (editor)
	{
	case SourceCodeEditor::Auto:
		return "Auto";
	case SourceCodeEditor::VisualStudio:
		return "VisualStudio";
	case SourceCodeEditor::VisualStudioCode:
		return "VisualStudioCode";
	case SourceCodeEditor::SystemDefault:
		return "SystemDefault";
	default:
		return "Auto";
	}
}

EditorSourceCodeUtils::SourceCodeEditor EditorSourceCodeUtils::SourceCodeEditorFromConfigValue(const std::string& value)
{
	const std::string normalizedValue = ToLower(Trim(value));
	if (normalizedValue == "visualstudio" || normalizedValue == "visual studio")
	{
		return SourceCodeEditor::VisualStudio;
	}
	if (normalizedValue == "visualstudiocode" || normalizedValue == "visual studio code" || normalizedValue == "vscode" || normalizedValue == "vs code")
	{
		return SourceCodeEditor::VisualStudioCode;
	}
	if (normalizedValue == "systemdefault" || normalizedValue == "system default" || normalizedValue == "default")
	{
		return SourceCodeEditor::SystemDefault;
	}

	return SourceCodeEditor::Auto;
}

bool EditorSourceCodeUtils::OpenSourceFile(const std::string& filePath)
{
	return OpenSourceFile(filePath, 0);
}

bool EditorSourceCodeUtils::OpenSourceFile(const std::string& filePath, int lineNumber)
{
	for (SourceCodeEditor editor : GetFallbackOrder(GetPreferredSourceCodeEditor()))
	{
		if (TryOpenSourceFile(editor, filePath, lineNumber))
		{
			return true;
		}
	}

	return false;
}

void EditorSourceCodeUtils::ShowInFileExplorer(const std::string& path, bool selectPath)
{
#ifdef GOKNAR_PLATFORM_WINDOWS
	const std::wstring nativePath = ToNativeWidePath(path);
	if (nativePath.empty())
	{
		return;
	}

	if (selectPath)
	{
		const std::wstring parameters = L"/select,\"" + nativePath + L"\"";
		ShellExecuteW(nullptr, L"open", L"explorer.exe", parameters.c_str(), nullptr, SW_SHOWNORMAL);
	}
	else
	{
		ShellExecuteW(nullptr, L"open", nativePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}
#else
	const std::filesystem::path normalizedPath = NormalizeFilesystemPath(path);
#ifdef __APPLE__
	if (selectPath)
	{
		const std::string command = "open -R " + QuoteShellPath(normalizedPath.generic_string());
		std::system(command.c_str());
		return;
	}

	const std::string command = "open " + QuoteShellPath(normalizedPath.generic_string());
#else
	const std::filesystem::path targetPath = selectPath && !std::filesystem::is_directory(normalizedPath) ? normalizedPath.parent_path() : normalizedPath;
	const std::string command = "xdg-open " + QuoteShellPath(targetPath.generic_string()) + " >/dev/null 2>&1 &";
#endif
	std::system(command.c_str());
#endif
}
