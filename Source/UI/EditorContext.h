#pragma once

#include "Goknar/Core.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "Goknar/Math/GoknarMath.h"

struct ImGuiContext;

class EditorFreeCameraObject;
class ObjectBase;
class RenderTarget;

enum class EditorSelectionType
{
	None = 0,
	Object,
	DirectionalLight,
	PointLight,
	SpotLight,
	Scene
};

enum class EditorAssetType
{
	None = 0,
	Material,
	MaterialFunction,
	Texture,
	StaticMesh,
	SkeletalMesh,
	AnimationGraph,
	NavigationTree,
	Audio,
	Scene,
	HeaderFile,
	SourceFile,
	Unknown
};

struct Folder
{
	std::string name;
	std::string path;
	std::vector<Folder*> subFolders;
	std::vector<std::string> files;
	bool isOpen = false;
};

class GOKNAR_API EditorContext
{
public:
	static EditorContext* Get()
	{
		if (!instance_)
		{
			instance_ = new EditorContext();
		}

		return instance_;
	}

	static void Destroy()
	{
		delete instance_;
		instance_ = nullptr;
	}

	~EditorContext();

	void Init();
	void BuildFileTree();
	void BuildSourceFileTree();
	EditorAssetType GetAssetType(const std::string& path) const;
	unsigned int GetFileTreeVersion() const
	{
		return fileTreeVersion_;
	}

	void SetSelection(void* obj, EditorSelectionType type)
	{
		selectedObject = obj;
		selectedObjectType = type;
		selectedObjects.clear();
		if (type == EditorSelectionType::Object && obj)
		{
			selectedObjects.push_back(static_cast<ObjectBase*>(obj));
		}
	}

	void ClearSelection()
	{
		selectedObject = nullptr;
		selectedObjectType = EditorSelectionType::None;
		selectedObjects.clear();
	}

	void SetObjectSelection(ObjectBase* object, bool additiveSelection = false)
	{
		if (!object)
		{
			if (!additiveSelection)
			{
				ClearSelection();
			}
			return;
		}

		if (!additiveSelection)
		{
			selectedObjects.clear();
			selectedObjects.push_back(object);
			selectedObject = object;
			selectedObjectType = EditorSelectionType::Object;
			return;
		}

		auto selectedObjectIterator = std::find(selectedObjects.begin(), selectedObjects.end(), object);
		if (selectedObjectIterator != selectedObjects.end())
		{
			selectedObject = object;
			selectedObjectType = EditorSelectionType::Object;
			return;
		}

		selectedObjects.push_back(object);
		selectedObject = object;
		selectedObjectType = EditorSelectionType::Object;
	}

	void RemoveObjectSelection(ObjectBase* object)
	{
		if (!object)
		{
			return;
		}

		selectedObjects.erase(std::remove(selectedObjects.begin(), selectedObjects.end(), object), selectedObjects.end());
		if (selectedObjects.empty())
		{
			ClearSelection();
			return;
		}

		selectedObject = selectedObjects.back();
		selectedObjectType = EditorSelectionType::Object;
	}

	bool IsObjectSelected(const ObjectBase* object) const
	{
		return object && std::find(selectedObjects.begin(), selectedObjects.end(), object) != selectedObjects.end();
	}

	const std::vector<ObjectBase*>& GetSelectedObjects() const
	{
		return selectedObjects;
	}

	void ClearCreateData()
	{
		objectToCreateType = EditorSelectionType::None;
		objectToCreateName = "";
		isPlacingObject = false;
	}

	template <typename T>
	T* GetSelectionAs()
	{
		return static_cast<T*>(selectedObject);
	}

	void SetCameraMovement(bool value);

	void MarkSceneDirty(const std::string& reason = "");
	void ClearSceneDirty();

	bool GetIsSceneDirty() const
	{
		return isSceneDirty_;
	}

	const std::string& GetSceneDirtyReason() const
	{
		return sceneDirtyReason_;
	}

	std::string objectToCreateName;
	std::string sceneSavePath;

	Vector2i windowSize;
	Vector2i buttonSize;

	void* selectedObject = nullptr;
	std::vector<ObjectBase*> selectedObjects{};

	EditorFreeCameraObject* viewportCameraObject{ nullptr };
	RenderTarget* viewportRenderTarget{ nullptr };

	ImGuiContext* imguiContext_{ nullptr };

	EditorSelectionType selectedObjectType = EditorSelectionType::None;
	EditorSelectionType objectToCreateType = EditorSelectionType::None;

	Folder* rootFolder{ nullptr };
	std::unordered_map<std::string, Folder*> folderMap{};
	std::unordered_map<std::string, EditorAssetType> assetTypeMap{};
	EditorAssetType assetSelectorFilter{ EditorAssetType::None };

	bool isPlacingObject = false;
private:
	EditorContext();

	inline static EditorContext* instance_{ nullptr };

	bool isSceneDirty_{ false };
	std::string sceneDirtyReason_;
	unsigned int fileTreeVersion_{ 0 };
};
