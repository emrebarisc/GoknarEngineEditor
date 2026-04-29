#pragma once

#include "EditorPanel.h"

#include "Goknar/Animation/AnimationNode.h"
#include "Goknar/Animation/AnimationState.h"
#include "Goknar/Animation/AnimationTransition.h"

#include "imgui.h"

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

struct EditorAnimationNode
{
    int id;
    std::string name;
    ImVec2 pos, size;
    std::shared_ptr<AnimationNode> animNode;
};

struct EditorAnimationState
{
    int id;
    std::string name;
    ImVec2 pos, size;
    std::shared_ptr<AnimationState> animState;
};

struct EditorAnimationNodeLink
{
    int id;
    int startId;
    int endId;
    std::shared_ptr<AnimationTransition<AnimationNode>> transition;
};

struct EditorAnimationStateLink
{
    int id;
    int startId;
    int endId;
    std::shared_ptr<AnimationTransition<AnimationState>> transition;
};

enum class EditorAnimationSelectionType
{
    None, State, Node, StateLink, NodeLink
};

enum class EditorAnimationViewMode
{
    StateMachine, InsideState
};

class EditorHUD;
struct AnimationGraph;

class AnimationGraphPanel : public IEditorPanel
{
public:
    AnimationGraphPanel(EditorHUD* hud);
    virtual ~AnimationGraphPanel();

    virtual void Init() override;
    virtual void Draw() override;

    void ResetToDefaultGraph();
    void OpenAnimationGraph(const std::string& path);
    void SaveAnimationGraph(const std::string& path);
    void SaveCurrentAnimationGraph();
    bool HasCurrentGraphPath() const { return !currentGraphPath_.empty(); }
    const std::string& GetCurrentGraphPath() const { return currentGraphPath_; }

private:
    void SaveGraphToXML(const std::string& filepath);
    void LoadGraphFromXML(const std::string& filepath);
    void SaveEditorReflection(const std::string& assetPath);
    bool LoadEditorReflection(const std::string& assetPath);
    void AutoLayoutEditorGraph();
    void RebuildEditorGraphFromRuntimeData();

    void DrawPropertiesPanel();
    void DrawNodeCanvas();
    void DrawDetailsSidebar();
    void DrawVariablesSidebar();
    void DrawHierarchySidebar();

    void CollectNodes(const std::shared_ptr<AnimationNode>& node, std::unordered_set<const AnimationNode*>& visited, std::vector<std::shared_ptr<AnimationNode>>& outNodes);

    std::shared_ptr<AnimationGraph> activeGraph_;

    std::vector<EditorAnimationState> editorStates_;
    std::vector<EditorAnimationStateLink> stateLinks_;

    std::unordered_map<const AnimationState*, std::vector<EditorAnimationNode>> stateNodes_;
    std::unordered_map<const AnimationState*, std::vector<EditorAnimationNodeLink>> stateNodeLinks_;

    EditorAnimationViewMode currentViewMode_{ EditorAnimationViewMode::StateMachine };
    std::shared_ptr<AnimationState> viewingState_{ nullptr };

    EditorAnimationSelectionType selectionType_{ EditorAnimationSelectionType::None };
    int selectedId_{ -1 };

    ImVec2 scrolling_{ 0.0f, 0.0f };
    char newVarName[64] = "";

    const float minScale_{ 0.1f };
    const float maxScale_{ 3.0f };
    float scale_{ 1.0f };

    int newVarType = 0;

    int nextId_{ 1 };

    std::string currentGraphPath_{};

    bool isDraggingLink_{ false };
    int draggingStartId_{ -1 };
};
