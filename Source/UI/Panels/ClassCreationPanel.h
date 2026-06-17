#pragma once

#include "EditorPanel.h"

#include <future>
#include <string>

class ClassCreationPanel : public IEditorPanel
{
public:
	ClassCreationPanel(EditorHUD* hud);

	void Draw() override;
	void Open(const std::string& initialDirectory = "");

	struct BaseClassOption
	{
		const char* name;
		const char* includePath;
		bool isComponent;
	};

private:
	void OpenLocationSelector();
	void OnLocationSelected(const std::string& directoryPath);
	void CreateClass();

	bool ValidateClassName(const std::string& className, std::string& outError) const;
	bool IsSelectedLocationValid(std::string& outError) const;
	bool WriteClassFiles(const std::string& className, const BaseClassOption& baseClassOption, std::string& outError);
	bool EnsureCMakeSubdirectory(const std::string& classDirectory, std::string& outError);
	void RebuildCMakeFiles();
	void RegisterCreatedClass(const std::string& className, const BaseClassOption& baseClassOption);

	std::string GetProjectRootPath() const;
	std::string GetSourceRootPath() const;
	std::string NormalizePath(const std::string& path) const;
	std::string EnsureTrailingSlash(const std::string& path) const;
	std::string QuoteForShell(const std::string& path) const;

	char classNameBuffer_[128]{ 0 };
	std::string selectedDirectory_;
	std::string statusMessage_;
	bool hasError_{ false };
	int selectedBaseClassIndex_{ 0 };
	std::future<void> asyncCMakeRebuildResult_;
};
