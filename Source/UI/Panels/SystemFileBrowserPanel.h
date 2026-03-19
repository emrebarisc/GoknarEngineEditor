#pragma once

#include "EditorPanel.h"

#include "Goknar/Delegates/Delegate.h"
#include <string>

class SystemFileBrowserPanel : public IEditorPanel
{
public:
	SystemFileBrowserPanel(EditorHUD* hud);

	virtual void Draw() override;

	void SetOnDirectorySelectedCallback(const Delegate<void(const std::string&)>& callback)
	{
		onDirectorySelectedCallback_ = callback;
	}

private:
	std::string currentPath_;
	std::string selectedFolder_;

	Delegate<void(const std::string&)> onDirectorySelectedCallback_;
};
