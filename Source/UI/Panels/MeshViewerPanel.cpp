#include "MeshViewerPanel.h"

#include <filesystem>

#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Helpers/ContentPathUtils.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Materials/MaterialInstance.h"
#include "Goknar/Materials/MaterialSerializer.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Model/StaticMeshInstance.h"

constexpr unsigned int MESH_VIEWER_RENDER_MASK = 0x10000000;

namespace
{
bool DoesMaterialAssetExist(const std::string& materialPath)
{
    const std::string relativeMaterialPath = ContentPathUtils::ToContentRelativePath(materialPath);
    if (relativeMaterialPath.empty())
    {
        return false;
    }

    return std::filesystem::exists(ContentPathUtils::ToAbsoluteContentPath(relativeMaterialPath));
}
}

MeshViewerPanel::MeshViewerPanel(EditorHUD* hud) :
    MeshAssetViewerPanelBase("Mesh Viewer", hud, "__Editor__MeshViewerCamera", "__Editor__MeshViewerTarget", MESH_VIEWER_RENDER_MASK, "MeshViewerViewportArea", "MeshViewerPropertiesArea")
{
    staticMeshComponent_ = viewedObject_->AddSubComponent<StaticMeshComponent>();
    staticMeshComponent_->GetMeshInstance()->SetRenderMask(MESH_VIEWER_RENDER_MASK);
}

MeshViewerPanel::~MeshViewerPanel() = default;

void MeshViewerPanel::SetTargetStaticMesh(StaticMesh* staticMesh)
{
    staticMeshComponent_->SetMesh(staticMesh);
    staticMeshComponent_->SetIsActive(staticMesh != nullptr);
    StaticMeshInstance* meshInstance = staticMeshComponent_->GetMeshInstance();
    if (meshInstance)
    {
        meshInstance->SetRenderMask(MESH_VIEWER_RENDER_MASK);
    }

    OnTargetMeshChanged();
}

bool MeshViewerPanel::HasCurrentMesh() const
{
    StaticMesh* staticMesh = GetCurrentStaticMesh();
    return staticMesh && !staticMesh->GetSubMeshes().empty();
}

std::string MeshViewerPanel::GetCurrentMeshPath() const
{
    StaticMesh* staticMesh = GetCurrentStaticMesh();
    return staticMesh ? staticMesh->GetPath() : "";
}

const Box* MeshViewerPanel::GetCurrentMeshBounds() const
{
    StaticMesh* staticMesh = GetCurrentStaticMesh();
    return staticMesh ? &staticMesh->GetAABB() : nullptr;
}

size_t MeshViewerPanel::GetSubMeshCount() const
{
    StaticMesh* staticMesh = GetCurrentStaticMesh();
    return staticMesh ? staticMesh->GetSubMeshes().size() : 0;
}

std::string MeshViewerPanel::GetSubMeshName(size_t subMeshIndex) const
{
    StaticMesh* staticMesh = GetCurrentStaticMesh();
    if (!staticMesh || subMeshIndex >= staticMesh->GetSubMeshes().size())
    {
        return "";
    }

    return staticMesh->GetSubMeshes()[subMeshIndex]->GetName();
}

bool MeshViewerPanel::RebuildCurrentMaterial(size_t subMeshIndex, const std::string& materialPath)
{
    StaticMesh* staticMesh = GetCurrentStaticMesh();
    if (!staticMesh || subMeshIndex >= staticMesh->GetSubMeshes().size())
    {
        return false;
    }

    auto* subMesh = staticMesh->GetSubMeshes()[subMeshIndex];
    Material* material = subMesh->GetMaterial();

    if (!material)
    {
        return false;
    }

    if (!DoesMaterialAssetExist(materialPath))
    {
        return false;
    }

    material->ResetForRebuild();
    MaterialSerializer::Deserialize(materialPath, material);
    material->Build(subMesh);

    if (subMesh->GetIsInitialized())
    {
        material->PreInit();
        material->Init();
        material->PostInit();
    }

    return true;
}

MaterialInstance* MeshViewerPanel::CreatePreviewMaterialInstance(size_t subMeshIndex) const
{
    StaticMesh* staticMesh = GetCurrentStaticMesh();
    if (!staticMesh || subMeshIndex >= staticMesh->GetSubMeshes().size())
    {
        return nullptr;
    }

    Material* material = staticMesh->GetSubMeshes()[subMeshIndex]->GetMaterial();
    return material ? MaterialInstance::Create(material) : nullptr;
}

void MeshViewerPanel::SetPreviewMaterial(size_t subMeshIndex, MaterialInstance* materialInstance)
{
    staticMeshComponent_->GetMeshInstance()->SetMaterial(static_cast<int>(subMeshIndex), materialInstance);
}

const char* MeshViewerPanel::GetNoMeshSelectedText() const
{
    return "No mesh selected.";
}

StaticMesh* MeshViewerPanel::GetCurrentStaticMesh() const
{
    return staticMeshComponent_ ? staticMeshComponent_->GetMeshInstance()->GetMesh() : nullptr;
}
