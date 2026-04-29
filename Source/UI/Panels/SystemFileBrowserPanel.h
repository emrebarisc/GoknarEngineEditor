#pragma once

#include "EditorPanel.h"

#include "Goknar/Delegates/Delegate.h"
#include <string>

enum class FileBrowserMode
{
	OpenDirectory,
	OpenFile,
	SaveFile,
	CreateProject
};

class SystemFileBrowserPanel : public IEditorPanel
{
public:
	SystemFileBrowserPanel(EditorHUD* hud);

	virtual void Draw() override;

	void OpenDirectorySelector(const Delegate<void(const std::string&)>& callback)
	{
		ClearBrowsingRootPath();
		mode_ = FileBrowserMode::OpenDirectory;
		onSelectionCallback_ = callback;
		SetIsOpen(true);
	}

	void OpenFileSelector(const Delegate<void(const std::string&)>& callback)
	{
		ClearBrowsingRootPath();
		selectedItem_.clear();
		mode_ = FileBrowserMode::OpenFile;
		onSelectionCallback_ = callback;
		SetIsOpen(true);
	}

	void OpenFileSelector(const Delegate<void(const std::string&)>& callback, const std::string& initialDirectory)
	{
		ClearBrowsingRootPath();
		SetCurrentPath(initialDirectory);
		OpenFileSelector(callback);
	}

	void OpenFileSelector(const Delegate<void(const std::string&)>& callback, const std::string& initialDirectory, const std::string& browsingRootPath)
	{
		SetBrowsingRootPath(browsingRootPath);
		SetCurrentPath(initialDirectory);
		selectedItem_.clear();
		mode_ = FileBrowserMode::OpenFile;
		onSelectionCallback_ = callback;
		SetIsOpen(true);
	}

	void SaveFileSelector(const Delegate<void(const std::string&)>& callback)
	{
		ClearBrowsingRootPath();
		selectedItem_.clear();
		mode_ = FileBrowserMode::SaveFile;
		onSelectionCallback_ = callback;
		saveFileName_ = "";
		SetIsOpen(true);
	}

	void SaveFileSelector(const Delegate<void(const std::string&)>& callback, const std::string& initialDirectory, const std::string& suggestedFileName = "")
	{
		ClearBrowsingRootPath();
		SetCurrentPath(initialDirectory);
		selectedItem_.clear();
		mode_ = FileBrowserMode::SaveFile;
		onSelectionCallback_ = callback;
		saveFileName_ = suggestedFileName;
		SetIsOpen(true);
	}

	void SaveFileSelector(const Delegate<void(const std::string&)>& callback, const std::string& initialDirectory, const std::string& suggestedFileName, const std::string& browsingRootPath)
	{
		SetBrowsingRootPath(browsingRootPath);
		SetCurrentPath(initialDirectory);
		selectedItem_.clear();
		mode_ = FileBrowserMode::SaveFile;
		onSelectionCallback_ = callback;
		saveFileName_ = suggestedFileName;
		SetIsOpen(true);
	}

	void CreateProjectSelector(const Delegate<void(const std::string&, const std::string&)>& callback)
	{
		ClearBrowsingRootPath();
		mode_ = FileBrowserMode::CreateProject;
		onProjectSelectionCallback_ = callback;
		saveFileName_ = "";
		SetIsOpen(true);
	}

	void SetOnDirectorySelectedCallback(const Delegate<void(const std::string&)>& callback)
	{
		ClearBrowsingRootPath();
		mode_ = FileBrowserMode::OpenDirectory;
		onSelectionCallback_ = callback;
	}

	void SetCurrentPath(const std::string& path);
	void SetBrowsingRootPath(const std::string& path);
	void ClearBrowsingRootPath();

private:
	std::string currentPath_;
	std::string selectedItem_;
	std::string saveFileName_;
	std::string browsingRootPath_;

	FileBrowserMode mode_{ FileBrowserMode::OpenDirectory };

	Delegate<void(const std::string&)> onSelectionCallback_;
	Delegate<void(const std::string&, const std::string&)> onProjectSelectionCallback_;
};
