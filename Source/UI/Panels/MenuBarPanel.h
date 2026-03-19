#pragma once

#include "EditorPanel.h"

class MenuBarPanel : public IEditorPanel
{
public:
	MenuBarPanel(EditorHUD* hud) : 
		IEditorPanel("Menu Bar", hud)
	{

	}
	
	virtual void Draw() override;

private:
	void OnProjectSelected(const std::string& directoryPath);
};