#include "MeshViewerPanel.h"

#include "imgui.h"

#include "Goknar/Camera.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Components/SkeletalMeshComponent.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Renderer/RenderTarget.h"
#include "Goknar/Renderer/Texture.h"

#include "Objects/MeshViewerCameraObject.h"
#include "Controllers/MeshViewerCameraController.h"
#include "UI/EditorUtils.h"

constexpr unsigned int MESH_VIEWER_RENDER_MASK = 0x80000000;

MeshViewerPanel::MeshViewerPanel(EditorHUD* hud) : 
	IEditorPanel("Mesh Viewer", hud),
	size_(1024, 1024)
{
	cameraObject_ = new MeshViewerCameraObject();
	cameraObject_->SetName("__Editor__MeshViewerCamera");
	cameraObject_->GetCameraComponent()->GetCamera()->SetCameraType(CameraType::RenderTarget);
	cameraObject_->GetCameraComponent()->GetCamera()->SetRenderMask(MESH_VIEWER_RENDER_MASK);
	cameraObject_->SetWorldPosition({ 0.f, 0.f, 1000.f });

	renderTarget_ = new RenderTarget();
	renderTarget_->SetCamera(cameraObject_->GetCameraComponent()->GetCamera());
	renderTarget_->SetFrameSize(size_);
	renderTarget_->SetRerenderShadowMaps(false);

	viewedObject_ = new ObjectBase();
	viewedObject_->SetName("__Editor__MeshViewerTarget");
	viewedObject_->SetWorldPosition(Vector3::ZeroVector);

	staticMeshComponent_ = viewedObject_->AddSubComponent<StaticMeshComponent>();
	staticMeshComponent_->GetMeshInstance()->SetRenderMask(MESH_VIEWER_RENDER_MASK);

	isOpen_ = false;
}

MeshViewerPanel::~MeshViewerPanel()
{
	delete renderTarget_;
	cameraObject_->Destroy();
	viewedObject_->Destroy();
}

void MeshViewerPanel::Init()
{
	renderTarget_->Init();
}

void MeshViewerPanel::Draw()
{
	if (!ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		cameraObject_->GetController()->SetIsActive(false);
		return;
	}

	ImVec2 newViewportSize = ImGui::GetContentRegionAvail();

	if (newViewportSize.x <= 0.0f || newViewportSize.y <= 0.0f)
	{
		return;
	}

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

	ImGui::End();
}

void MeshViewerPanel::SetTargetStaticMesh(StaticMesh* staticMesh)
{
	staticMeshComponent_->SetMesh(staticMesh);
	staticMeshComponent_->SetIsActive(true);
	staticMeshComponent_->GetMeshInstance()->GetMaterial()->SetEmmisiveColor({ 0.2f });
	
	if (staticMesh)
	{
		cameraObject_->GetController()->ResetViewWithBoundingBox(viewedObject_, staticMesh->GetAABB());
	}
}