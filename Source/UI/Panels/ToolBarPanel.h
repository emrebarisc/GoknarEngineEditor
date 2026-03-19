#pragma once

#include "EditorPanel.h"

class ToolBarPanel : public IEditorPanel
{
public:
	ToolBarPanel(EditorHUD* hud) : 
		IEditorPanel("Tool Bar", hud)
	{

	}
	
	virtual void Draw() override;
};