#include "ContentBrowserPanel.h"

#include "imgui.h"

void ContentBrowserPanel::Init()
{
	BuildFileTree("Content", rootFolder_);
}

void ContentBrowserPanel::BuildFileTree(const std::filesystem::path& path, Folder& folder)
{
	folder.path = path.string();
	folder.name = path.filename().string();

	for (const auto& entry : std::filesystem::directory_iterator(path))
	{
		if (entry.is_directory())
		{
			Folder sub;
			BuildFileTree(entry.path(), sub);
			folder.subFolders.push_back(sub);
		}
		else
		{
			folder.files.push_back(entry.path().filename().string());
		}
	}
}

void ContentBrowserPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_);

	DrawFolder(rootFolder_);
	ImGui::End();
}

void ContentBrowserPanel::DrawFolder(Folder& folder)
{
	bool nodeOpen = ImGui::TreeNode(folder.name.c_str());
	if (nodeOpen)
	{
		for (auto& sub : folder.subFolders)
		{
			DrawFolder(sub);
		}

		for (auto& file : folder.files)
		{
			ImGui::Text("%s", file.c_str());
		}
		
		ImGui::TreePop();
	}
}