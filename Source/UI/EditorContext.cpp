#include "EditorContext.h"

#include <algorithm>

#include "imgui.h"

#include "Objects/FreeCameraObject.h"
#include "Controllers/FreeCameraController.h"

#include "Goknar/Camera.h"
#include "Goknar/Engine.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Contents/Audio.h"
#include "Goknar/Contents/Image.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Renderer/RenderTarget.h"

EditorContext::EditorContext()
{
	imguiContext_ = ImGui::CreateContext();

	viewportCameraObject = new FreeCameraObject();
	viewportCameraObject->SetName("__Editor__ViewportCamera");
	viewportCameraObject->GetFreeCameraController()->SetName("__Editor__FreeCameraController");

	Camera* viewportCamera = viewportCameraObject->GetCameraComponent()->GetCamera();
	viewportCamera->SetCameraType(CameraType::RenderTarget);

	viewportRenderTarget = new RenderTarget();
	viewportRenderTarget->SetCamera(viewportCamera);
	viewportRenderTarget->Init();
}

EditorContext::~EditorContext()
{
	delete viewportRenderTarget;

	viewportCameraObject->Destroy();

	ImGui::DestroyContext(imguiContext_);

	for (auto folder : folderMap)
	{
		delete folder.second;
	}

	delete rootFolder;
}

void EditorContext::Init()
{
	//rootFolder = new Folder();
	//BuildFileTree("Content", rootFolder);

	BuildFileTree();
}

//void EditorContext::BuildFileTree(const std::filesystem::path& path, Folder* folder)
//{
//	folder->path = path.string();
//	folder->name = path.filename().string();
//
//	for (const auto& entry : std::filesystem::directory_iterator(path))
//	{
//		if (entry.is_directory())
//		{
//			Folder* subFolder = new Folder();
//			BuildFileTree(entry.path(), subFolder);
//			folder->subFolders.push_back(subFolder);
//		}
//		else
//		{
//			folder->files.push_back(entry.path().filename().string());
//		}
//	}
//}

void EditorContext::SetCameraMovement(bool value)
{
	viewportCameraObject->GetFreeCameraController()->SetIsActive(value);
}

void EditorContext::BuildFileTree()
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
	rootFolder->name = "Content";

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
					parentFolder->files.push_back(folderName);
				}

				breakTheLoop = true;
			}
			else
			{
				if (folderMap.find(folderPath) == folderMap.end())
				{
					folderMap[folderPath] = new Folder();
					folderMap[folderPath]->name = folderName;
					folderMap[folderPath]->path = folderPath;
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
