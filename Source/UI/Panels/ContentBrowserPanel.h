#pragma once

#include <filesystem>

#include "EditorPanel.h"
#include "UI/EditorContext.h"

class ContentBrowserPanel : public IEditorPanel
{
public:
	ContentBrowserPanel(EditorHUD* hud) : IEditorPanel("Content Browser", hud) {}
	
	virtual void Draw() override;

private:
	void DrawFolder(Folder* folder);
};