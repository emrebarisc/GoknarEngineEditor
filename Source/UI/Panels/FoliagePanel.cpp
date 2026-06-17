#include "FoliagePanel.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

#include "Goknar/Application.h"
#include "Goknar/Camera.h"
#include "Goknar/Engine.h"
#include "Goknar/Log.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Scene.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Components/InstancedStaticMeshComponent.h"
#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Managers/InputManager.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Model/InstancedStaticMesh.h"
#include "Goknar/Model/InstancedStaticMeshInstance.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Physics/PhysicsWorld.h"
#include "Goknar/Renderer/RenderTarget.h"

#include "UI/EditorAssetPathUtils.h"
#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/EditorUtils.h"
#include "UI/Panels/AssetSelectorPanel.h"
#include "UI/Panels/ViewportPanel.h"

namespace
{
	constexpr const char* FoliageGridObjectPrefix = "FoliageGridObject";
	constexpr const char* BrushPreviewObjectName = "__Editor__FoliageBrushPreview";
	constexpr float ViewportRaycastDistance = 10000.f;
	constexpr float PlacementRaycastDistance = 1000.f;
	constexpr std::size_t MaxUndoSnapshots = 32u;

	bool StartsWith(const std::string& value, const std::string& prefix)
	{
		return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
	}

	std::string StripInstancedMeshSuffix(const std::string& path)
	{
		const std::size_t suffixPosition = path.find("::");
		return suffixPosition == std::string::npos ? path : path.substr(0, suffixPosition);
	}

	float RandomFloat(std::mt19937& generator, float minValue, float maxValue)
	{
		std::uniform_real_distribution<float> distribution(minValue, maxValue);
		return distribution(generator);
	}

	int ClampPositiveInt(int value, int minValue, int maxValue)
	{
		return GoknarMath::Clamp(value, minValue, maxValue);
	}
}

FoliagePanel::FoliagePanel(EditorHUD* hud) :
	IEditorPanel("Foliage Paint", hud),
	randomGenerator_(randomSeed_)
{
	isOpen_ = false;
}

FoliagePanel::~FoliagePanel()
{
	DestroyBrushPreview();
}

void FoliagePanel::Init()
{
	SynchronizeFromScene();
}

void FoliagePanel::Draw()
{
	UpdateToolState();
	UpdateBrushPreview();

	if (isStrokeInProgress_)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && IsCursorInsideViewport() && !IsItemHovered())
		{
			ApplyBrushAtCursor(false);
		}
		else
		{
			EndStroke();
		}
	}

	if (!ImGui::Begin(title_.c_str(), &isOpen_))
	{
		ImGui::End();
		return;
	}

	const ImVec2 windowPosition = ImGui::GetWindowPos();
	const ImVec2 windowSize = ImGui::GetWindowSize();
	position_ = Vector2i((int)windowPosition.x, (int)windowPosition.y);
	size_ = Vector2i((int)windowSize.x, (int)windowSize.y);

	bool active = isToolActive_;
	if (ImGui::Checkbox("Active", &active))
	{
		SetToolActive(active);
	}

	ImGui::SameLine();
	if (ImGui::RadioButton("Paint", !eraseMode_))
	{
		eraseMode_ = false;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Erase", eraseMode_))
	{
		eraseMode_ = true;
	}

	ImGui::Separator();
	DrawBrushSettings();
	ImGui::Separator();
	DrawMeshList();
	ImGui::Separator();
	DrawHistoryControls();
	DrawNotes();

	ImGui::End();

	if (!isOpen_)
	{
		SetToolActive(false);
	}
}

void FoliagePanel::SetIsOpen(bool isOpen)
{
	IEditorPanel::SetIsOpen(isOpen);
	if (!isOpen)
	{
		SetToolActive(false);
	}
}

bool FoliagePanel::HandleViewportLeftPressed()
{
	if (!isToolActive_ || IsItemHovered() || !IsCursorInsideViewport())
	{
		return false;
	}

	BeginStroke();
	ApplyBrushAtCursor(true);
	return true;
}

void FoliagePanel::HandleViewportLeftReleased()
{
	if (isStrokeInProgress_)
	{
		EndStroke();
	}
}

void FoliagePanel::PrepareSceneForSave()
{
	if (!hasSynchronizedScene_)
	{
		return;
	}

	RebuildAllCells();
}

void FoliagePanel::DrawBrushSettings()
{
	ImGui::Text("Brush");

	ImGui::DragFloat("Radius", &brushRadius_, 0.1f, 0.1f, 1000.f, "%.2f");
	brushRadius_ = GoknarMath::Max(0.1f, brushRadius_);

	ImGui::DragFloat("Density", &density_, 0.01f, 0.f, 100.f, "%.2f");
	density_ = GoknarMath::Max(0.f, density_);

	ImGui::DragFloat("Minimum Spacing", &minimumSpacing_, 0.01f, 0.f, 1000.f, "%.2f");
	minimumSpacing_ = GoknarMath::Max(0.f, minimumSpacing_);

	ImGui::DragInt("Max Attempts / Step", &maxAttemptsPerBrushStep_, 1.f, 1, 4096);
	maxAttemptsPerBrushStep_ = ClampPositiveInt(maxAttemptsPerBrushStep_, 1, 4096);

	float editedGridSize = gridSize_;
	if (ImGui::DragFloat("Foliage Grid Size", &editedGridSize, 1.f, 1.f, 100000.f, "%.2f"))
	{
		gridSize_ = GoknarMath::Max(1.f, editedGridSize);
	}
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		RebuildAllCells();
		EditorContext::Get()->MarkSceneDirty("Foliage grid rebuilt");
	}

	int seed = static_cast<int>(randomSeed_);
	if (ImGui::InputInt("Random Seed", &seed))
	{
		randomSeed_ = static_cast<std::uint32_t>(GoknarMath::Max(0, seed));
		randomGenerator_.seed(randomSeed_);
	}
}

void FoliagePanel::DrawMeshList()
{
	ImGui::Text("Meshes");

	if (ImGui::Button("Add Mesh"))
	{
		OpenMeshSelector();
	}

	ImGui::SameLine();
	ImGui::Text("Instances: %llu", static_cast<unsigned long long>(GetInstanceCount()));
	ImGui::SameLine();
	ImGui::Text("Cells: %llu", static_cast<unsigned long long>(runtimeCells_.size()));

	for (int meshIndex = 0; meshIndex < static_cast<int>(meshEntries_.size()); ++meshIndex)
	{
		MeshEntry& entry = meshEntries_[meshIndex];
		if (!entry.mesh && !entry.meshPath.empty())
		{
			entry.mesh = ResolveMesh(entry.meshPath);
		}

		ImGui::PushID(meshIndex);
		const std::string label = entry.meshPath.empty() ? "Missing Mesh" : entry.meshPath;
		const bool treeOpen = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

		ImGui::SameLine();
		if (ImGui::SmallButton("Remove"))
		{
			meshEntries_.erase(meshEntries_.begin() + meshIndex);
			ImGui::PopID();
			--meshIndex;
			continue;
		}

		if (treeOpen)
		{
			ImGui::Checkbox("Enabled", &entry.enabled);
			ImGui::DragFloat("Spawn Weight", &entry.spawnWeight, 0.05f, 0.f, 1000.f, "%.2f");
			entry.spawnWeight = GoknarMath::Max(0.f, entry.spawnWeight);

			ImGui::DragFloat("Density Multiplier", &entry.densityMultiplier, 0.05f, 0.f, 1000.f, "%.2f");
			entry.densityMultiplier = GoknarMath::Max(0.f, entry.densityMultiplier);

			ImGui::DragFloat("Min Scale", &entry.minScale, 0.01f, 0.001f, 1000.f, "%.3f");
			ImGui::DragFloat("Max Scale", &entry.maxScale, 0.01f, 0.001f, 1000.f, "%.3f");
			entry.minScale = GoknarMath::Max(0.001f, entry.minScale);
			entry.maxScale = GoknarMath::Max(entry.minScale, entry.maxScale);

			ImGui::Checkbox("Random Yaw", &entry.randomYaw);
			ImGui::Checkbox("Align To Surface Normal", &entry.alignToSurfaceNormal);
			ImGui::DragFloat("Ground Offset", &entry.groundOffset, 0.01f, -1000.f, 1000.f, "%.3f");

			if (!entry.mesh)
			{
				ImGui::TextColored(ImVec4(1.f, 0.45f, 0.25f, 1.f), "Mesh is missing or failed to load.");
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}
}

void FoliagePanel::DrawHistoryControls()
{
	if (ImGui::Button("Undo") && !undoStack_.empty())
	{
		UndoStroke();
	}

	ImGui::SameLine();
	if (ImGui::Button("Redo") && !redoStack_.empty())
	{
		RedoStroke();
	}

	ImGui::SameLine();
	if (ImGui::Button("Rebuild Foliage Grid"))
	{
		RebuildAllCells();
		EditorContext::Get()->MarkSceneDirty("Foliage grid rebuilt");
	}
}

void FoliagePanel::DrawNotes()
{
	if (!ImGui::CollapsingHeader("Implementation Notes"))
	{
		return;
	}

	ImGui::TextWrapped("Foliage is stored as ObjectBase grid cells with InstancedStaticMeshComponent children.");
	ImGui::TextWrapped("Global Ctrl+Z is not available in the current editor; use this panel's foliage-local Undo and Redo buttons.");
	ImGui::TextWrapped("TODO: if the engine exposes public component removal and instanced bounds APIs later, remove empty mesh components and update renderer-visible per-cell bounds directly.");
}

void FoliagePanel::OpenMeshSelector()
{
	EditorContext::Get()->assetSelectorFilter = EditorAssetType::StaticMesh;
	AssetSelectorPanel::OnAssetSelected =
		Delegate<void(const std::string&)>::Create<FoliagePanel, &FoliagePanel::OnMeshAssetSelected>(this);
	hud_->ShowPanel<AssetSelectorPanel>();
}

void FoliagePanel::OnMeshAssetSelected(const std::string& path)
{
	const std::string meshPath = NormalizeMeshPath(path);
	if (meshPath.empty())
	{
		return;
	}

	StaticMesh* mesh = ResolveMesh(meshPath);
	if (!mesh)
	{
		GOKNAR_CORE_WARN("Foliage mesh %s could not be loaded.", meshPath.c_str());
		return;
	}

	MeshEntry entry;
	entry.meshPath = meshPath;
	entry.mesh = mesh;
	meshEntries_.push_back(entry);
}

bool FoliagePanel::IsCursorInsideViewport() const
{
	ViewportPanel* viewportPanel = hud_ ? hud_->GetPanel<ViewportPanel>() : nullptr;
	if (!viewportPanel)
	{
		return false;
	}

	double xPosition = 0.0;
	double yPosition = 0.0;
	engine->GetInputManager()->GetCursorPosition(xPosition, yPosition);

	const Vector2i& viewportPosition = viewportPanel->GetPosition();
	const Vector2i& viewportSize = viewportPanel->GetSize();
	return viewportPosition.x <= xPosition &&
		xPosition <= viewportPosition.x + viewportSize.x &&
		viewportPosition.y <= yPosition &&
		yPosition <= viewportPosition.y + viewportSize.y;
}

bool FoliagePanel::GetViewportCursorRay(Vector3& outOrigin, Vector3& outDirection) const
{
	EditorContext* context = EditorContext::Get();
	ViewportPanel* viewportPanel = hud_ ? hud_->GetPanel<ViewportPanel>() : nullptr;
	if (!context || !context->viewportRenderTarget || !viewportPanel || !engine)
	{
		return false;
	}

	Camera* activeCamera = context->viewportRenderTarget->GetCamera();
	if (!activeCamera)
	{
		return false;
	}

	double xPosition = 0.0;
	double yPosition = 0.0;
	engine->GetInputManager()->GetCursorPosition(xPosition, yPosition);

	const Vector2i& viewportPosition = viewportPanel->GetPosition();
	outOrigin = activeCamera->GetPosition();
	outDirection = activeCamera->GetWorldDirectionAtPixel(
		Vector2i{ (int)(xPosition - viewportPosition.x), (int)(yPosition - viewportPosition.y) }).GetNormalized();

	return outDirection.SquareLength() > SMALLER_EPSILON;
}

bool FoliagePanel::Raycast(const Vector3& from, const Vector3& to, BrushHit& outHit) const
{
	if (!engine || !engine->GetPhysicsWorld())
	{
		return false;
	}

	RaycastData raycastData;
	raycastData.from = from;
	raycastData.to = to;
	raycastData.collisionGroup = CollisionGroup::All;
	raycastData.collisionMask = CollisionMask(0xFFFFFF);

	RaycastSingleResult result;
	if (!engine->GetPhysicsWorld()->RaycastClosest(raycastData, result))
	{
		return false;
	}

	outHit.position = result.hitPosition;
	outHit.normal = result.hitNormal.SquareLength() > SMALLER_EPSILON ? result.hitNormal.GetNormalized() : Vector3::UpVector;
	return true;
}

bool FoliagePanel::TryGetBrushHit(BrushHit& outHit) const
{
	if (!IsCursorInsideViewport())
	{
		return false;
	}

	Vector3 rayOrigin;
	Vector3 rayDirection;
	if (!GetViewportCursorRay(rayOrigin, rayDirection))
	{
		return false;
	}

	return Raycast(rayOrigin, rayOrigin + rayDirection * ViewportRaycastDistance, outHit);
}

void FoliagePanel::UpdateToolState()
{
	Scene* currentScene = GetCurrentScene();
	if (currentScene != synchronizedScene_)
	{
		ClearRuntimeCells(false);
		instances_.clear();
		synchronizedScene_ = currentScene;
		hasSynchronizedScene_ = false;
	}

	if (!hasSynchronizedScene_)
	{
		SynchronizeFromScene();
	}
}

void FoliagePanel::SetToolActive(bool active)
{
	if (isToolActive_ == active)
	{
		return;
	}

	isToolActive_ = active;
	if (isToolActive_)
	{
		EnsureBrushPreview();
	}
	else
	{
		EndStroke();
		HideBrushPreview();
	}
}

void FoliagePanel::EnsureBrushPreview()
{
	if (brushPreviewObject_)
	{
		return;
	}

	StaticMesh* previewMesh = EditorUtils::GetEditorContent<StaticMesh>("Meshes/SM_UnitSphere.fbx");
	if (!previewMesh)
	{
		GOKNAR_CORE_WARN("Foliage brush preview mesh EditorContent/Meshes/SM_UnitSphere.fbx could not be loaded.");
		return;
	}

	brushPreviewObject_ = new ObjectBase();
	brushPreviewObject_->SetName(BrushPreviewObjectName);
	brushPreviewComponent_ = brushPreviewObject_->AddSubComponent<StaticMeshComponent>();
	brushPreviewComponent_->SetMesh(previewMesh);
	brushPreviewObject_->SetWorldScaling(Vector3(brushRadius_));
	brushPreviewObject_->SetIsActive(false);

	engine->InitializePendingObjectsAndComponents();
}

void FoliagePanel::UpdateBrushPreview()
{
	if (!isToolActive_)
	{
		return;
	}

	EnsureBrushPreview();
	if (!brushPreviewObject_)
	{
		return;
	}

	BrushHit hit;
	if (!TryGetBrushHit(hit))
	{
		HideBrushPreview();
		return;
	}

	brushPreviewObject_->SetWorldPosition(hit.position, false);
	brushPreviewObject_->SetWorldRotation(Quaternion::Identity, false);
	brushPreviewObject_->SetWorldScaling(Vector3(brushRadius_));
	brushPreviewObject_->SetIsActive(true);
}

void FoliagePanel::HideBrushPreview()
{
	if (brushPreviewObject_)
	{
		brushPreviewObject_->SetIsActive(false);
	}
}

void FoliagePanel::DestroyBrushPreview()
{
	if (!brushPreviewObject_)
	{
		return;
	}

	brushPreviewObject_->Destroy();
	brushPreviewObject_ = nullptr;
	brushPreviewComponent_ = nullptr;
	if (engine)
	{
		engine->FlushPendingDestroy();
	}
}

void FoliagePanel::BeginStroke()
{
	if (isStrokeInProgress_)
	{
		return;
	}

	strokeStartSnapshot_ = instances_;
	strokeChangedInstances_ = false;
	hasLastStrokePoint_ = false;
	isStrokeInProgress_ = true;
}

void FoliagePanel::EndStroke()
{
	if (!isStrokeInProgress_)
	{
		return;
	}

	isStrokeInProgress_ = false;
	hasLastStrokePoint_ = false;

	if (strokeChangedInstances_)
	{
		PushUndoSnapshot();
	}

	strokeStartSnapshot_.clear();
	strokeChangedInstances_ = false;
}

void FoliagePanel::ApplyBrushAtCursor(bool force)
{
	BrushHit centerHit;
	if (!TryGetBrushHit(centerHit))
	{
		return;
	}

	const float strokeSpacing = GoknarMath::Max(brushRadius_ * 0.35f, minimumSpacing_);
	if (!force && hasLastStrokePoint_ && Vector3::SquareDistance(lastStrokePoint_, centerHit.position) < strokeSpacing * strokeSpacing)
	{
		return;
	}

	CellSet affectedCells;
	const bool changed = eraseMode_ ? EraseAtHit(centerHit, affectedCells) : PaintAtHit(centerHit, affectedCells);
	if (!changed)
	{
		return;
	}

	RebuildCells(affectedCells);
	EditorContext::Get()->MarkSceneDirty(eraseMode_ ? "Foliage erased" : "Foliage painted");
	strokeChangedInstances_ = true;
	lastStrokePoint_ = centerHit.position;
	hasLastStrokePoint_ = true;
}

bool FoliagePanel::PaintAtHit(const BrushHit& centerHit, CellSet& outAffectedCells)
{
	if (brushRadius_ <= 0.f || density_ <= 0.f || meshEntries_.empty())
	{
		return false;
	}

	const float brushArea = PI * brushRadius_ * brushRadius_;
	const int attemptCount = ClampPositiveInt(static_cast<int>(std::ceil(brushArea * density_)), 1, maxAttemptsPerBrushStep_);

	const Vector3 normal = centerHit.normal.SquareLength() > SMALLER_EPSILON ? centerHit.normal.GetNormalized() : Vector3::UpVector;
	Vector3 tangent = normal.GetOrthonormalBasis();
	if (tangent.SquareLength() <= SMALLER_EPSILON)
	{
		tangent = Vector3::ForwardVector;
	}
	tangent = tangent.GetNormalized();

	Vector3 bitangent = normal.Cross(tangent);
	if (bitangent.SquareLength() <= SMALLER_EPSILON)
	{
		bitangent = Vector3::LeftVector;
	}
	bitangent = bitangent.GetNormalized();

	bool placedAny = false;
	for (int attemptIndex = 0; attemptIndex < attemptCount; ++attemptIndex)
	{
		MeshEntry* entry = ChooseMeshEntry();
		if (!entry || !entry->mesh)
		{
			continue;
		}

		const float angle = RandomFloat(randomGenerator_, 0.f, TWO_PI);
		const float radius = std::sqrt(RandomFloat(randomGenerator_, 0.f, 1.f)) * brushRadius_;
		const Vector3 sample = centerHit.position +
			tangent * (std::cos(angle) * radius) +
			bitangent * (std::sin(angle) * radius);

		BrushHit placementHit;
		if (!Raycast(
			sample + normal * PlacementRaycastDistance,
			sample - normal * PlacementRaycastDistance,
			placementHit))
		{
			continue;
		}

		const Vector3 hitDelta = placementHit.position - centerHit.position;
		if (hitDelta.SquareLength() > brushRadius_ * brushRadius_ * 1.25f)
		{
			continue;
		}

		if (!IsFarEnoughFromExistingInstances(placementHit.position))
		{
			continue;
		}

		const Vector3 hitNormal = placementHit.normal.SquareLength() > SMALLER_EPSILON ?
			placementHit.normal.GetNormalized() :
			Vector3::UpVector;
		const Vector3 placementPosition = placementHit.position + hitNormal * entry->groundOffset;
		const float scale = RandomFloat(randomGenerator_, entry->minScale, entry->maxScale);

		Quaternion rotation = entry->alignToSurfaceNormal ?
			Quaternion::FromTwoVectors(Vector3::UpVector, hitNormal) :
			Quaternion::Identity;

		if (entry->randomYaw)
		{
			const Vector3 yawAxis = entry->alignToSurfaceNormal ? hitNormal : Vector3::UpVector;
			const Quaternion yawRotation(yawAxis, RandomFloat(randomGenerator_, 0.f, TWO_PI));
			rotation = yawRotation * rotation;
		}

		PlacedInstance placedInstance;
		placedInstance.meshPath = entry->meshPath;
		placedInstance.worldPosition = placementPosition;
		placedInstance.worldTransform = Matrix::GetTransformationMatrix(rotation, placementPosition, Vector3(scale));
		instances_.push_back(placedInstance);
		outAffectedCells.insert(GetCellCoord(placementPosition));
		placedAny = true;
	}

	return placedAny;
}

bool FoliagePanel::EraseAtHit(const BrushHit& centerHit, CellSet& outAffectedCells)
{
	const float eraseRadiusSquared = brushRadius_ * brushRadius_;
	const std::size_t originalCount = instances_.size();

	instances_.erase(
		std::remove_if(
			instances_.begin(),
			instances_.end(),
			[&](const PlacedInstance& instance)
			{
				if (Vector3::SquareDistance(instance.worldPosition, centerHit.position) > eraseRadiusSquared)
				{
					return false;
				}

				outAffectedCells.insert(GetCellCoord(instance.worldPosition));
				return true;
			}),
		instances_.end());

	return instances_.size() != originalCount;
}

FoliagePanel::MeshEntry* FoliagePanel::ChooseMeshEntry()
{
	float totalWeight = 0.f;
	for (MeshEntry& entry : meshEntries_)
	{
		if (!entry.enabled || entry.spawnWeight <= 0.f || entry.densityMultiplier <= 0.f)
		{
			continue;
		}

		if (!entry.mesh)
		{
			entry.mesh = ResolveMesh(entry.meshPath);
		}

		if (entry.mesh)
		{
			totalWeight += entry.spawnWeight * entry.densityMultiplier;
		}
	}

	if (totalWeight <= 0.f)
	{
		return nullptr;
	}

	const float chosenWeight = RandomFloat(randomGenerator_, 0.f, totalWeight);
	float accumulatedWeight = 0.f;
	for (MeshEntry& entry : meshEntries_)
	{
		if (!entry.enabled || !entry.mesh || entry.spawnWeight <= 0.f || entry.densityMultiplier <= 0.f)
		{
			continue;
		}

		accumulatedWeight += entry.spawnWeight * entry.densityMultiplier;
		if (chosenWeight <= accumulatedWeight)
		{
			return &entry;
		}
	}

	return nullptr;
}

bool FoliagePanel::IsFarEnoughFromExistingInstances(const Vector3& position) const
{
	if (minimumSpacing_ <= 0.f)
	{
		return true;
	}

	const float minimumSpacingSquared = minimumSpacing_ * minimumSpacing_;
	for (const PlacedInstance& instance : instances_)
	{
		if (Vector3::SquareDistance(instance.worldPosition, position) < minimumSpacingSquared)
		{
			return false;
		}
	}

	return true;
}

void FoliagePanel::SynchronizeFromScene()
{
	ClearRuntimeCells(false);
	instances_.clear();
	synchronizedScene_ = GetCurrentScene();
	hasSynchronizedScene_ = true;

	Scene* scene = synchronizedScene_;
	if (!scene)
	{
		return;
	}

	for (ObjectBase* object : scene->GetObjects())
	{
		if (!object || !StartsWith(object->GetNameWithoutId(), FoliageGridObjectPrefix))
		{
			continue;
		}

		const FoliageCellCoord coord = GetCellCoord(object->GetWorldPosition());
		CellRuntime& runtime = runtimeCells_[coord];
		runtime.object = object;

		const Matrix& cellMatrix = object->GetWorldTransformationMatrix();
		std::vector<InstancedStaticMeshComponent*> components = object->GetComponentsOfType<InstancedStaticMeshComponent>();
		for (InstancedStaticMeshComponent* component : components)
		{
			if (!component || !component->GetMeshInstance())
			{
				continue;
			}

			InstancedStaticMesh* instancedMesh = component->GetMeshInstance()->GetMesh();
			if (!instancedMesh)
			{
				GOKNAR_CORE_WARN("Foliage grid object %s has an instanced mesh component with no mesh.", object->GetNameWithoutId().c_str());
				continue;
			}

			const std::string meshPath = NormalizeMeshPath(
				instancedMesh->GetSourceMeshPath().empty() ? instancedMesh->GetPath() : instancedMesh->GetSourceMeshPath());
			if (meshPath.empty())
			{
				GOKNAR_CORE_WARN("Foliage grid object %s has an instanced mesh component with no source mesh path.", object->GetNameWithoutId().c_str());
				continue;
			}

			runtime.componentsByMeshPath[meshPath] = component;

			for (const Matrix& localTransform : instancedMesh->GetInstanceTransformationMatrices())
			{
				Matrix worldTransform = localTransform * cellMatrix;
				Vector3 translation = Vector3::ZeroVector;
				Vector3 scaling = Vector3(1.f);
				Quaternion rotation = Quaternion::Identity;
				worldTransform.Decompose(translation, scaling, rotation);

				PlacedInstance instance;
				instance.meshPath = meshPath;
				instance.worldPosition = translation;
				instance.worldTransform = worldTransform;
				instances_.push_back(instance);
			}
		}
	}
}

void FoliagePanel::ClearRuntimeCells(bool destroySceneObjects)
{
	if (!destroySceneObjects)
	{
		runtimeCells_.clear();
		return;
	}

	std::vector<FoliageCellCoord> cellCoords;
	cellCoords.reserve(runtimeCells_.size());
	for (const auto& cellPair : runtimeCells_)
	{
		cellCoords.push_back(cellPair.first);
	}

	for (const FoliageCellCoord& coord : cellCoords)
	{
		RemoveRuntimeCellObject(coord);
	}

	runtimeCells_.clear();
}

void FoliagePanel::RebuildAllCells()
{
	if (!hasSynchronizedScene_)
	{
		SynchronizeFromScene();
	}

	ClearRuntimeCells(true);

	CellSet cells;
	for (const PlacedInstance& instance : instances_)
	{
		cells.insert(GetCellCoord(instance.worldPosition));
	}

	RebuildCells(cells);
}

void FoliagePanel::RebuildCells(const CellSet& cells)
{
	for (const FoliageCellCoord& coord : cells)
	{
		RebuildCell(coord);
	}
}

void FoliagePanel::RebuildCell(const FoliageCellCoord& coord)
{
	std::unordered_map<std::string, std::vector<Matrix>> transformsByMeshPath;
	const Vector3 cellOrigin = GetCellOrigin(coord);

	for (const PlacedInstance& instance : instances_)
	{
		if (!(GetCellCoord(instance.worldPosition) == coord))
		{
			continue;
		}

		Vector3 translation = Vector3::ZeroVector;
		Vector3 scaling = Vector3(1.f);
		Quaternion rotation = Quaternion::Identity;
		instance.worldTransform.Decompose(translation, scaling, rotation);
		transformsByMeshPath[instance.meshPath].push_back(
			Matrix::GetTransformationMatrix(rotation, translation - cellOrigin, scaling));
	}

	if (transformsByMeshPath.empty())
	{
		RemoveRuntimeCellObject(coord);
		return;
	}

	CellRuntime& runtime = GetOrCreateRuntimeCell(coord);
	if (!runtime.object)
	{
		return;
	}

	std::unordered_set<std::string> activeMeshPaths;
	for (const auto& transformsPair : transformsByMeshPath)
	{
		const std::string& meshPath = transformsPair.first;
		InstancedStaticMeshComponent* component = GetOrCreateInstancedComponent(runtime, coord, meshPath);
		if (!component || !component->GetMeshInstance())
		{
			continue;
		}

		InstancedStaticMesh* instancedMesh = component->GetMeshInstance()->GetMesh();
		if (!instancedMesh)
		{
			continue;
		}

		instancedMesh->SetInstanceTransformations(transformsPair.second);
		instancedMesh->UpdateAllTransforms();
		component->SetIsActive(true);
		activeMeshPaths.insert(meshPath);
	}

	if (activeMeshPaths.empty())
	{
		RemoveRuntimeCellObject(coord);
		return;
	}

	for (auto& componentPair : runtime.componentsByMeshPath)
	{
		if (activeMeshPaths.find(componentPair.first) != activeMeshPaths.end())
		{
			continue;
		}

		InstancedStaticMeshComponent* component = componentPair.second;
		InstancedStaticMesh* instancedMesh =
			component && component->GetMeshInstance() ? component->GetMeshInstance()->GetMesh() : nullptr;
		if (instancedMesh)
		{
			// TODO: Remove this component when ObjectBase exposes public component removal.
			instancedMesh->SetInstanceTransformations({});
			instancedMesh->UpdateAllTransforms();
		}
		if (component)
		{
			component->SetIsActive(false);
		}
	}

	engine->GetResourceManager()->InitializePendingMaterials();
	engine->InitializePendingObjectsAndComponents();
}

void FoliagePanel::RemoveRuntimeCellObject(const FoliageCellCoord& coord)
{
	auto runtimeIterator = runtimeCells_.find(coord);
	if (runtimeIterator == runtimeCells_.end())
	{
		return;
	}

	ObjectBase* object = runtimeIterator->second.object;
	Scene* scene = GetCurrentScene();
	if (scene && object)
	{
		scene->RemoveObject(object);
	}
	if (object)
	{
		object->Destroy();
	}

	runtimeCells_.erase(runtimeIterator);
	if (engine)
	{
		engine->FlushPendingDestroy();
	}
}

FoliagePanel::CellRuntime& FoliagePanel::GetOrCreateRuntimeCell(const FoliageCellCoord& coord)
{
	CellRuntime& runtime = runtimeCells_[coord];
	if (runtime.object)
	{
		runtime.object->SetWorldPosition(GetCellOrigin(coord), false);
		runtime.object->SetWorldRotation(Quaternion::Identity, false);
		runtime.object->SetWorldScaling(Vector3(1.f));
		runtime.object->SetName(GetCellObjectName(coord));
		return runtime;
	}

	Scene* scene = GetCurrentScene();
	if (!scene)
	{
		return runtime;
	}

	ObjectBase* object = new ObjectBase();
	object->SetName(GetCellObjectName(coord));
	object->SetWorldPosition(GetCellOrigin(coord), false);
	object->SetWorldRotation(Quaternion::Identity, false);
	object->SetWorldScaling(Vector3(1.f));
	scene->AddObject(object);
	runtime.object = object;

	return runtime;
}

InstancedStaticMeshComponent* FoliagePanel::GetOrCreateInstancedComponent(
	CellRuntime& runtime,
	const FoliageCellCoord& coord,
	const std::string& meshPath)
{
	auto componentIterator = runtime.componentsByMeshPath.find(meshPath);
	if (componentIterator != runtime.componentsByMeshPath.end() && componentIterator->second)
	{
		return componentIterator->second;
	}

	if (!runtime.object)
	{
		return nullptr;
	}

	StaticMesh* sourceMesh = ResolveMesh(meshPath);
	if (!sourceMesh)
	{
		GOKNAR_CORE_WARN("Skipping foliage mesh %s because it could not be loaded.", meshPath.c_str());
		return nullptr;
	}

	InstancedStaticMesh* instancedMesh = InstancedStaticMesh::CreateFromStaticMesh(
		sourceMesh,
		sourceMesh->GetPath() + "::Foliage_" + std::to_string(coord.x) + "_" + std::to_string(coord.y) + "_" + std::to_string(coord.z));
	if (!instancedMesh)
	{
		GOKNAR_CORE_WARN("Failed to create instanced foliage mesh for %s.", meshPath.c_str());
		return nullptr;
	}

	instancedMesh->PreInit();
	instancedMesh->Init();
	instancedMesh->PostInit();

	InstancedStaticMeshComponent* component = runtime.object->AddSubComponent<InstancedStaticMeshComponent>();
	component->SetMesh(instancedMesh);
	runtime.componentsByMeshPath[meshPath] = component;
	return component;
}

FoliageCellCoord FoliagePanel::GetCellCoord(const Vector3& position) const
{
	const float safeGridSize = GoknarMath::Max(1.f, gridSize_);
	return FoliageCellCoord
	{
		static_cast<int>(std::floor(position.x / safeGridSize)),
		static_cast<int>(std::floor(position.y / safeGridSize)),
		static_cast<int>(std::floor(position.z / safeGridSize))
	};
}

Vector3 FoliagePanel::GetCellOrigin(const FoliageCellCoord& coord) const
{
	const float safeGridSize = GoknarMath::Max(1.f, gridSize_);
	return Vector3(
		static_cast<float>(coord.x) * safeGridSize,
		static_cast<float>(coord.y) * safeGridSize,
		static_cast<float>(coord.z) * safeGridSize);
}

std::string FoliagePanel::GetCellObjectName(const FoliageCellCoord& coord) const
{
	return std::string(FoliageGridObjectPrefix) +
		"_X" + std::to_string(coord.x) +
		"_Y" + std::to_string(coord.y) +
		"_Z" + std::to_string(coord.z);
}

std::string FoliagePanel::NormalizeMeshPath(const std::string& path) const
{
	const std::string strippedPath = StripInstancedMeshSuffix(path);
	return EditorAssetPathUtils::NormalizePath(EditorAssetPathUtils::ToContentRelativePath(strippedPath));
}

StaticMesh* FoliagePanel::ResolveMesh(const std::string& meshPath) const
{
	if (!engine || !engine->GetResourceManager() || meshPath.empty())
	{
		return nullptr;
	}

	return engine->GetResourceManager()->GetContent<StaticMesh>(NormalizeMeshPath(meshPath));
}

std::size_t FoliagePanel::GetInstanceCount() const
{
	return instances_.size();
}

Scene* FoliagePanel::GetCurrentScene() const
{
	return engine && engine->GetApplication() ? engine->GetApplication()->GetMainScene() : nullptr;
}

void FoliagePanel::PushUndoSnapshot()
{
	undoStack_.push_back(strokeStartSnapshot_);
	if (undoStack_.size() > MaxUndoSnapshots)
	{
		undoStack_.erase(undoStack_.begin());
	}

	redoStack_.clear();
}

void FoliagePanel::ApplySnapshot(const std::vector<PlacedInstance>& snapshot)
{
	instances_ = snapshot;
	RebuildAllCells();
	EditorContext::Get()->MarkSceneDirty("Foliage changed");
}

void FoliagePanel::UndoStroke()
{
	if (undoStack_.empty())
	{
		return;
	}

	redoStack_.push_back(instances_);
	const std::vector<PlacedInstance> snapshot = undoStack_.back();
	undoStack_.pop_back();
	ApplySnapshot(snapshot);
}

void FoliagePanel::RedoStroke()
{
	if (redoStack_.empty())
	{
		return;
	}

	undoStack_.push_back(instances_);
	const std::vector<PlacedInstance> snapshot = redoStack_.back();
	redoStack_.pop_back();
	ApplySnapshot(snapshot);
}
