#pragma once

#include "EditorPanel.h"

class MenuBar : public IEditorPanel
{
public:
	MenuBar(EditorHUD* hud) : 
		IEditorPanel("MenuBar", hud)
	{

	}
	
	virtual void Draw() override;
};