#pragma once

#include "EditorPanel.h"

#include "Goknar/Math/GoknarMath.h"
#include "Goknar/Materials/Material.h"

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

enum class ShaderPinType
{
    None = 0,
    Float,
    Vector2,
    Vector3,
    Vector4,
    Vector4i,
    Matrix4x4,
    Texture,
    Any
};

enum class ShaderPinKind
{
    Input,
    Output
};

struct ShaderPin
{
    int id;
    int nodeId;
    std::string name;
    ShaderPinType type;
    ShaderPinKind kind;

    std::variant<float, Vector2, Vector3, Vector4, std::string> defaultValue = 0.0f;
};

struct ShaderNode
{
    int id;
    std::string name;
    std::string typeCategory;
    ImVec2 pos;
    ImVec2 size;

    std::vector<ShaderPin> inputs;
    std::vector<ShaderPin> outputs;

    std::string stringData{ "" };
};

struct ShaderLink
{
    int id;
    int startPinId; // Always an Output pin
    int endPinId;   // Always an Input pin
};

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
    void OnMaterialSaved(const std::string& path);

private:
    struct TextureInfo {
        std::string name;
        std::string path;
    };

    void DrawNodeCanvas();
    void DrawPropertiesSidebar();
    void DrawPreview();
    void DrawMaterialProperties();

    ShaderNode SpawnNode(const std::string& category, const std::string& name, ImVec2 pos);

    ShaderNode CreateVariableNode(const std::string& name, ShaderPinType type, ImVec2 pos);

    ImVec2 GetPinPosition(int pinId);
    ShaderPin* FindPin(int pinId);
    ShaderNode* FindNode(int nodeId);

    std::vector<ShaderNode> nodes_;
    std::vector<ShaderLink> links_;

    Material* activeMaterial_{ nullptr };

    int nextId_{ 1 };

    ImVec2 scrolling_{ 0.0f, 0.0f };
    float scale_{ 1.0f };

    bool isDraggingLink_{ false };
    int draggingStartPinId_{ -1 };
    int selectedNodeId_{ -1 };
    int selectedLinkId_{ -1 };

    int masterNodeId_{ -1 };

    std::unordered_set<int> selectedNodeIds_;

    std::vector<ShaderNode> clipboardNodes_;
    std::vector<ShaderLink> clipboardLinks_;

    bool isDraggingSelection_{ false };
    ImVec2 selectionStart_{ 0.0f, 0.0f };
    std::unordered_set<int> preDragSelection_;

    RenderTarget* renderTarget_{ nullptr };
    MeshViewerCameraObject* cameraObject_{ nullptr };
    ObjectBase* viewedObject_{ nullptr };
    StaticMeshComponent* staticMeshComponent_{ nullptr };
    Vector2 previewSize_{ 300.f, 300.f };
};