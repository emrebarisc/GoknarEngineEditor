#include "pch.h"

#include "SkeletalMeshViewerPanel.h"

#include <cmath>
#include <algorithm>

#include "imgui.h"

#include "Goknar/Camera.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Components/SkeletalMeshComponent.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Renderer/RenderTarget.h"
#include "Goknar/Renderer/Texture.h"
#include "Goknar/Materials/MaterialBase.h"

#include "Objects/MeshViewerCameraObject.h"
#include "Controllers/MeshViewerCameraController.h"
#include "UI/EditorUtils.h"

#include "Model/SkeletalMesh.h"
#include "Model/SkeletalMeshInstance.h"

constexpr unsigned int SKELETAL_MESH_VIEWER_RENDER_MASK = 0x40000000;

SkeletalMeshViewerPanel::SkeletalMeshViewerPanel(EditorHUD* hud) :
	IEditorPanel("Skeletal Mesh Viewer", hud),
	size_(1024, 1024)
{
	cameraObject_ = new MeshViewerCameraObject();
	cameraObject_->SetName("__Editor__SkeletalMeshViewerCamera");
	cameraObject_->GetCameraComponent()->GetCamera()->SetCameraType(CameraType::RenderTarget);
	cameraObject_->GetCameraComponent()->GetCamera()->SetRenderMask(SKELETAL_MESH_VIEWER_RENDER_MASK);
	cameraObject_->SetWorldPosition({ 0.f, 0.f, 1000.f });

	renderTarget_ = new RenderTarget();
	renderTarget_->SetCamera(cameraObject_->GetCameraComponent()->GetCamera());
	renderTarget_->SetFrameSize(size_);
	renderTarget_->SetRerenderShadowMaps(false);

	viewedObject_ = new ObjectBase();
	viewedObject_->SetName("__Editor__SkeletalMeshViewerTarget");
	viewedObject_->SetWorldPosition(Vector3::ZeroVector);

	skeletalMeshComponent_ = viewedObject_->AddSubComponent<SkeletalMeshComponent>();

	isOpen_ = false;
}

SkeletalMeshViewerPanel::~SkeletalMeshViewerPanel()
{
	delete renderTarget_;
	cameraObject_->Destroy();
	viewedObject_->Destroy();
}

void SkeletalMeshViewerPanel::Init()
{
	renderTarget_->Init();
}

void SkeletalMeshViewerPanel::Draw()
{
	if (!ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		cameraObject_->GetController()->SetIsActive(false);
		return;
	}

	ImVec2 contentAvail = ImGui::GetContentRegionAvail();
	float animationListWidth = 250.0f; // Fixed width for the animation list
	float viewportWidth = contentAvail.x - animationListWidth - ImGui::GetStyle().ItemSpacing.x;

	// =========================================================
	// 1. VIEWPORT AREA (LEFT)
	// =========================================================
	ImGui::BeginChild("ViewportArea", ImVec2(viewportWidth, contentAvail.y), true);
	ImVec2 newViewportSize = ImGui::GetContentRegionAvail();

	if (newViewportSize.x > 0.0f && newViewportSize.y > 0.0f)
	{
		if (size_.x != newViewportSize.x || size_.y != newViewportSize.y)
		{
			size_ = EditorUtils::ToVector2(newViewportSize);
			renderTarget_->SetFrameSize({ newViewportSize.x, newViewportSize.y });
		}

		Texture* renderTargetTexture = renderTarget_->GetTexture();
		ImGui::Image(
			(ImTextureID)(intptr_t)renderTargetTexture->GetRendererTextureId(),
			EditorUtils::ToImVec2(size_),
			ImVec2{ 0.f, 1.f },
			ImVec2{ 1.f, 0.f }
		);

		bool isHovered = ImGui::IsItemHovered() || ImGui::IsWindowHovered();
		cameraObject_->GetController()->SetIsActive(isHovered);

		EditorUtils::DrawWorldAxis(cameraObject_->GetCameraComponent()->GetCamera());
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// =========================================================
	// 2. ANIMATION LIST AREA (RIGHT)
	// =========================================================
	ImGui::BeginChild("AnimationListArea", ImVec2(animationListWidth, contentAvail.y), true);

	ImGui::Text("Available Animations");
	ImGui::Separator();

	SkeletalMeshInstance* meshInstance = skeletalMeshComponent_->GetMeshInstance();
	if (meshInstance && meshInstance->GetMesh())
	{
		SkeletalMesh* skeletalMesh = meshInstance->GetMesh();
		const auto& animationsMap = skeletalMesh->GetAnimationsMap();

		if (animationsMap.empty())
		{
			ImGui::TextDisabled("No animations found.");
		}
		else
		{
			for (const auto& pair : animationsMap)
			{
				const std::string& animName = pair.first;

				// Check if this animation is the currently playing one
				bool isSelected = false;
				const auto& currentAnimData = meshInstance->GetSkeletalMeshAnimation();
				if (currentAnimData.skeletalAnimation && currentAnimData.skeletalAnimation->name == animName)
				{
					isSelected = true;
				}

				if (ImGui::Selectable(animName.c_str(), isSelected))
				{
					meshInstance->PlayAnimation(animName);
				}
			}
		}
	}
	else
	{
		ImGui::TextDisabled("No Skeletal Mesh selected.");
	}

	ImGui::EndChild();

	ImGui::End();
}


void SkeletalMeshViewerPanel::SetTargetSkeletalMesh(SkeletalMesh* skeletalMesh)
{
	skeletalMeshComponent_->SetMesh(skeletalMesh);
	skeletalMeshComponent_->SetIsActive(true);

	SkeletalMeshInstance* meshInstance = skeletalMeshComponent_->GetMeshInstance();
	if (meshInstance)
	{
		meshInstance->SetRenderMask(SKELETAL_MESH_VIEWER_RENDER_MASK);

		if (meshInstance->GetMaterial())
		{
			meshInstance->GetMaterial()->SetEmmisiveColor({ 0.2f });
		}
	}

	if (skeletalMesh)
	{
		cameraObject_->GetController()->ResetViewWithBoundingBox(viewedObject_, skeletalMesh->GetAABB());
	}
}