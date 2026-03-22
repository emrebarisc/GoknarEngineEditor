#pragma once

#include "EditorPanel.h"
#include "UI/EditorContext.h"
#include <string>

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

	// File system operations
	void MoveFileSystemItem(const std::string& source, const std::string& targetFolder);
	void RefreshAndRestore();

	Folder* currentFolder_{ nullptr };
	float thumbnailSize_{ 48.0f };

	// Operation states
	bool isRenaming_{ false };
	bool isCreatingFile_{ false }; // NEW: Creating file state
	bool needsRefresh_{ false };
	std::string selectedItemForMenu_{ "" };
	char renameBuffer_[256]{ 0 };
	char fileNameBuffer_[256]{ 0 }; // NEW: Buffer for new file names
};