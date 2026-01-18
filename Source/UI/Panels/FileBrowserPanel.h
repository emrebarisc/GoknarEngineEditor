#pragma once

#include "EditorPanel.h"

struct Folder;

class FileBrowserPanel : public IEditorPanel
{
public:
	FileBrowserPanel(EditorHUD* hud) : 
		IEditorPanel("FileBrowser", hud)
	{

	}

	~FileBrowserPanel();
	
	virtual void Draw() override;
	virtual void Init() override;

private:
	void DrawFileTree(const Folder* folder);
};