#pragma once

#include "EditorPanel.h"
#include <string>

class SystemFileBrowserPanel : public IEditorPanel
{
public:
	SystemFileBrowserPanel(EditorHUD* hud);

	virtual void Draw() override;

private:
	std::string currentPath_;
	std::string selectedFolder_;
};
