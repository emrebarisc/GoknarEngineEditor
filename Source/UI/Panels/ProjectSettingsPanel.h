#pragma once

#include "EditorPanel.h"

#include <filesystem>
#include <string>
#include <vector>

class ProjectSettingsPanel : public IEditorPanel
{
public:
	ProjectSettingsPanel(EditorHUD* hud);

	virtual void Draw() override;

private:
	enum class SettingsView
	{
		Build = 0,
		Game
	};

	struct ConfigField
	{
		std::string key;
		std::string value;
		std::string linePrefix;
		size_t lineIndex{ 0 };
		std::vector<char> editBuffer{};
	};

	struct ConfigSection
	{
		std::string name;
		std::vector<ConfigField> fields{};
	};

	struct ConfigDocument
	{
		std::string filePath;
		std::string defaultSectionName;
		std::vector<std::string> lines{};
		std::vector<ConfigSection> sections{};
		std::string errorMessage;
		bool isLoaded{ false };
		bool hasLastWriteTime{ false };
		std::filesystem::file_time_type lastWriteTime{};
	};

	void EnsureConfigDocumentsLoaded();
	void LoadConfigDocuments();
	void DrawNavigation();
	void DrawSelectedSettings();
	void DrawConfigDocument(ConfigDocument& document);
	void DrawConfigSection(const std::string& configId, ConfigSection& section, ConfigDocument& document);
	void DrawConfigField(const std::string& configId, const std::string& sectionName, ConfigField& field, ConfigDocument& document);

	bool WriteFieldValue(ConfigDocument& document, ConfigField& field, const std::string& newValue);

	static std::string ResolveActiveProjectPath();
	static std::string NormalizePath(const std::string& path);
	static std::string EnsureTrailingSlash(const std::string& path);
	static std::string Trim(const std::string& value);
	static std::string GetProjectNameFromPath(const std::string& directoryPath);
	static std::string GetEngineLocationFromBuildConfig(const std::string& configPath);
	static std::string GetFallbackEngineLocation();
	static std::vector<char> CreateEditBuffer(const std::string& value);
	static bool TryGetLastWriteTime(const std::string& filePath, std::filesystem::file_time_type& lastWriteTime);
	static void EnsureBuildConfigExists(const std::string& projectRootPath, const std::string& buildConfigPath);
	static ConfigDocument LoadConfigDocument(const std::string& filePath, const std::string& defaultSectionName);

	std::string projectRootPath_{};
	std::string buildConfigPath_{};
	std::string gameConfigPath_{};
	ConfigDocument buildConfigDocument_{};
	ConfigDocument gameConfigDocument_{};
	SettingsView selectedSettingsView_{ SettingsView::Build };
	std::string statusMessage_{};
	bool statusMessageIsError_{ false };
};
