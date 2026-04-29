#include "ProjectSettingsPanel.h"

#include "imgui.h"

#include "Goknar/Managers/ConfigManager.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace
{
	constexpr size_t kProjectSettingsBufferSize = 1024;
}

ProjectSettingsPanel::ProjectSettingsPanel(EditorHUD* hud) :
	IEditorPanel("Project Settings", hud)
{
	isOpen_ = false;
}

void ProjectSettingsPanel::Draw()
{
	EnsureConfigDocumentsLoaded();

	ImGui::SetNextWindowSize(ImVec2(900.f, 540.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(title_.c_str(), &isOpen_))
	{
		ImGui::End();
		return;
	}

	position_ = Vector2i((int)ImGui::GetWindowPos().x, (int)ImGui::GetWindowPos().y);
	size_ = Vector2i((int)ImGui::GetWindowSize().x, (int)ImGui::GetWindowSize().y);

	if (!statusMessage_.empty())
	{
		const ImVec4 statusColor = statusMessageIsError_ ? ImVec4(0.95f, 0.35f, 0.35f, 1.f) : ImVec4(0.35f, 0.85f, 0.45f, 1.f);
		ImGui::TextColored(statusColor, "%s", statusMessage_.c_str());
		ImGui::Separator();
	}

	ImGui::BeginChild("ProjectSettingsNavigation", ImVec2(220.f, 0.f), true);
	DrawNavigation();
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("ProjectSettingsContent", ImVec2(0.f, 0.f), true);
	DrawSelectedSettings();
	ImGui::EndChild();

	ImGui::End();
}

void ProjectSettingsPanel::EnsureConfigDocumentsLoaded()
{
	const std::string resolvedProjectPath = ResolveActiveProjectPath();
	const std::string resolvedBuildConfigPath = resolvedProjectPath + "Config/Build.ini";
	const std::string resolvedGameConfigPath = resolvedProjectPath + "Config/GameConfig.ini";

	bool shouldReload = projectRootPath_ != resolvedProjectPath ||
		buildConfigPath_ != resolvedBuildConfigPath ||
		gameConfigPath_ != resolvedGameConfigPath ||
		!buildConfigDocument_.isLoaded ||
		!gameConfigDocument_.isLoaded;

	if (!shouldReload)
	{
		std::filesystem::file_time_type buildWriteTime{};
		if (TryGetLastWriteTime(buildConfigPath_, buildWriteTime))
		{
			if (!buildConfigDocument_.hasLastWriteTime || buildConfigDocument_.lastWriteTime != buildWriteTime)
			{
				shouldReload = true;
			}
		}

		std::filesystem::file_time_type gameWriteTime{};
		if (TryGetLastWriteTime(gameConfigPath_, gameWriteTime))
		{
			if (!gameConfigDocument_.hasLastWriteTime || gameConfigDocument_.lastWriteTime != gameWriteTime)
			{
				shouldReload = true;
			}
		}
	}

	if (!shouldReload)
	{
		return;
	}

	projectRootPath_ = resolvedProjectPath;
	buildConfigPath_ = resolvedBuildConfigPath;
	gameConfigPath_ = resolvedGameConfigPath;
	LoadConfigDocuments();
}

void ProjectSettingsPanel::LoadConfigDocuments()
{
	EnsureBuildConfigExists(projectRootPath_, buildConfigPath_);
	buildConfigDocument_ = LoadConfigDocument(buildConfigPath_, "Build");
	gameConfigDocument_ = LoadConfigDocument(gameConfigPath_, "Game");

	if (selectedSettingsView_ != SettingsView::Build && selectedSettingsView_ != SettingsView::Game)
	{
		selectedSettingsView_ = SettingsView::Build;
	}
}

void ProjectSettingsPanel::DrawNavigation()
{
	if (ImGui::Selectable("Build Settings", selectedSettingsView_ == SettingsView::Build))
	{
		selectedSettingsView_ = SettingsView::Build;
	}

	if (ImGui::Selectable("Game Settings", selectedSettingsView_ == SettingsView::Game))
	{
		selectedSettingsView_ = SettingsView::Game;
	}
}

void ProjectSettingsPanel::DrawSelectedSettings()
{
	ConfigDocument* document = selectedSettingsView_ == SettingsView::Build ? &buildConfigDocument_ : &gameConfigDocument_;
	DrawConfigDocument(*document);
}

void ProjectSettingsPanel::DrawConfigDocument(ConfigDocument& document)
{
	if (!document.errorMessage.empty())
	{
		ImGui::TextWrapped("%s", document.errorMessage.c_str());
		return;
	}

	if (document.sections.empty())
	{
		ImGui::TextWrapped("No editable settings were found in %s.", document.filePath.c_str());
		return;
	}

	ImGui::TextWrapped("%s", document.filePath.c_str());
	ImGui::Spacing();

	for (ConfigSection& section : document.sections)
	{
		DrawConfigSection(document.filePath, section, document);
	}
}

void ProjectSettingsPanel::DrawConfigSection(const std::string& configId, ConfigSection& section, ConfigDocument& document)
{
	ImGui::PushID((configId + section.name).c_str());
	ImGui::Text("%s", section.name.c_str());
	ImGui::Separator();

	if (ImGui::BeginTable("SettingsTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
	{
		for (ConfigField& field : section.fields)
		{
			DrawConfigField(configId, section.name, field, document);
		}

		ImGui::EndTable();
	}

	ImGui::Spacing();
	ImGui::PopID();
}

void ProjectSettingsPanel::DrawConfigField(const std::string& configId, const std::string& sectionName, ConfigField& field, ConfigDocument& document)
{
	ImGui::PushID((configId + sectionName + field.key).c_str());
	ImGui::TableNextRow();

	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%s", field.key.c_str());

	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputText("##Value", field.editBuffer.data(), field.editBuffer.size());
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		const std::string newValue = Trim(field.editBuffer.data());
		if (WriteFieldValue(document, field, newValue))
		{
			statusMessage_.clear();
			statusMessageIsError_ = false;
		}
		else
		{
			statusMessage_ = "Failed to update " + field.key + " in " + document.filePath + ".";
			statusMessageIsError_ = true;
			field.editBuffer = CreateEditBuffer(field.value);
		}
	}

	ImGui::PopID();
}

bool ProjectSettingsPanel::WriteFieldValue(ConfigDocument& document, ConfigField& field, const std::string& newValue)
{
	if (field.lineIndex >= document.lines.size())
	{
		return false;
	}

	const std::string previousLine = document.lines[field.lineIndex];
	document.lines[field.lineIndex] = field.linePrefix + newValue;

	std::ofstream configFile(document.filePath);
	if (!configFile.is_open())
	{
		document.lines[field.lineIndex] = previousLine;
		return false;
	}

	for (size_t lineIndex = 0; lineIndex < document.lines.size(); ++lineIndex)
	{
		configFile << document.lines[lineIndex];
		if (lineIndex + 1 < document.lines.size())
		{
			configFile << "\n";
		}
	}

	configFile.close();

	field.value = newValue;
	field.editBuffer = CreateEditBuffer(newValue);

	std::filesystem::file_time_type lastWriteTime{};
	if (TryGetLastWriteTime(document.filePath, lastWriteTime))
	{
		document.hasLastWriteTime = true;
		document.lastWriteTime = lastWriteTime;
	}

	return true;
}

std::string ProjectSettingsPanel::ResolveActiveProjectPath()
{
	ConfigManager editorConfig;
	if (editorConfig.ReadFile("Config/EditorConfig.ini"))
	{
		const std::string currentProjectPath = editorConfig.GetString("Editor", "CurrentProjectPath", "");
		if (!currentProjectPath.empty())
		{
			return EnsureTrailingSlash(currentProjectPath);
		}
	}

	return EnsureTrailingSlash(std::filesystem::current_path().string());
}

std::string ProjectSettingsPanel::NormalizePath(const std::string& path)
{
	std::string normalizedPath = path;
	for (char& character : normalizedPath)
	{
		if (character == '\\')
		{
			character = '/';
		}
	}

	return normalizedPath;
}

std::string ProjectSettingsPanel::EnsureTrailingSlash(const std::string& path)
{
	std::string normalizedPath = NormalizePath(path);
	if (!normalizedPath.empty() && normalizedPath.back() != '/')
	{
		normalizedPath += '/';
	}

	return normalizedPath;
}

std::string ProjectSettingsPanel::Trim(const std::string& value)
{
	const size_t firstNonWhitespace = value.find_first_not_of(" \t\r\n");
	if (firstNonWhitespace == std::string::npos)
	{
		return "";
	}

	const size_t lastNonWhitespace = value.find_last_not_of(" \t\r\n");
	return value.substr(firstNonWhitespace, lastNonWhitespace - firstNonWhitespace + 1);
}

std::string ProjectSettingsPanel::GetProjectNameFromPath(const std::string& directoryPath)
{
	std::filesystem::path projectPath(directoryPath);

	std::string projectName = projectPath.filename().string();
	if (projectName.empty())
	{
		projectName = projectPath.parent_path().filename().string();
	}

	return projectName;
}

std::string ProjectSettingsPanel::GetEngineLocationFromBuildConfig(const std::string& configPath)
{
	std::ifstream buildConfig(configPath);
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
		if (key != "EngineLocation")
		{
			continue;
		}

		return NormalizePath(Trim(line.substr(equalsIndex + 1)));
	}

	return "";
}

std::string ProjectSettingsPanel::GetFallbackEngineLocation()
{
	const std::filesystem::path currentPath = std::filesystem::current_path();
	const std::filesystem::path candidatePaths[] =
	{
		currentPath / "GoknarEngine" / "Goknar",
		currentPath / ".." / "GoknarEngine" / "Goknar",
		currentPath / ".." / ".." / "GoknarEngine" / "Goknar"
	};

	for (const std::filesystem::path& candidatePath : candidatePaths)
	{
		std::error_code errorCode;
		const std::filesystem::path absolutePath = std::filesystem::weakly_canonical(candidatePath, errorCode);
		if (!errorCode && std::filesystem::exists(absolutePath))
		{
			return NormalizePath(absolutePath.string());
		}
	}

	return "";
}

std::vector<char> ProjectSettingsPanel::CreateEditBuffer(const std::string& value)
{
	std::vector<char> buffer(kProjectSettingsBufferSize, '\0');
	const size_t copyLength = std::min(value.size(), kProjectSettingsBufferSize - 1);
	if (copyLength > 0)
	{
		std::memcpy(buffer.data(), value.c_str(), copyLength);
	}

	buffer[copyLength] = '\0';
	return buffer;
}

bool ProjectSettingsPanel::TryGetLastWriteTime(const std::string& filePath, std::filesystem::file_time_type& lastWriteTime)
{
	std::error_code errorCode;
	const std::filesystem::path path(filePath);
	if (!std::filesystem::exists(path, errorCode) || errorCode)
	{
		return false;
	}

	lastWriteTime = std::filesystem::last_write_time(path, errorCode);
	return !errorCode;
}

void ProjectSettingsPanel::EnsureBuildConfigExists(const std::string& projectRootPath, const std::string& buildConfigPath)
{
	std::error_code errorCode;
	if (std::filesystem::exists(buildConfigPath, errorCode) && !errorCode)
	{
		return;
	}

	std::filesystem::create_directories(std::filesystem::path(buildConfigPath).parent_path(), errorCode);

	std::string engineLocation = GetEngineLocationFromBuildConfig("Config/Build.ini");
	if (engineLocation.empty())
	{
		engineLocation = GetFallbackEngineLocation();
	}

	std::ofstream buildConfig(buildConfigPath);
	if (!buildConfig.is_open())
	{
		return;
	}

	const std::string projectName = GetProjectNameFromPath(projectRootPath);
	if (!projectName.empty())
	{
		buildConfig << "ProjectName=" << projectName << "\n";
	}

	if (!engineLocation.empty())
	{
		buildConfig << "EngineLocation=" << NormalizePath(engineLocation) << "\n";
	}
}

ProjectSettingsPanel::ConfigDocument ProjectSettingsPanel::LoadConfigDocument(const std::string& filePath, const std::string& defaultSectionName)
{
	ConfigDocument document;
	document.filePath = filePath;
	document.defaultSectionName = defaultSectionName;

	std::ifstream configFile(filePath);
	if (!configFile.is_open())
	{
		document.errorMessage = "Failed to open " + filePath + ".";
		return document;
	}

	std::unordered_map<std::string, size_t> sectionIndexMap;
	auto ensureSection = [&document, &sectionIndexMap](const std::string& sectionName) -> ConfigSection&
	{
		const auto sectionIndexIterator = sectionIndexMap.find(sectionName);
		if (sectionIndexIterator != sectionIndexMap.end())
		{
			return document.sections[sectionIndexIterator->second];
		}

		document.sections.push_back({ sectionName, {} });
		sectionIndexMap[sectionName] = document.sections.size() - 1;
		return document.sections.back();
	};

	std::string currentSectionName = defaultSectionName;
	std::string line;
	while (std::getline(configFile, line))
	{
		document.lines.push_back(line);

		const std::string trimmedLine = Trim(line);
		if (trimmedLine.empty() || trimmedLine[0] == ';' || trimmedLine[0] == '#')
		{
			continue;
		}

		if (trimmedLine.front() == '[' && trimmedLine.back() == ']')
		{
			currentSectionName = Trim(trimmedLine.substr(1, trimmedLine.size() - 2));
			ensureSection(currentSectionName);
			continue;
		}

		const size_t equalsIndex = line.find('=');
		if (equalsIndex == std::string::npos)
		{
			continue;
		}

		const std::string key = Trim(line.substr(0, equalsIndex));
		if (key.empty())
		{
			continue;
		}

		size_t valueStartIndex = equalsIndex + 1;
		while (valueStartIndex < line.size() && std::isspace(static_cast<unsigned char>(line[valueStartIndex])))
		{
			++valueStartIndex;
		}

		ConfigField field;
		field.key = key;
		field.value = Trim(line.substr(equalsIndex + 1));
		field.linePrefix = line.substr(0, valueStartIndex);
		field.lineIndex = document.lines.size() - 1;
		field.editBuffer = CreateEditBuffer(field.value);

		ensureSection(currentSectionName).fields.push_back(std::move(field));
	}

	configFile.close();
	document.isLoaded = true;

	std::filesystem::file_time_type lastWriteTime{};
	if (TryGetLastWriteTime(filePath, lastWriteTime))
	{
		document.hasLastWriteTime = true;
		document.lastWriteTime = lastWriteTime;
	}

	return document;
}
