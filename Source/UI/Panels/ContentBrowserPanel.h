#pragma once

#include "EditorPanel.h"
#include <vector>
#include <filesystem>

class ContentBrowserPanel : public IEditorPanel
{
public:
	ContentBrowserPanel(EditorHUD* hud) : IEditorPanel("Content Browser", hud) {}
	
	virtual void Init() override;
	virtual void Draw() override;

private:
	struct Folder
	{
		std::string name;
		std::string path;
		std::vector<Folder> subFolders;
		std::vector<std::string> files;
		bool isOpen = false;
	};

	Folder rootFolder_;
	void BuildFileTree(const std::filesystem::path& path, Folder& folder);
	void DrawFolder(Folder& folder);
};