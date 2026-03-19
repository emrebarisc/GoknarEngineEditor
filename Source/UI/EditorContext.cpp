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
#include "Goknar/Managers/CameraManager.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Renderer/RenderTarget.h"

EditorContext::EditorContext()
{
	imguiContext_ = ImGui::CreateContext();

	viewportCameraObject = new FreeCameraObject();
	viewportCameraObject->SetName("__Editor__ViewportCamera");
	viewportCameraObject->GetController()->SetName("__Editor__FreeCameraController");

	Camera* viewportCamera = viewportCameraObject->GetCameraComponent()->GetCamera();
	viewportCamera->SetCameraType(CameraType::RenderTarget);

	viewportRenderTarget = new RenderTarget();
	viewportRenderTarget->SetCamera(viewportCamera);
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
	viewportRenderTarget->Init();

	BuildFileTree();
}

void EditorContext::SetCameraMovement(bool value)
{
	viewportCameraObject->GetController()->SetIsActive(value);
}

void EditorContext::BuildFileTree()
{
	for (auto folder : folderMap)
	{
		delete folder.second;
	}
	folderMap.clear();

	if (rootFolder)
	{
		delete rootFolder;
		rootFolder = nullptr;
	}

	std::vector<std::string> contentPaths;

	ResourceManager* resourceManager = engine->GetResourceManager();

	auto getRelativePath = [](const std::string& fullPath, const std::string& baseDir) -> std::string {
		std::string normalizedFullPath = fullPath;
		std::string normalizedBaseDir = baseDir;

		auto normalizeSlashes = [](std::string& path) {
			for (char& c : path)
				if (c == '\\') c = '/';
			
			size_t pos;
			while ((pos = path.find("//")) != std::string::npos) {
				path.replace(pos, 2, "/");
			}
		};

		normalizeSlashes(normalizedFullPath);
		normalizeSlashes(normalizedBaseDir);

		size_t pos = normalizedFullPath.find(normalizedBaseDir);
		if (pos != std::string::npos) {
			return normalizedFullPath.substr(pos + normalizedBaseDir.length());
		}
		
		return "";
	};

#ifdef ENGINE_CONTENT_DIR
	int resourceIndex = 0;
	while (MeshUnit* mesh = resourceManager->GetResourceContainer()->GetMesh(resourceIndex))
	{
		if (mesh->GetPath().find(std::string(ENGINE_CONTENT_DIR)) == std::string::npos)
		{
			contentPaths.emplace_back(getRelativePath(mesh->GetPath(), ContentDir));
		}
		++resourceIndex;
	}

	resourceIndex = 0;
	while (Image* image = resourceManager->GetResourceContainer()->GetImage(resourceIndex))
	{
		if (image->GetPath().find(std::string(ENGINE_CONTENT_DIR)) == std::string::npos)
		{
			contentPaths.emplace_back(getRelativePath(image->GetPath(), ContentDir));
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
			contentPaths.emplace_back(getRelativePath(audio->GetPath(), ContentDir));
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
		std::string accumulatedPath = "";

		while (!currentPath.empty())
		{
			std::string folderName = currentPath.substr(0, currentPath.find_first_of("/"));
			
			accumulatedPath += folderName + "/";
			std::string folderPath = accumulatedPath;

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
