#pragma once

#include "EditorPanel.h"

class SaveScenePanel : public IEditorPanel
{
public:
	SaveScenePanel(EditorHUD* hud) : 
		IEditorPanel("SaveScene", hud)
	{
		isOpen_ = false;
	}
	
	virtual void Draw() override;
};