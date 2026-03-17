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
	rootFolder = new Folder();
	rootFolder->name = "Project";

	auto AddFolderTree = [this](const std::string& startPath, Folder* root) {
		std::filesystem::path basePath(startPath);
		if (!std::filesystem::exists(basePath) || !std::filesystem::is_directory(basePath))
		{
			return;
		}

		Folder* baseFolder = new Folder();
		baseFolder->name = basePath.filename().string();
		baseFolder->path = basePath.string();
		root->subFolders.push_back(baseFolder);
		folderMap[basePath.string()] = baseFolder;

		for (auto& entry : std::filesystem::recursive_directory_iterator(basePath, std::filesystem::directory_options::skip_permission_denied))
		{
			const auto& path = entry.path();
			std::string pathStr = path.string();
			std::string parentPathStr = path.parent_path().string();
			std::string filename = path.filename().string();

			if (folderMap.find(parentPathStr) == folderMap.end())
			{
				continue; // Shouldn't happen with recursive iterator unless it's a hidden/filtered folder we skipped
			}

			Folder* parentFolder = folderMap[parentPathStr];

			if (entry.is_directory())
			{
				Folder* newFolder = new Folder();
				newFolder->name = filename;
				newFolder->path = pathStr;
				parentFolder->subFolders.push_back(newFolder);
				folderMap[pathStr] = newFolder;
			}
			else
			{
				parentFolder->files.push_back(filename); // Just push filename to match existing file browser logic
			}
		}
	};

	AddFolderTree(ContentDir, rootFolder);
	AddFolderTree("Source", rootFolder);
}
