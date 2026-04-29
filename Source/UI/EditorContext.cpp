#include "EditorContext.h"

#include <algorithm>
#include <cctype>

#include "imgui.h"
#include "tinyxml2.h"

#include "Objects/FreeCameraObject.h"
#include "Controllers/FreeCameraController.h"
#include "UI/EditorAssetPathUtils.h"

#include "Goknar/Camera.h"
#include "Goknar/Engine.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Contents/Audio.h"
#include "Goknar/Contents/Image.h"
#include "Goknar/Managers/CameraManager.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Model/MeshUnit.h"
#include "Goknar/Model/SkeletalMesh.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Renderer/RenderTarget.h"

namespace
{
	std::string ToLower(const std::string& value)
	{
		std::string lower = value;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		return lower;
	}

	std::string NormalizePath(const std::filesystem::path& path)
	{
		return path.lexically_normal().generic_string();
	}

	bool StartsWith(const std::string& value, const std::string& prefix)
	{
		return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
	}

	EditorAssetType GetGameAssetType(const std::filesystem::path& absolutePath)
	{
		tinyxml2::XMLDocument document;
		if (document.LoadFile(absolutePath.generic_string().c_str()) != tinyxml2::XML_SUCCESS)
		{
			return EditorAssetType::None;
		}

		tinyxml2::XMLElement* root = document.FirstChildElement("GameAsset");
		const char* fileType = root ? root->Attribute("FileType") : nullptr;
		if (!fileType)
		{
			return EditorAssetType::None;
		}

		const std::string fileTypeString = fileType;
		if (fileTypeString == "Material")
		{
			return EditorAssetType::Material;
		}
		if (fileTypeString == "MaterialFunction")
		{
			return EditorAssetType::MaterialFunction;
		}
		if (fileTypeString == "AnimationGraph")
		{
			return EditorAssetType::AnimationGraph;
		}
		if (fileTypeString == "Scene")
		{
			return EditorAssetType::Scene;
		}

		return EditorAssetType::Unknown;
	}

	EditorAssetType ResolveMeshAssetType(const std::string& assetPath)
	{
		const std::string fileName = std::filesystem::path(assetPath).filename().generic_string();
		if (fileName.rfind("SM_", 0) == 0)
		{
			return EditorAssetType::StaticMesh;
		}
		if (fileName.rfind("SK_", 0) == 0)
		{
			return EditorAssetType::SkeletalMesh;
		}

		const std::string contentRelativePath = EditorAssetPathUtils::ToContentRelativePath(assetPath);
		MeshUnit* mesh = engine->GetResourceManager()->GetContent<MeshUnit>(contentRelativePath);
		if (dynamic_cast<SkeletalMesh*>(mesh))
		{
			return EditorAssetType::SkeletalMesh;
		}
		if (dynamic_cast<StaticMesh*>(mesh))
		{
			return EditorAssetType::StaticMesh;
		}

		return EditorAssetType::Unknown;
	}

	EditorAssetType ClassifyAssetPath(const std::filesystem::path& absolutePath, const std::string& assetPath)
	{
		const std::string extension = ToLower(absolutePath.extension().generic_string());

		if (StartsWith(assetPath, "Content/"))
		{
			if (extension == ".fbx")
			{
				return ResolveMeshAssetType(assetPath);
			}
			if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga")
			{
				return EditorAssetType::Texture;
			}
			if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
			{
				return EditorAssetType::Audio;
			}

			EditorAssetType gameAssetType = GetGameAssetType(absolutePath);
			if (gameAssetType != EditorAssetType::None)
			{
				return gameAssetType;
			}

			if (assetPath.find("/Scenes/") != std::string::npos || assetPath.rfind("Content/Scenes/", 0) == 0)
			{
				return EditorAssetType::Scene;
			}
		}
		else if (StartsWith(assetPath, "Source/"))
		{
			if (extension == ".h" || extension == ".hpp" || extension == ".inl")
			{
				return EditorAssetType::HeaderFile;
			}
			if (extension == ".c" || extension == ".cc" || extension == ".cpp" || extension == ".cxx")
			{
				return EditorAssetType::SourceFile;
			}
		}

		return EditorAssetType::Unknown;
	}

	void SortFolderEntries(Folder* folder)
	{
		if (!folder)
		{
			return;
		}

		std::sort(folder->subFolders.begin(), folder->subFolders.end(), [](const Folder* left, const Folder* right)
			{
				return left->name < right->name;
			});
		std::sort(folder->files.begin(), folder->files.end());

		for (Folder* subFolder : folder->subFolders)
		{
			SortFolderEntries(subFolder);
		}
	}

	void BuildFolderHierarchy(EditorContext* context, Folder* parentFolder, const std::filesystem::path& absolutePath, const std::string& virtualPathPrefix)
	{
		if (!context || !parentFolder || !std::filesystem::exists(absolutePath) || !std::filesystem::is_directory(absolutePath))
		{
			return;
		}

		std::vector<std::filesystem::directory_entry> entries;
		for (const auto& entry : std::filesystem::directory_iterator(absolutePath, std::filesystem::directory_options::skip_permission_denied))
		{
			entries.push_back(entry);
		}

		std::sort(entries.begin(), entries.end(), [](const std::filesystem::directory_entry& left, const std::filesystem::directory_entry& right)
			{
				if (left.is_directory() != right.is_directory())
				{
					return left.is_directory();
				}

				return left.path().filename().generic_string() < right.path().filename().generic_string();
			});

		for (const std::filesystem::directory_entry& entry : entries)
		{
			const std::string entryName = entry.path().filename().generic_string();
			if (entry.is_directory())
			{
				Folder* subFolder = new Folder();
				subFolder->name = entryName;
				subFolder->path = virtualPathPrefix + entryName + "/";
				context->folderMap[subFolder->path] = subFolder;
				parentFolder->subFolders.push_back(subFolder);

				BuildFolderHierarchy(context, subFolder, entry.path(), subFolder->path);
			}
			else if (entry.is_regular_file())
			{
				parentFolder->files.push_back(entryName);
				context->assetTypeMap[virtualPathPrefix + entryName] = ClassifyAssetPath(entry.path(), virtualPathPrefix + entryName);
			}
		}
	}
}

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
	assetTypeMap.clear();

	if (rootFolder)
	{
		delete rootFolder;
		rootFolder = nullptr;
	}

	rootFolder = new Folder();
	rootFolder->name = "Root";

	Folder* contentFolder = new Folder();
	contentFolder->name = "Content";
	contentFolder->path = "Content/";
	folderMap[contentFolder->path] = contentFolder;

	rootFolder->subFolders.push_back(contentFolder);
	BuildFolderHierarchy(this, contentFolder, std::filesystem::path(EditorAssetPathUtils::GetContentRootPath()), contentFolder->path);
	BuildSourceFileTree();
	SortFolderEntries(rootFolder);
}

void EditorContext::BuildSourceFileTree()
{
	const std::string sourceDirStr = NormalizePath(std::filesystem::path(EditorAssetPathUtils::GetProjectRootPath()) / "Source");

	if (!std::filesystem::exists(sourceDirStr) || !std::filesystem::is_directory(sourceDirStr))
	{
		return;
	}

	Folder* sourceFolder = new Folder();
	sourceFolder->name = "Source";
	sourceFolder->path = "Source/";
	folderMap[sourceFolder->path] = sourceFolder;

	rootFolder->subFolders.push_back(sourceFolder);
	BuildFolderHierarchy(this, sourceFolder, std::filesystem::path(sourceDirStr), sourceFolder->path);
}

EditorAssetType EditorContext::GetAssetType(const std::string& path) const
{
	auto assetTypeIterator = assetTypeMap.find(path);
	if (assetTypeIterator == assetTypeMap.end())
	{
		return EditorAssetType::None;
	}

	return assetTypeIterator->second;
}
