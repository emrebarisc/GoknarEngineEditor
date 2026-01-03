#include "FileBrowserPanel.h"

#include "imgui.h"

#include "Goknar/Engine.h"
#include "Goknar/Contents/Audio.h"
#include "Goknar/Contents/Image.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Model/MeshUnit.h"

FileBrowserPanel::~FileBrowserPanel()
{
	for (auto folder : folderMap)
	{
		delete folder.second;
	}

	delete rootFolder;
}

void FileBrowserPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_None);

	if (ImGui::TreeNode("Assets"))
	{
		DrawFileTree(rootFolder);
		ImGui::TreePop();
	}

	ImGui::End();
}

void FileBrowserPanel::Init()
{
	BuildFileTree();
}

void FileBrowserPanel::DrawFileTree(Folder* folder)
{
	for (auto subFolder : folder->subFolders)
	{
		if (ImGui::TreeNode(subFolder->folderName.c_str()))
		{
			DrawFileTree(subFolder);
			ImGui::TreePop();
		}
	}

	for (auto fileName : folder->fileNames)
	{
		ImGui::Text(fileName.c_str());
	}
}

void FileBrowserPanel::BuildFileTree()
{
	std::vector<std::string> contentPaths;

	ResourceManager* resourceManager = engine->GetResourceManager();

#ifdef ENGINE_CONTENT_DIR
	int resourceIndex = 0;
	while (MeshUnit* mesh = resourceManager->GetResourceContainer()->GetMesh(resourceIndex))
	{
		if (mesh->GetPath().find(std::string(ENGINE_CONTENT_DIR)) == std::string::npos)
		{
			std::string path = mesh->GetPath();
			contentPaths.emplace_back(path.substr(ContentDir.size()));
		}
		++resourceIndex;
	}

	resourceIndex = 0;
	while (Image* image = resourceManager->GetResourceContainer()->GetImage(resourceIndex))
	{
		if (image->GetPath().find(std::string(ENGINE_CONTENT_DIR)) == std::string::npos)
		{
			std::string path = image->GetPath();
			contentPaths.emplace_back(path.substr(ContentDir.size()));
		}
		++resourceIndex;
	}
#endif

	for (std::string& contentPath : contentPaths)
	{
		for (char& c : contentPath)
		{
			if (c == '\\')
			{
				c = '/';
			}
		}
	}

	resourceIndex = 0;
	while (Audio* audio = resourceManager->GetResourceContainer()->GetAudio(resourceIndex))
	{
#ifdef ENGINE_CONTENT_DIR
		if (audio->GetPath().find(std::string(ENGINE_CONTENT_DIR)) == std::string::npos)
		{
			std::string path = audio->GetPath();
			contentPaths.emplace_back(path.substr(ContentDir.size()));
		}
#endif
		++resourceIndex;
	}

	std::sort(contentPaths.begin(), contentPaths.end());

	rootFolder = new Folder();
	rootFolder->folderName = "Content";

	int contentPathsSize = contentPaths.size();
	for (size_t pathIndex = 0; pathIndex < contentPathsSize; pathIndex++)
	{
		std::string currentPath = contentPaths[pathIndex];
		std::string fullPath = currentPath;

		Folder* parentFolder = rootFolder;

		bool breakTheLoop = false;
		while (!currentPath.empty())
		{
			std::string folderName = currentPath.substr(0, currentPath.find_first_of("/"));
			std::string folderPath = fullPath.substr(0, fullPath.find(folderName) + folderName.size() + 1);

			if (folderName.find('.') != std::string::npos)
			{
				if (parentFolder)
				{
					parentFolder->fileNames.push_back(folderName);
				}

				breakTheLoop = true;
			}
			else
			{
				if (folderMap.find(folderPath) == folderMap.end())
				{
					folderMap[folderPath] = new Folder();
					folderMap[folderPath]->folderName = folderName;
					folderMap[folderPath]->fullPath = folderPath;
				}

				if (std::find(parentFolder->subFolders.begin(),
					parentFolder->subFolders.end(),
					folderMap[folderPath]) == std::end(parentFolder->subFolders))
				{
					parentFolder->subFolders.push_back(folderMap[folderPath]);
				}
			}

			if (breakTheLoop)
			{
				break;
			}

			parentFolder = folderMap[folderPath];
			currentPath = currentPath.substr(folderName.size() + 1);
		}
	}
}

void FileBrowserPanel::DrawFileGrid(Folder* folder, std::string& selectedFileName, bool& isAFileSelected)
{
	for (auto subFolder : folder->subFolders)
	{
		if (ImGui::TreeNode(subFolder->folderName.c_str()))
		{
			DrawFileGrid(subFolder, selectedFileName, isAFileSelected);
			ImGui::TreePop();
		}
	}

	ImGui::Columns(4, nullptr, false);
	int fileCount = folder->fileNames.size();
	for (int fileIndex = 0; fileIndex < fileCount; ++fileIndex)
	{
		std::string fileName = folder->fileNames[fileIndex];

		if (ImGui::Button(fileName.c_str(), { 150.f, 30.f }))
		{
			selectedFileName = folder->fullPath + fileName;
			isAFileSelected = true;
		}

		ImGui::NextColumn();
	}
	ImGui::Columns(1, nullptr, false);
}
