#pragma once

#include "EditorPanel.h"
#include "UI/EditorContext.h"
#include <string>

enum class PendingAssetCreationType
{
	None = 0,
	Material,
	MaterialFunction,
	AnimationGraph
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
	void OpenAssetFile(const std::string& filePath);

	// File system operations
	void MoveFileSystemItem(const std::string& source, const std::string& targetFolder);
	void RefreshAndRestore();
	void FinalizeFolderCreation();
	void FinalizeAssetCreation();

	Folder* currentFolder_{ nullptr };
	float thumbnailSize_{ 48.0f };

	// Operation states
	bool isRenaming_{ false };
	bool isCreatingFolder_{ false };
	bool isCreatingAsset_{ false };
	bool needsRefresh_{ false };
	std::string selectedItemForMenu_{ "" };
	std::string pendingCreationDirectory_{};
	PendingAssetCreationType pendingAssetCreationType_{ PendingAssetCreationType::None };
	char renameBuffer_[256]{ 0 };
	char creationNameBuffer_[256]{ 0 };
};
