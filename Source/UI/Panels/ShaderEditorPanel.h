#pragma once

#include "EditorPanel.h"

#include "Goknar/Math/GoknarMath.h"
#include "Goknar/Materials/Material.h"

#include "imgui.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>

enum class ShaderPinType
{
    Float,
    Vector2,
    Vector3,
    Vector4,
    Vector4i,
    Matrix4x4,
    Texture
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
    
    // Default value if nothing is connected
    std::variant<float, Vector2, Vector3, Vector4, std::string> defaultValue = 0.0f;
};

struct ShaderNode
{
    int id;
    std::string name;
    std::string typeCategory; // e.g., "Math", "Constants", "Master"
    ImVec2 pos;
    ImVec2 size;
    
    std::vector<ShaderPin> inputs;
    std::vector<ShaderPin> outputs;
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
    virtual ~ShaderEditorPanel() = default;

    virtual void Init() override;
    virtual void Draw() override;

    // The crucial function: Translates the visual graph into GLSL strings for ShaderBuilder
    void CompileGraphToMaterial(MaterialInitializationData* outMaterialData);

private:
    void DrawNodeCanvas();
    void DrawPropertiesSidebar();

    ShaderNode SpawnNode(const std::string& category, const std::string& name, ImVec2 pos);

    ShaderNode CreateVariableNode(const std::string& name, ShaderPinType type, ImVec2 pos);
    void PopulateVariableNodes();

    // Canvas Helpers
    ImVec2 GetPinPosition(int pinId);
    ShaderPin* FindPin(int pinId);
    ShaderNode* FindNode(int nodeId);

    std::vector<ShaderNode> nodes_;
    std::vector<ShaderLink> links_;

    Material* activeMaterial_{ nullptr };

    int nextId_{ 1 };
    
    // Canvas state
    ImVec2 scrolling_{ 0.0f, 0.0f };
    float scale_{ 1.0f };
    
    bool isDraggingLink_{ false };
    int draggingStartPinId_{ -1 };
    int selectedNodeId_{ -1 };
    int selectedLinkId_{ -1 };
    
    int masterNodeId_{ -1 };
};