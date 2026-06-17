#pragma once

#include "EditorPanel.h"
#include "UI/EditorContext.h"
#include <future>
#include <string>

enum class PendingAssetCreationType
{
	None = 0,
	Material,
	MaterialFunction,
	AnimationGraph,
	Scene
};

class FileBrowserPanel : public IEditorPanel
{
public:
	FileBrowserPanel(EditorHUD* hud);

	virtual void Draw() override;

private:
	void DrawGrid();
	void DrawFolderTree(Folder* folder);

	// Context menu & drag-drop helpers
	void HandleDragDropSource(const std::string& sourcePath, const std::string& payloadName);
	void HandleDragDropTarget(const std::string& targetPath);
	void HandleContextMenu(const std::string& itemPath, const std::string& itemName, bool isFolder);
	void DrawCreateContentMenu(const std::string& targetDirectory);
	void OpenClassCreationPanel(const std::string& initialDirectory);
	void OpenAssetFile(const std::string& filePath);
	void RequestOpenScene(const std::string& filePath);
	void DrawSaveChangesPrompt();
	void OpenPendingScene();
	bool SaveCurrentScene();
	void SetCurrentFolder(Folder* folder);
	void RestoreCurrentFolderFromPath();
	bool IsKnownFolder(Folder* folder) const;
	void SynchronizeCurrentFolder();
	void UpdateCMakeRebuildResult();
	void RebuildCMakeFiles();

	// File system operations
	void DuplicateFileSystemItem(const std::string& source);
	void MoveFileSystemItem(const std::string& source, const std::string& targetFolder);
	void RefreshAndRestore();
	void FinalizeFolderCreation();
	void FinalizeAssetCreation();

	Folder* currentFolder_{ nullptr };
	std::string currentFolderPath_{};
	unsigned int observedFileTreeVersion_{ 0 };
	std::future<void> asyncCMakeRebuildResult_;
	float thumbnailSize_{ 48.0f };

	// Operation states
	bool isRenaming_{ false };
	bool isCreatingFolder_{ false };
	bool isCreatingAsset_{ false };
	bool needsRefresh_{ false };
	bool shouldOpenSaveChangesPopup_{ false };
	bool shouldFocusCreationNameInput_{ false };
	std::string selectedItemForMenu_{ "" };
	std::string pendingSceneToOpen_{ "" };
	std::string pendingCreationDirectory_{};
	PendingAssetCreationType pendingAssetCreationType_{ PendingAssetCreationType::None };
	char renameBuffer_[256]{ 0 };
	char creationNameBuffer_[256]{ 0 };
};
