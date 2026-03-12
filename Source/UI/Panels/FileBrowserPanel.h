#pragma once

#include "EditorPanel.h"
#include "UI/EditorContext.h"

class FileBrowserPanel : public IEditorPanel
{
public:
	FileBrowserPanel(EditorHUD* hud);

	virtual void Draw() override;

private:
	void DrawGrid();

	Folder* currentFolder_{ nullptr };
	float thumbnailSize_{ 96.0f };
};