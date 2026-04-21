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

	int resourceIndex = 0;
#ifdef ENGINE_CONTENT_DIR
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
	rootFolder->name = "Root";

	Folder* contentFolder = new Folder();
	contentFolder->name = "Content";
	contentFolder->path = "Content/";
	folderMap[contentFolder->path] = contentFolder;

	rootFolder->subFolders.push_back(contentFolder);

	int contentPathsSize = (int)contentPaths.size();
	for (size_t pathIndex = 0; pathIndex < contentPathsSize; pathIndex++)
	{
		std::string currentPath = contentPaths[pathIndex];
		std::string fullPath = currentPath;

		Folder* parentFolder = contentFolder;

		bool breakTheLoop = false;
		std::string accumulatedPath = "";

		while (!currentPath.empty())
		{
			std::string folderName = currentPath.substr(0, currentPath.find_first_of("/"));
			
			accumulatedPath += folderName + "/";
			std::string folderPath = std::string("Content/") + accumulatedPath;

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
	BuildSourceFileTree();
}

void EditorContext::BuildSourceFileTree()
{
	// Derive the Source directory path from ContentDir
	std::string sourceDirStr = ContentDir;
	size_t contentPos = sourceDirStr.rfind("Content");
	if (contentPos != std::string::npos)
	{
		sourceDirStr.replace(contentPos, 7, "Source");
	}

	// Make sure the directory actually exists before trying to parse it
	if (!std::filesystem::exists(sourceDirStr) || !std::filesystem::is_directory(sourceDirStr))
	{
		return;
	}

	Folder* sourceFolder = new Folder();
	sourceFolder->name = "Source";
	sourceFolder->path = "Source/";
	folderMap[sourceFolder->path] = sourceFolder;

	rootFolder->subFolders.push_back(sourceFolder);

	std::vector<std::string> sourcePaths;

	// Reusing the getRelativePath lambda logic for physical files
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

	// Recursively scan the Source directory for files
	for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDirStr))
	{
		if (entry.is_regular_file())
		{
			std::string fullPath = entry.path().string();
			std::string relativePath = getRelativePath(fullPath, sourceDirStr);

			if (!relativePath.empty())
			{
				// Strip leading slash if present to match the Content parsing logic format
				if (relativePath[0] == '/')
				{
					relativePath = relativePath.substr(1);
				}
				sourcePaths.emplace_back(relativePath);
			}
		}
	}

	std::sort(sourcePaths.begin(), sourcePaths.end());

	// Map the gathered relative paths into the Folder structure
	size_t sourcePathsSize = sourcePaths.size();
	for (size_t pathIndex = 0; pathIndex < sourcePathsSize; pathIndex++)
	{
		std::string currentPath = sourcePaths[pathIndex];
		std::string fullPath = currentPath;

		Folder* parentFolder = sourceFolder;

		bool breakTheLoop = false;
		std::string accumulatedPath = "";

		while (!currentPath.empty())
		{
			std::string folderName = currentPath.substr(0, currentPath.find_first_of("/"));

			accumulatedPath += folderName + "/";
			std::string folderPath = std::string("Source/") + accumulatedPath;

			// Assuming anything with a '.' is a file (matching original logic)
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