#include "ClassCreationPanel.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Goknar/Engine.h"
#include "Goknar/Factories/DynamicObjectFactory.h"

#include "UI/EditorAssetPathUtils.h"
#include "UI/EditorContext.h"
#include "UI/EditorGameClassRegistration.h"
#include "UI/EditorGameProjectBuildUtils.h"
#include "UI/EditorHUD.h"
#include "UI/Panels/SystemFileBrowserPanel.h"

namespace
{
	const ClassCreationPanel::BaseClassOption kBuiltInBaseClassOptions[] =
	{
		{ "ObjectBase", "Goknar/ObjectBase.h", false },
		{ "PhysicsObject", "Goknar/Physics/PhysicsObject.h", false },
		{ "OverlappingPhysicsObject", "Goknar/Physics/OverlappingPhysicsObject.h", false },
		{ "RigidBody", "Goknar/Physics/RigidBody.h", false },
		{ "Character", "Goknar/Physics/Character.h", false },
		{ "HUD", "Goknar/UI/HUD.h", false },
		{ "Component", "Goknar/Components/Component.h", true },
		{ "PhysicsMovementComponent", "Goknar/Physics/Components/PhysicsMovementComponent.h", true }
	};

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

	std::vector<std::string> ReadLines(const std::string& filePath)
	{
		std::vector<std::string> lines;
		std::ifstream file(filePath);
		std::string line;
		while (std::getline(file, line))
		{
			lines.push_back(line);
		}
		return lines;
	}

	bool WriteLines(const std::string& filePath, const std::vector<std::string>& lines)
	{
		std::ofstream file(filePath);
		if (!file.is_open())
		{
			return false;
		}

		for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
		{
			file << lines[lineIndex];
			if (lineIndex + 1 < lines.size())
			{
				file << "\n";
			}
		}

		return true;
	}

}

ClassCreationPanel::ClassCreationPanel(EditorHUD* hud) :
	IEditorPanel("Class Creation", hud)
{
	isOpen_ = false;
	selectedDirectory_ = GetSourceRootPath();
}

void ClassCreationPanel::Open(const std::string& initialDirectory)
{
	const std::string sourceRoot = GetSourceRootPath();
	std::string normalizedInitialDirectory = NormalizePath(initialDirectory);
	if (normalizedInitialDirectory.empty())
	{
		normalizedInitialDirectory = sourceRoot;
	}

	if (!normalizedInitialDirectory.empty() && std::filesystem::exists(normalizedInitialDirectory))
	{
		std::error_code errorCode;
		if (!std::filesystem::is_directory(normalizedInitialDirectory, errorCode))
		{
			normalizedInitialDirectory = NormalizePath(std::filesystem::path(normalizedInitialDirectory).parent_path().generic_string());
		}
	}

	if (normalizedInitialDirectory.empty() || normalizedInitialDirectory.rfind(sourceRoot, 0) != 0)
	{
		normalizedInitialDirectory = sourceRoot;
	}

	selectedDirectory_ = normalizedInitialDirectory;
	classNameBuffer_[0] = '\0';
	statusMessage_.clear();
	hasError_ = false;
	SetIsOpen(true);
}

void ClassCreationPanel::Draw()
{
	if (!isOpen_)
	{
		return;
	}

	if (!ImGui::Begin(title_.c_str(), &isOpen_))
	{
		ImGui::End();
		return;
	}

	ImGui::InputText("Class Name", classNameBuffer_, sizeof(classNameBuffer_));

	const std::vector<BaseClassOption> baseClassOptions = BuildBaseClassOptions();
	if (selectedBaseClassIndex_ < 0 || selectedBaseClassIndex_ >= static_cast<int>(baseClassOptions.size()))
	{
		selectedBaseClassIndex_ = 0;
	}

	const char* selectedBaseClassName = baseClassOptions.empty() ? "" : baseClassOptions[selectedBaseClassIndex_].name.c_str();
	if (ImGui::BeginCombo("Base Class", selectedBaseClassName))
	{
		for (int optionIndex = 0; optionIndex < static_cast<int>(baseClassOptions.size()); ++optionIndex)
		{
			const bool isSelected = optionIndex == selectedBaseClassIndex_;
			if (ImGui::Selectable(baseClassOptions[optionIndex].name.c_str(), isSelected))
			{
				selectedBaseClassIndex_ = optionIndex;
			}

			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::TextWrapped("Location: %s", selectedDirectory_.empty() ? GetSourceRootPath().c_str() : selectedDirectory_.c_str());
	if (ImGui::Button("Select Location"))
	{
		OpenLocationSelector();
	}

	if (!statusMessage_.empty())
	{
		ImGui::TextColored(hasError_ ? ImVec4(1.f, 0.25f, 0.2f, 1.f) : ImVec4(0.3f, 0.85f, 0.35f, 1.f), "%s", statusMessage_.c_str());
	}

	ImGui::Separator();

	if (ImGui::Button("Create", ImVec2(120, 0)))
	{
		CreateClass();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(120, 0)))
	{
		SetIsOpen(false);
	}

	ImGui::End();
}

void ClassCreationPanel::OpenLocationSelector()
{
	SystemFileBrowserPanel* systemFileBrowserPanel = hud_ ? hud_->GetPanel<SystemFileBrowserPanel>() : nullptr;
	if (!systemFileBrowserPanel)
	{
		statusMessage_ = "System file browser panel is not available.";
		hasError_ = true;
		return;
	}

	const std::string sourceRoot = GetSourceRootPath();
	systemFileBrowserPanel->OpenDirectorySelector(
		Delegate<void(const std::string&)>::Create<ClassCreationPanel, &ClassCreationPanel::OnLocationSelected>(this),
		selectedDirectory_.empty() ? sourceRoot : selectedDirectory_,
		sourceRoot);
	hud_->ShowPanel<SystemFileBrowserPanel>();
}

void ClassCreationPanel::OnLocationSelected(const std::string& directoryPath)
{
	selectedDirectory_ = EnsureTrailingSlash(NormalizePath(directoryPath));
	statusMessage_.clear();
	hasError_ = false;
}

void ClassCreationPanel::CreateClass()
{
	std::string errorMessage;
	const std::string className = Trim(classNameBuffer_);
	if (!ValidateClassName(className, errorMessage) || !IsSelectedLocationValid(errorMessage))
	{
		statusMessage_ = errorMessage;
		hasError_ = true;
		return;
	}

	std::vector<BaseClassOption> baseClassOptions = BuildBaseClassOptions();
	if (baseClassOptions.empty())
	{
		statusMessage_ = "No base classes are available.";
		hasError_ = true;
		return;
	}

	if (selectedBaseClassIndex_ < 0 || selectedBaseClassIndex_ >= static_cast<int>(baseClassOptions.size()))
	{
		selectedBaseClassIndex_ = 0;
	}

	const BaseClassOption baseClassOption = baseClassOptions[selectedBaseClassIndex_];
	if (!WriteClassFiles(className, baseClassOption, errorMessage))
	{
		statusMessage_ = errorMessage;
		hasError_ = true;
		return;
	}

	if (!EnsureCMakeSubdirectory(selectedDirectory_, errorMessage))
	{
		statusMessage_ = errorMessage;
		hasError_ = true;
		return;
	}

	EditorContext::Get()->BuildFileTree();

	statusMessage_ = className + " created. Rebuilding the editor to compile and register it.";
	hasError_ = false;
	classNameBuffer_[0] = '\0';

	if (!EditorGameProjectBuildUtils::RestartEditor(true))
	{
		statusMessage_ = className + " created, but the editor rebuild could not be started.";
		hasError_ = true;
		return;
	}

	engine->Exit();
}

bool ClassCreationPanel::ValidateClassName(const std::string& className, std::string& outError) const
{
	if (className.empty())
	{
		outError = "Class name cannot be empty.";
		return false;
	}

	const auto isIdentifierStart = [](char character)
	{
		return std::isalpha(static_cast<unsigned char>(character)) || character == '_';
	};

	const auto isIdentifierCharacter = [](char character)
	{
		return std::isalnum(static_cast<unsigned char>(character)) || character == '_';
	};

	if (!isIdentifierStart(className.front()))
	{
		outError = "Class name must start with a letter or underscore.";
		return false;
	}

	for (char character : className)
	{
		if (!isIdentifierCharacter(character))
		{
			outError = "Class name can only contain letters, numbers, and underscores.";
			return false;
		}
	}

	static const std::unordered_set<std::string> reservedKeywords =
	{
		"class", "struct", "template", "typename", "namespace", "public", "private", "protected",
		"virtual", "override", "final", "const", "static", "operator", "new", "delete"
	};

	if (reservedKeywords.find(className) != reservedKeywords.end())
	{
		outError = "Class name cannot be a C++ keyword.";
		return false;
	}

	DynamicObjectFactory* dynamicObjectFactory = DynamicObjectFactory::GetInstance();
	if (dynamicObjectFactory->GetObjectMap().find(className) != dynamicObjectFactory->GetObjectMap().end() ||
		dynamicObjectFactory->GetComponentMap().find(className) != dynamicObjectFactory->GetComponentMap().end())
	{
		outError = "A dynamic class with that name is already registered.";
		return false;
	}

	return true;
}

bool ClassCreationPanel::IsSelectedLocationValid(std::string& outError) const
{
	const std::string sourceRoot = GetSourceRootPath();
	const std::string normalizedDirectory = EnsureTrailingSlash(NormalizePath(selectedDirectory_));
	if (normalizedDirectory.empty() || normalizedDirectory.rfind(sourceRoot, 0) != 0)
	{
		outError = "Class location must be inside the project Source directory.";
		return false;
	}

	std::error_code errorCode;
	if (!std::filesystem::exists(normalizedDirectory, errorCode) || !std::filesystem::is_directory(normalizedDirectory, errorCode))
	{
		outError = "Selected class location does not exist.";
		return false;
	}

	return true;
}

std::vector<ClassCreationPanel::BaseClassOption> ClassCreationPanel::BuildBaseClassOptions() const
{
	std::vector<BaseClassOption> baseClassOptions;
	std::unordered_set<std::string> registeredBaseClassNames;

	for (const BaseClassOption& baseClassOption : kBuiltInBaseClassOptions)
	{
		baseClassOptions.push_back(baseClassOption);
		registeredBaseClassNames.insert(baseClassOption.name);
	}

	for (const EditorGameClassRegistration::ClassMetadata& classMetadata : EditorGameClassRegistration::GetRegisteredClassMetadata())
	{
		if (classMetadata.className.empty() || classMetadata.includePath.empty())
		{
			continue;
		}

		if (!registeredBaseClassNames.insert(classMetadata.className).second)
		{
			continue;
		}

		baseClassOptions.push_back({ classMetadata.className, classMetadata.includePath, classMetadata.isComponent });
	}

	return baseClassOptions;
}

bool ClassCreationPanel::WriteClassFiles(const std::string& className, const BaseClassOption& baseClassOption, std::string& outError)
{
	const std::filesystem::path classDirectory = std::filesystem::path(selectedDirectory_);
	const std::filesystem::path headerPath = classDirectory / (className + ".h");
	const std::filesystem::path sourcePath = classDirectory / (className + ".cpp");

	if (std::filesystem::exists(headerPath) || std::filesystem::exists(sourcePath))
	{
		outError = "A header or source file with that class name already exists in the selected location.";
		return false;
	}

	std::ofstream headerFile(headerPath);
	if (!headerFile.is_open())
	{
		outError = "Failed to create header file.";
		return false;
	}

	headerFile << "#pragma once\n\n";
	headerFile << "#include \"" << baseClassOption.includePath << "\"\n\n";
	if (baseClassOption.isComponent && baseClassOption.includePath != "Goknar/Components/Component.h")
	{
		headerFile << "#include \"Goknar/Components/Component.h\"\n\n";
	}
	headerFile << "class " << className << " : public " << baseClassOption.name << "\n";
	headerFile << "{\n";
	headerFile << "public:\n";
	if (baseClassOption.isComponent)
	{
		headerFile << "\t" << className << "(Component* parent);\n";
	}
	else
	{
		headerFile << "\t" << className << "();\n";
	}
	headerFile << "\tvirtual ~" << className << "();\n\n";
	headerFile << "\tvirtual void BeginGame() override;\n";
	if (baseClassOption.isComponent)
	{
		headerFile << "\tvirtual void TickComponent(float deltaTime) override;\n";
	}
	else if (baseClassOption.name == "HUD")
	{
		headerFile << "\tvirtual void UpdateHUD() override;\n";
	}
	else
	{
		headerFile << "\tvirtual void Tick(float deltaTime) override;\n";
	}
	headerFile << "};\n";
	headerFile.close();

	std::ofstream sourceFile(sourcePath);
	if (!sourceFile.is_open())
	{
		std::error_code errorCode;
		std::filesystem::remove(headerPath, errorCode);
		outError = "Failed to create source file.";
		return false;
	}

	sourceFile << "#include \"" << className << ".h\"\n\n";
	if (baseClassOption.isComponent)
	{
		sourceFile << className << "::" << className << "(Component* parent) :\n";
		sourceFile << "\t" << baseClassOption.name << "(parent)\n";
	}
	else
	{
		sourceFile << className << "::" << className << "() :\n";
		sourceFile << "\t" << baseClassOption.name << "()\n";
	}
	sourceFile << "{\n";
	sourceFile << "}\n";
	sourceFile << "\n";
	sourceFile << className << "::~" << className << "()\n";
	sourceFile << "{\n";
	sourceFile << "}\n";
	sourceFile << "\n";
	sourceFile << "void " << className << "::BeginGame()\n";
	sourceFile << "{\n";
	sourceFile << "\t" << baseClassOption.name << "::BeginGame();\n";
	sourceFile << "}\n";
	sourceFile << "\n";
	if (baseClassOption.isComponent)
	{
		sourceFile << "void " << className << "::TickComponent(float deltaTime)\n";
		sourceFile << "{\n";
		sourceFile << "\t" << baseClassOption.name << "::TickComponent(deltaTime);\n";
		sourceFile << "}\n";
	}
	else if (baseClassOption.name == "HUD")
	{
		sourceFile << "void " << className << "::UpdateHUD()\n";
		sourceFile << "{\n";
		sourceFile << "\t" << baseClassOption.name << "::UpdateHUD();\n";
		sourceFile << "}\n";
	}
	else
	{
		sourceFile << "void " << className << "::Tick(float deltaTime)\n";
		sourceFile << "{\n";
		sourceFile << "\t" << baseClassOption.name << "::Tick(deltaTime);\n";
		sourceFile << "}\n";
	}

	return true;
}

bool ClassCreationPanel::EnsureCMakeSubdirectory(const std::string& classDirectory, std::string& outError)
{
	const std::string projectRoot = GetProjectRootPath();
	const std::string cmakeListsPath = NormalizePath((std::filesystem::path(projectRoot) / "CMakeLists.txt").generic_string());
	if (!std::filesystem::exists(cmakeListsPath))
	{
		outError = "Project CMakeLists.txt could not be found.";
		return false;
	}

	const std::filesystem::path sourceRootPath = std::filesystem::path(GetSourceRootPath());
	const std::filesystem::path classDirectoryPath = std::filesystem::path(classDirectory).lexically_normal();
	std::string relativeToSource = classDirectoryPath.lexically_relative(sourceRootPath).generic_string();
	if (relativeToSource == ".")
	{
		relativeToSource.clear();
	}

	const std::string cmakeSubdirectory = relativeToSource.empty() ? "${SOURCE_DIR_NAME}" : "${SOURCE_DIR_NAME}/" + relativeToSource;
	std::vector<std::string> lines = ReadLines(cmakeListsPath);
	for (const std::string& line : lines)
	{
		if (line.find(cmakeSubdirectory) != std::string::npos)
		{
			return true;
		}
	}

	size_t subdirsStartIndex = std::string::npos;
	for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
	{
		if (Trim(lines[lineIndex]).rfind("set(SUBDIRS", 0) == 0)
		{
			subdirsStartIndex = lineIndex;
			break;
		}
	}

	if (subdirsStartIndex == std::string::npos)
	{
		outError = "Could not find SUBDIRS in project CMakeLists.txt.";
		return false;
	}

	size_t insertIndex = std::string::npos;
	for (size_t lineIndex = subdirsStartIndex + 1; lineIndex < lines.size(); ++lineIndex)
	{
		if (Trim(lines[lineIndex]) == ")")
		{
			insertIndex = lineIndex;
			break;
		}
	}

	if (insertIndex == std::string::npos)
	{
		outError = "Could not find the end of SUBDIRS in project CMakeLists.txt.";
		return false;
	}

	lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insertIndex), "    " + cmakeSubdirectory);
	if (!WriteLines(cmakeListsPath, lines))
	{
		outError = "Failed to update project CMakeLists.txt.";
		return false;
	}

	return true;
}

std::string ClassCreationPanel::GetProjectRootPath() const
{
	return EnsureTrailingSlash(NormalizePath(EditorAssetPathUtils::GetProjectRootPath()));
}

std::string ClassCreationPanel::GetSourceRootPath() const
{
	return EnsureTrailingSlash(NormalizePath((std::filesystem::path(GetProjectRootPath()) / "Source").generic_string()));
}

std::string ClassCreationPanel::NormalizePath(const std::string& path) const
{
	if (path.empty())
	{
		return "";
	}

	return std::filesystem::path(path).lexically_normal().generic_string();
}

std::string ClassCreationPanel::EnsureTrailingSlash(const std::string& path) const
{
	if (path.empty() || path.back() == '/')
	{
		return path;
	}

	return path + "/";
}
