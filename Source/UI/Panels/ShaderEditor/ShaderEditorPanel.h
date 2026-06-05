#pragma once

#include "UI/Panels/EditorPanel.h"

#include "Goknar/Math/GoknarMath.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Materials/MaterialBase.h"
#include "Goknar/Materials/MaterialFunction.h"
#include "ShaderEditorTypes.h"

#include "imgui.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <variant>

class RenderTarget;
class MeshViewerCameraObject;
class ObjectBase;
class StaticMeshComponent;
class Texture;

class EditorHUD;

class ShaderEditorPanel : public IEditorPanel
{
public:
    ShaderEditorPanel(EditorHUD* hud);
    virtual ~ShaderEditorPanel();

    virtual void Init() override;
    virtual void Draw() override;

    void CompileGraphToMaterial(MaterialInitializationData* outMaterialData);

    void OnMaterialOpened(const std::string& path);
    void OnMaterialFunctionOpened(const std::string& path);
    void OnMaterialSaved(const std::string& path);
    void OnMaterialFunctionSaved(const std::string& path);
    void SaveCurrentMaterial();
    void SaveCurrentAsset();
    bool HasCurrentAssetPath() const { return !currentAssetPath_.empty(); }

private:
    using TextureInfo = ShaderEditorTextureInfo;

    struct ShaderEditorSnapshot
    {
        ShaderEditorAssetMode assetMode{ ShaderEditorAssetMode::Material };
        std::string assetPath{};
        std::vector<ShaderNode> nodes{};
        std::vector<ShaderLink> links{};
        std::vector<TextureInfo> textures{};
        std::unordered_set<int> selectedNodeIds{};
        float ambientOcclusion{ 1.f };
        float metallic{ 0.f };
        float roughness{ 0.5f };
        bool usesReflectionProbe{ false };
        ImVec2 scrolling{ 0.0f, 0.0f };
        float translucency{ 0.f };
        float scale{ 1.f };
        int selectedNodeId{ -1 };
        int selectedLinkId{ -1 };
        int masterNodeId{ -1 };
        int nextId{ 1 };
        MaterialBlendModel blendModel{ MaterialBlendModel::Opaque };
        MaterialShadingModel shadingModel{ MaterialShadingModel::Default };
        MaterialShadingType shadingType{ MaterialShadingType::Default };
    };

    void ResetEditorGraph(const ImVec2& masterNodePos);
    void ResetInteractionState();
    void SetAssetMode(ShaderEditorAssetMode assetMode);
    void CaptureSnapshotForNavigation();
    bool RestorePreviousSnapshot();
    ShaderEditorSnapshot CaptureSnapshot() const;
    void RestoreSnapshot(const ShaderEditorSnapshot& snapshot);
    void RebuildActiveMaterialFromGraph();
    void RefreshTextureBindingsFromNodes();
    void MigrateLegacyTextureSampleNodes();
    std::string GetTextureSamplerNameForPath(const std::string& texturePath) const;
    std::string GetTextureSamplerNameForNode(const ShaderNode& textureNode) const;
    const TextureInfo* GetTextureInfoForNode(const ShaderNode& textureNode) const;
    std::string GetTextureSampleUVExpression(const ShaderNode& textureNode, const std::string& uvExpression, ShaderPinType uvType) const;
    Texture* GetTexturePreviewTexture(const std::string& texturePath, bool allowStandaloneCreation) const;
    void OnTextureAssetSelected(const std::string& path);
    bool BuildActiveMaterialFunction(MaterialFunction& outMaterialFunction);
    void SaveEditorReflection(const std::string& assetPath);
    bool LoadEditorReflection(const std::string& assetPath);
    void ApplyHierarchicalLayout();
    bool LoadMaterialFunctionSignature(const std::string& assetPath, MaterialFunction& outMaterialFunction) const;
    void ApplyMaterialFunctionSignatureToGraph(const MaterialFunction& materialFunction);
    void RefreshMaterialFunctionCallNodeSignature(ShaderNode& node);
    ShaderNode CreateMaterialFunctionCallNode(const std::string& assetPath, ImVec2 pos);
    std::vector<std::string> GetMaterialFunctionAssetPaths() const;
    void OpenMaterialFunctionFromNode(const std::string& assetPath);
    const char* GetCurrentEditorReflectionFileType() const;
    bool IsEditingMaterial() const { return activeAssetMode_ == ShaderEditorAssetMode::Material; }
    bool IsEditingMaterialFunction() const { return activeAssetMode_ == ShaderEditorAssetMode::MaterialFunction; }
    std::string GetOpenButtonLabel() const;
    std::string GetSaveButtonLabel() const;
    std::string GetBackButtonLabel() const;

    void DrawNodeCanvas();
    void DrawPropertiesSidebar();
    void DrawPreview();
    void DrawMaterialProperties();
    void DrawMaterialFunctionProperties();

    ShaderNode SpawnNode(const std::string& category, const std::string& name, ImVec2 pos);
    ShaderNode CreateMaterialVariableAccessorNode(const ShaderNode& declarationNode, bool createSetter, ImVec2 pos);
    void SynchronizeMaterialVariableAccessorNodes();
    void RenameMaterialVariableAccessorReferences(const std::string& previousName, const std::string& newName);

    ShaderNode CreateVariableNode(const std::string& name, ShaderPinType type, ImVec2 pos);

    ImVec2 GetPinPosition(int pinId);
    ShaderPin* FindPin(int pinId);
    ShaderNode* FindNode(int nodeId);

    std::vector<ShaderNode> nodes_;
    std::vector<ShaderLink> links_;

    std::unordered_set<int> selectedNodeIds_;
    std::unordered_set<int> preDragSelection_;

    std::vector<ShaderNode> clipboardNodes_;
    std::vector<ShaderLink> clipboardLinks_;
    std::vector<TextureInfo> textures_;

    float ambientOcclusion_{ 1.f };
    float metallic_{ 0.f };
    float roughness_{ 0.5f };
    bool usesReflectionProbe_{ false };

    Vector2 previewSize_{ 300.f, 300.f };

    ImVec2 scrolling_{ 0.0f, 0.0f };
    ImVec2 selectionStart_{ 0.0f, 0.0f };

    RenderTarget* renderTarget_{ nullptr };

    MeshViewerCameraObject* cameraObject_{ nullptr };

    ObjectBase* viewedObject_{ nullptr };

    StaticMeshComponent* staticMeshComponent_{ nullptr };

    EditorHUD* editorHUD_{ nullptr };

    Material* activeMaterial_{ nullptr };

    ShaderEditorAssetMode activeAssetMode_{ ShaderEditorAssetMode::Material };
    std::string currentAssetPath_{};
    std::vector<ShaderEditorSnapshot> navigationStack_{};

    float translucency_{ 0.f };
    float scale_{ 1.0f };

    int autoConnectStartPinId_{ -1 };
    int draggingStartPinId_{ -1 };
    int selectedNodeId_{ -1 };
    int selectedLinkId_{ -1 };
    int textureSelectionNodeId_{ -1 };
    int masterNodeId_{ -1 };
    int nextId_{ 1 };

    bool isDraggingSelection_{ false };
    bool isBackgroundClicked_{ false };
    bool isDraggingLink_{ false };

    MaterialBlendModel blendModel_{ MaterialBlendModel::Opaque };
    MaterialShadingType shadingType_{ MaterialShadingType::Default };

    MaterialShadingModel shadingModel_{ MaterialShadingModel::Default };
};
