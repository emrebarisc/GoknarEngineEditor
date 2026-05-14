#include "pch.h"

#include <cstring>
#include <filesystem>
#include <sstream>
#include <queue>

#include "AnimationGraphPanel.h"
#include "UI/EditorHUD.h"
#include "UI/EditorAssetPathUtils.h"
#include "UI/Panels/SystemFileBrowserPanel.h"
#include "Goknar/Animation/AnimationCondition.h"
#include "Goknar/Animation/AnimationGraph.h"
#include "Goknar/Animation/AnimationSerializer.h"
#include "Goknar/Animation/AnimationDeserializer.h"

#include "imgui_internal.h"
#include "tinyxml2.h"

namespace
{
	constexpr const char* kAnimationGraphEditorReflectionFileType = "AnimationGraphEditorReflection";

	std::string GetAnimationGraphBrowserDirectory(const std::string& currentGraphPath)
	{
		if (!currentGraphPath.empty())
		{
			const std::filesystem::path absolutePath = std::filesystem::path(EditorAssetPathUtils::GetContentRootPath()) / currentGraphPath;
			return absolutePath.parent_path().lexically_normal().generic_string();
		}

		return EditorAssetPathUtils::GetContentRootPath();
	}

	const char* AnimationNodeTypeToLabel(AnimationNodeType type)
	{
		switch (type)
		{
		case AnimationNodeType::BlendSpace1D: return "Blend Space 1D";
		case AnimationNodeType::BlendSpace2D: return "Blend Space 2D";
		case AnimationNodeType::Clip:
		default: return "Clip";
		}
	}

	int AnimationNodeTypeToIndex(AnimationNodeType type)
	{
		switch (type)
		{
		case AnimationNodeType::BlendSpace1D: return 1;
		case AnimationNodeType::BlendSpace2D: return 2;
		case AnimationNodeType::Clip:
		default: return 0;
		}
	}

	AnimationNodeType AnimationNodeTypeFromIndex(int index)
	{
		switch (index)
		{
		case 1: return AnimationNodeType::BlendSpace1D;
		case 2: return AnimationNodeType::BlendSpace2D;
		case 0:
		default: return AnimationNodeType::Clip;
		}
	}

	bool DrawStringInput(const char* label, std::string& value)
	{
		char buffer[256] = {};
		value.copy(buffer, sizeof(buffer) - 1);
		if (ImGui::InputText(label, buffer, sizeof(buffer)))
		{
			value = buffer;
			return true;
		}

		return false;
	}

	std::string BuildNodeDisplayName(const AnimationNode* node)
	{
		if (!node)
		{
			return "Node";
		}

		switch (node->type)
		{
		case AnimationNodeType::BlendSpace1D:
			return node->parameterName.empty() ? "Blend Space 1D" : "Blend 1D: " + node->parameterName;
		case AnimationNodeType::BlendSpace2D:
			if (!node->parameterXName.empty() || !node->parameterYName.empty())
			{
				return "Blend 2D: " + node->parameterXName + "/" + node->parameterYName;
			}
			return "Blend Space 2D";
		case AnimationNodeType::Clip:
		default:
			return node->animationName.empty() ? "Clip" : node->animationName;
		}
	}

	ImU32 GetNodeColor(const AnimationNode* node, bool selected, bool entry)
	{
		if (entry)
		{
			return selected ? IM_COL32(100, 120, 80, 255) : IM_COL32(50, 100, 50, 255);
		}

		if (!node)
		{
			return selected ? IM_COL32(80, 80, 120, 255) : IM_COL32(50, 50, 50, 255);
		}

		if (selected)
		{
			return IM_COL32(80, 80, 120, 255);
		}

		switch (node->type)
		{
		case AnimationNodeType::BlendSpace1D: return IM_COL32(56, 76, 104, 255);
		case AnimationNodeType::BlendSpace2D: return IM_COL32(78, 62, 104, 255);
		case AnimationNodeType::Clip:
		default: return IM_COL32(50, 50, 50, 255);
		}
	}

	AnimationVariable MakeDefaultConditionValueForVariable(const AnimationVariable& variable)
	{
		return std::visit([](const auto& value) -> AnimationVariable
			{
				using T = std::decay_t<decltype(value)>;
				if constexpr (std::is_same_v<T, bool>) return false;
				else if constexpr (std::is_same_v<T, int>) return 0;
				else if constexpr (std::is_same_v<T, float>) return 0.f;
				else if constexpr (std::is_same_v<T, Vector2>) return Vector2(0.f);
				else if constexpr (std::is_same_v<T, Vector2i>) return Vector2i(0);
				else if constexpr (std::is_same_v<T, Vector3>) return Vector3(0.f);
				else if constexpr (std::is_same_v<T, Vector3i>) return Vector3i(0);
				else if constexpr (std::is_same_v<T, Vector4>) return Vector4(0.f);
				else return false;
			}, variable);
	}

	void DrawAnimationVariableValueEditor(const char* label, AnimationVariable& value)
	{
		std::visit([label](auto&& arg)
			{
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<T, bool>) ImGui::Checkbox(label, &arg);
				else if constexpr (std::is_same_v<T, int>) ImGui::DragInt(label, &arg);
				else if constexpr (std::is_same_v<T, float>) ImGui::DragFloat(label, &arg, 0.01f);
				else if constexpr (std::is_same_v<T, Vector2>) ImGui::DragFloat2(label, &arg.x, 0.01f);
				else if constexpr (std::is_same_v<T, Vector2i>) ImGui::DragInt2(label, &arg.x);
				else if constexpr (std::is_same_v<T, Vector3>) ImGui::DragFloat3(label, &arg.x, 0.01f);
				else if constexpr (std::is_same_v<T, Vector3i>) ImGui::DragInt3(label, &arg.x);
				else if constexpr (std::is_same_v<T, Vector4>) ImGui::DragFloat4(label, &arg.x, 0.01f);
			}, value);
	}

	void ConfigureNodeForType(AnimationNode& node, AnimationNodeType nodeType)
	{
		if (node.type == nodeType)
		{
			return;
		}

		node.type = nodeType;
		switch (nodeType)
		{
		case AnimationNodeType::Clip:
			node.blendSpace1DPoints.clear();
			node.blendSpace2DPoints.clear();
			break;
		case AnimationNodeType::BlendSpace1D:
			if (node.parameterName.empty())
			{
				node.parameterName = "Speed";
			}
			if (node.blendSpace1DPoints.empty())
			{
				BlendSpace1DPoint point;
				point.value = 0.f;
				point.animationName = node.animationName;
				node.blendSpace1DPoints.push_back(point);
			}
			node.blendSpace2DPoints.clear();
			break;
		case AnimationNodeType::BlendSpace2D:
			if (node.parameterXName.empty())
			{
				node.parameterXName = "MoveX";
			}
			if (node.parameterYName.empty())
			{
				node.parameterYName = "MoveY";
			}
			if (node.blendSpace2DPoints.empty())
			{
				BlendSpace2DPoint point;
				point.x = 0.f;
				point.y = 0.f;
				point.animationName = node.animationName;
				node.blendSpace2DPoints.push_back(point);
			}
			node.blendSpace1DPoints.clear();
			break;
		}
	}
}

static std::vector<float> ParseFloats(const std::string& str)
{
    std::vector<float> result;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ',')) result.push_back(std::stof(token));
    return result;
}

static std::vector<int> ParseInts(const std::string& str)
{
    std::vector<int> result;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ',')) result.push_back(std::stoi(token));
    return result;
}

std::string VariantToString(const AnimationVariable& val)
{
    if (std::holds_alternative<bool>(val)) return std::get<bool>(val) ? "1" : "0";
    if (std::holds_alternative<int>(val)) return std::to_string(std::get<int>(val));
    if (std::holds_alternative<float>(val)) return std::to_string(std::get<float>(val));

    if (std::holds_alternative<Vector2>(val)) { auto v = std::get<Vector2>(val); return std::to_string(v.x) + "," + std::to_string(v.y); }
    if (std::holds_alternative<Vector2i>(val)) { auto v = std::get<Vector2i>(val); return std::to_string(v.x) + "," + std::to_string(v.y); }
    if (std::holds_alternative<Vector3>(val)) { auto v = std::get<Vector3>(val); return std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z); }
    if (std::holds_alternative<Vector3i>(val)) { auto v = std::get<Vector3i>(val); return std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z); }
    if (std::holds_alternative<Vector4>(val)) { auto v = std::get<Vector4>(val); return std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z) + "," + std::to_string(v.w); }
    return "";
}

AnimationVariable StringToVariant(int typeIndex, const std::string& str)
{
    switch (typeIndex)
    {
    case 0: return str == "1";
    case 1: return std::stoi(str);
    case 2: return std::stof(str);
    case 3: { auto vals = ParseFloats(str); return Vector2{ vals.size() > 0 ? vals[0] : 0.f, vals.size() > 1 ? vals[1] : 0.f }; }
    case 4: { auto vals = ParseInts(str);   return Vector2i{ vals.size() > 0 ? vals[0] : 0, vals.size() > 1 ? vals[1] : 0 }; }
    case 5: { auto vals = ParseFloats(str); return Vector3{ vals.size() > 0 ? vals[0] : 0.f, vals.size() > 1 ? vals[1] : 0.f, vals.size() > 2 ? vals[2] : 0.f }; }
    case 6: { auto vals = ParseInts(str);   return Vector3i{ vals.size() > 0 ? vals[0] : 0, vals.size() > 1 ? vals[1] : 0, vals.size() > 2 ? vals[2] : 0 }; }
    case 7: { auto vals = ParseFloats(str); return Vector4{ vals.size() > 0 ? vals[0] : 0.f, vals.size() > 1 ? vals[1] : 0.f, vals.size() > 2 ? vals[2] : 0.f, vals.size() > 3 ? vals[3] : 0.f }; }
    default: return false;
    }
}

AnimationGraphPanel::AnimationGraphPanel(EditorHUD* hud)
    : IEditorPanel("Animation Graph Editor", hud)
{
    activeGraph_ = std::make_shared<AnimationGraph>();

    isOpen_ = false;
}

AnimationGraphPanel::~AnimationGraphPanel()
{
}

void AnimationGraphPanel::Init()
{
    ResetToDefaultGraph();
}

void AnimationGraphPanel::ResetToDefaultGraph()
{
    activeGraph_ = std::make_shared<AnimationGraph>();
    editorStates_.clear();
    stateLinks_.clear();
    stateNodes_.clear();
    stateNodeLinks_.clear();
    currentViewMode_ = EditorAnimationViewMode::StateMachine;
    viewingState_ = nullptr;
    selectionType_ = EditorAnimationSelectionType::None;
    selectedId_ = -1;
    scrolling_ = ImVec2(0.0f, 0.0f);
    scale_ = 1.0f;
    nextId_ = 1;
    isDraggingLink_ = false;
    draggingStartId_ = -1;

    EditorAnimationState rootState;
    rootState.id = nextId_++;
    rootState.name = "RootState";
    rootState.pos = ImVec2(50, 50);
    rootState.size = ImVec2(150, 50);
    rootState.animState = std::make_shared<AnimationState>();
    rootState.animState->name = "RootState";

    EditorAnimationNode rootNode;
    rootNode.id = nextId_++;
    rootNode.name = "Idle";
    rootNode.pos = ImVec2(50, 50);
    rootNode.size = ImVec2(190, 64);
    rootNode.animNode = std::make_shared<AnimationNode>();
    rootNode.animNode->type = AnimationNodeType::Clip;
    rootNode.animNode->animationName = "Idle";
    rootNode.name = BuildNodeDisplayName(rootNode.animNode.get());

    rootState.animState->SetEntryNode(rootNode.animNode);
    rootState.animState->AddNode(rootNode.animNode);

    editorStates_.push_back(rootState);
    stateNodes_[rootState.animState.get()].push_back(rootNode);
    activeGraph_->AddState(rootState.animState);
}

void AnimationGraphPanel::OpenAnimationGraph(const std::string& path)
{
    LoadGraphFromXML(path);
}

void AnimationGraphPanel::SaveAnimationGraph(const std::string& path)
{
    SaveGraphToXML(path);
}

void AnimationGraphPanel::SaveCurrentAnimationGraph()
{
    if (!currentGraphPath_.empty())
    {
        SaveGraphToXML(currentGraphPath_);
        return;
    }

    if (SystemFileBrowserPanel* browser = hud_->GetPanel<SystemFileBrowserPanel>())
    {
        browser->SaveFileSelector(
            Delegate<void(const std::string&)>::Create<AnimationGraphPanel, &AnimationGraphPanel::SaveAnimationGraph>(this),
            GetAnimationGraphBrowserDirectory(currentGraphPath_),
            "",
            EditorAssetPathUtils::GetProjectRootPath());
    }
}

void AnimationGraphPanel::Draw()
{
    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title_.c_str(), &isOpen_)) { ImGui::End(); return; }

    if (ImGui::Button("New Graph"))
    {
        ResetToDefaultGraph();
        currentGraphPath_.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Open Graph"))
    {
        if (SystemFileBrowserPanel* browser = hud_->GetPanel<SystemFileBrowserPanel>())
        {
            browser->OpenFileSelector(
                Delegate<void(const std::string&)>::Create<AnimationGraphPanel, &AnimationGraphPanel::OpenAnimationGraph>(this),
                GetAnimationGraphBrowserDirectory(currentGraphPath_),
                EditorAssetPathUtils::GetProjectRootPath());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Graph"))
    {
        SaveCurrentAnimationGraph();
    }
    ImGui::Separator();

    static ImGuiTableFlags rootFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;
    if (ImGui::BeginTable("EditorLayout", 3, rootFlags))
    {
        ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 250.0f);
        ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 300.0f);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        DrawVariablesSidebar();
        ImGui::Separator();
        DrawHierarchySidebar();

        ImGui::TableSetColumnIndex(1);
        DrawNodeCanvas();

        ImGui::TableSetColumnIndex(2);
        DrawPropertiesPanel();
        ImGui::Separator();
        if (selectionType_ != EditorAnimationSelectionType::None) DrawDetailsSidebar();

        ImGui::EndTable();
    }
    ImGui::End();
}

void AnimationGraphPanel::DrawVariablesSidebar()
{
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Graph Variables");

    if (ImGui::Button("Add Variable", ImVec2(-1, 0))) ImGui::OpenPopup("AddVarPopup");

    if (ImGui::BeginPopup("AddVarPopup"))
    {
        ImGui::InputText("Name", newVarName, IM_ARRAYSIZE(newVarName));
        const char* types[] = { "Bool", "Int", "Float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4" };
        ImGui::Combo("Type", &newVarType, types, IM_ARRAYSIZE(types));

        if (ImGui::Button("Create") && strlen(newVarName) > 0)
        {
            switch (newVarType)
            {
            case 0: activeGraph_->SetVariable(newVarName, false); break;
            case 1: activeGraph_->SetVariable(newVarName, 0); break;
            case 2: activeGraph_->SetVariable(newVarName, 0.0f); break;
            case 3: activeGraph_->SetVariable(newVarName, Vector2(0.f)); break;
            case 4: activeGraph_->SetVariable(newVarName, Vector2i(0)); break;
            case 5: activeGraph_->SetVariable(newVarName, Vector3(0.f)); break;
            case 6: activeGraph_->SetVariable(newVarName, Vector3i(0)); break;
            case 7: activeGraph_->SetVariable(newVarName, Vector4(0.f)); break;
            }
            memset(newVarName, 0, sizeof(newVarName));
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::BeginChild("VarsList", ImVec2(0, 0), true);
    if (activeGraph_)
    {
        std::string varToDelete = "";
        for (auto& [name, value] : activeGraph_->variables)
        {
            ImGui::PushID(name.c_str());
            const char* typeName = "Unknown";
            size_t typeIdx = value.index();
            const char* typeNames[] = { "bool", "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4" };
            if (typeIdx < 8) typeName = typeNames[typeIdx];

            ImGui::TextDisabled("[%s]", typeName); ImGui::SameLine(); ImGui::Text("%s", name.c_str());

            if (ImGui::BeginPopupContextItem("VarContextMenu"))
            {
                if (ImGui::MenuItem("Delete Variable")) varToDelete = name;
                ImGui::EndPopup();
            }

            ImGui::Indent();
            std::visit([](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, bool>) ImGui::Checkbox("Value", &arg);
                else if constexpr (std::is_same_v<T, int>) ImGui::DragInt("Value", &arg);
                else if constexpr (std::is_same_v<T, float>) ImGui::DragFloat("Value", &arg, 0.01f);
                else if constexpr (std::is_same_v<T, Vector2>) ImGui::DragFloat2("Value", &arg.x, 0.01f);
                else if constexpr (std::is_same_v<T, Vector2i>) ImGui::DragInt2("Value", &arg.x);
                else if constexpr (std::is_same_v<T, Vector3>) ImGui::DragFloat3("Value", &arg.x, 0.01f);
                else if constexpr (std::is_same_v<T, Vector3i>) ImGui::DragInt3("Value", &arg.x);
                else if constexpr (std::is_same_v<T, Vector4>) ImGui::DragFloat4("Value", &arg.x, 0.01f);
                }, value);
            ImGui::Unindent();

            ImGui::Separator();
            ImGui::PopID();
        }
        if (!varToDelete.empty()) activeGraph_->variables.erase(varToDelete);
    }
    ImGui::EndChild();
}

void AnimationGraphPanel::DrawHierarchySidebar()
{
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "States Hierarchy");
    ImGui::BeginChild("HierarchyList", ImVec2(0, 0), true);

    if (activeGraph_)
    {
        for (const auto& state : activeGraph_->GetStates())
        {
            bool isSelected = (currentViewMode_ == EditorAnimationViewMode::InsideState && viewingState_ == state);
            if (ImGui::Selectable(state->name.c_str(), isSelected))
            {
                currentViewMode_ = EditorAnimationViewMode::InsideState;
                viewingState_ = state;
                selectionType_ = EditorAnimationSelectionType::None;
            }
        }
    }
    ImGui::EndChild();
}

void AnimationGraphPanel::CollectNodes(const std::shared_ptr<AnimationNode>& node, std::unordered_set<const AnimationNode*>& visited, std::vector<std::shared_ptr<AnimationNode>>& outNodes)
{
    if (!node || visited.count(node.get())) return;
    visited.insert(node.get());
    outNodes.push_back(node);
    for (const auto& transition : node->outboundConnections)
    {
        CollectNodes(transition->target.lock(), visited, outNodes);
    }
}

void AnimationGraphPanel::SaveGraphToXML(const std::string& filepath)
{
    currentGraphPath_ = EditorAssetPathUtils::ToContentRelativePath(filepath);
    AnimationSerializer serializer;
    if (serializer.Serialize(activeGraph_.get(), currentGraphPath_))
    {
        SaveEditorReflection(currentGraphPath_);
    }
}

void AnimationGraphPanel::LoadGraphFromXML(const std::string& filepath)
{
    currentGraphPath_ = EditorAssetPathUtils::ToContentRelativePath(filepath);
    activeGraph_ = std::make_shared<AnimationGraph>();
    AnimationDeserializer deserializer;
    if (!deserializer.Deserialize(activeGraph_.get(), currentGraphPath_))
    {
        ResetToDefaultGraph();
        currentGraphPath_.clear();
        return;
    }

    RebuildEditorGraphFromRuntimeData();
    if (!LoadEditorReflection(currentGraphPath_))
    {
        AutoLayoutEditorGraph();
        SaveEditorReflection(currentGraphPath_);
    }
}

void AnimationGraphPanel::RebuildEditorGraphFromRuntimeData()
{
    editorStates_.clear();
    stateLinks_.clear();
    stateNodes_.clear();
    stateNodeLinks_.clear();
    selectionType_ = EditorAnimationSelectionType::None;
    currentViewMode_ = EditorAnimationViewMode::StateMachine;
    viewingState_ = nullptr;
    scrolling_ = ImVec2(0.0f, 0.0f);
    scale_ = 1.0f;
    nextId_ = 1;
    isDraggingLink_ = false;
    draggingStartId_ = -1;

    std::unordered_map<const AnimationState*, int> stateToEditorId;
    std::unordered_map<const AnimationNode*, int> nodeToEditorId;

    for (const auto& state : activeGraph_->GetStates())
    {
        EditorAnimationState editorState;
        editorState.id = nextId_++;
        editorState.name = state->name;
        editorState.pos = ImVec2(50.0f, 50.0f);
        editorState.size = ImVec2(160.0f, 50.0f);
        editorState.animState = state;
        editorStates_.push_back(editorState);
        stateToEditorId[state.get()] = editorState.id;

        std::unordered_set<const AnimationNode*> visitedNodes;
        std::vector<std::shared_ptr<AnimationNode>> flatNodes;

        std::weak_ptr<AnimationNode> entryNodeWeakPtr = state->GetEntryNode();
        CollectNodes(entryNodeWeakPtr.lock(), visitedNodes, flatNodes);
        for (const std::shared_ptr<AnimationNode>& node : state->GetNodes())
        {
            CollectNodes(node, visitedNodes, flatNodes);
        }

        for (const auto& node : flatNodes)
        {
            EditorAnimationNode editorNode;
            editorNode.id = nextId_++;
            editorNode.name = BuildNodeDisplayName(node.get());
            editorNode.pos = ImVec2(50.0f, 50.0f);
            editorNode.size = ImVec2(190.0f, 64.0f);
            editorNode.animNode = node;
            stateNodes_[state.get()].push_back(editorNode);
            nodeToEditorId[node.get()] = editorNode.id;
        }
    }

    for (const auto& state : activeGraph_->GetStates())
    {
        for (const auto& transition : state->outboundConnections)
        {
            EditorAnimationStateLink link;
            link.id = nextId_++;
            link.startId = stateToEditorId[state.get()];
            link.endId = stateToEditorId[transition->target.lock().get()];
            link.transition = transition;
            stateLinks_.push_back(link);
        }

        std::unordered_set<const AnimationNode*> visitedNodes;
        std::vector<std::shared_ptr<AnimationNode>> flatNodes;
        CollectNodes(state->GetEntryNode(), visitedNodes, flatNodes);
        for (const std::shared_ptr<AnimationNode>& node : state->GetNodes())
        {
            CollectNodes(node, visitedNodes, flatNodes);
        }
        for (const auto& node : flatNodes)
        {
            for (const auto& transition : node->outboundConnections)
            {
                EditorAnimationNodeLink link;
                link.id = nextId_++;
                link.startId = nodeToEditorId[node.get()];
                link.endId = nodeToEditorId[transition->target.lock().get()];
                link.transition = transition;
                stateNodeLinks_[state.get()].push_back(link);
            }
        }
    }
}

void AnimationGraphPanel::AddNodeToViewingState(AnimationNodeType nodeType, const ImVec2& position)
{
    if (!viewingState_)
    {
        return;
    }

    EditorAnimationNode editorNode;
    editorNode.id = nextId_++;
    editorNode.pos = position;
    editorNode.size = ImVec2(190.0f, 64.0f);
    editorNode.animNode = std::make_shared<AnimationNode>();
    ConfigureNodeForType(*editorNode.animNode, nodeType);
    editorNode.name = BuildNodeDisplayName(editorNode.animNode.get());

    if (!viewingState_->GetEntryNode())
    {
        viewingState_->SetEntryNode(editorNode.animNode);
    }

    viewingState_->AddNode(editorNode.animNode);
    stateNodes_[viewingState_.get()].push_back(editorNode);
}

void AnimationGraphPanel::AutoLayoutEditorGraph()
{
    std::unordered_map<const AnimationState*, int> stateDepths;
    std::unordered_map<int, int> stateRowCounters;
    std::queue<std::pair<const AnimationState*, int>> stateQueue;
    std::unordered_set<const AnimationState*> visitedStates;

    if (!activeGraph_->GetStates().empty())
    {
        stateQueue.push({ activeGraph_->GetStates().front().get(), 0 });
        visitedStates.insert(activeGraph_->GetStates().front().get());
    }

    for (const auto& state : activeGraph_->GetStates())
    {
        if (visitedStates.find(state.get()) == visitedStates.end())
        {
            stateQueue.push({ state.get(), 0 });
            visitedStates.insert(state.get());
        }
    }

    while (!stateQueue.empty())
    {
        auto [currentState, depth] = stateQueue.front();
        stateQueue.pop();
        stateDepths[currentState] = depth;

        for (const auto& transition : currentState->outboundConnections)
        {
            if (visitedStates.find(transition->target.lock().get()) == visitedStates.end())
            {
                visitedStates.insert(transition->target.lock().get());
                stateQueue.push({ transition->target.lock().get(), depth + 1 });
            }
        }
    }

    for (EditorAnimationState& state : editorStates_)
    {
        const int depth = stateDepths[state.animState.get()];
        const int row = stateRowCounters[depth]++;
        state.pos = ImVec2(50.0f + depth * 250.0f, 50.0f + row * 150.0f);

        std::unordered_set<const AnimationNode*> visitedNodes;
        std::vector<std::shared_ptr<AnimationNode>> flatNodes;
        CollectNodes(state.animState->GetEntryNode(), visitedNodes, flatNodes);
        for (const std::shared_ptr<AnimationNode>& node : state.animState->GetNodes())
        {
            CollectNodes(node, visitedNodes, flatNodes);
        }

        std::unordered_map<const AnimationNode*, int> nodeDepths;
        std::unordered_map<int, int> nodeRowCounters;
        std::queue<std::pair<const AnimationNode*, int>> nodeQueue;
        std::unordered_set<const AnimationNode*> queuedNodes;

        if (state.animState->GetEntryNode())
        {
            nodeQueue.push({ state.animState->GetEntryNode().get(), 0 });
            queuedNodes.insert(state.animState->GetEntryNode().get());
        }

        for (const auto& node : flatNodes)
        {
            if (queuedNodes.find(node.get()) == queuedNodes.end())
            {
                nodeQueue.push({ node.get(), 0 });
                queuedNodes.insert(node.get());
            }
        }

        while (!nodeQueue.empty())
        {
            auto [currentNode, currentDepth] = nodeQueue.front();
            nodeQueue.pop();
            nodeDepths[currentNode] = currentDepth;

            for (const auto& transition : currentNode->outboundConnections)
            {
                if (queuedNodes.find(transition->target.lock().get()) == queuedNodes.end())
                {
                    queuedNodes.insert(transition->target.lock().get());
                    nodeQueue.push({ transition->target.lock().get(), currentDepth + 1 });
                }
            }
        }

        for (EditorAnimationNode& node : stateNodes_[state.animState.get()])
        {
            const int nodeDepth = nodeDepths[node.animNode.get()];
            const int nodeRow = nodeRowCounters[nodeDepth]++;
            node.pos = ImVec2(50.0f + nodeDepth * 250.0f, 50.0f + nodeRow * 150.0f);
        }
    }
}

void AnimationGraphPanel::SaveEditorReflection(const std::string& assetPath)
{
    const std::string reflectionPath = EditorAssetPathUtils::ToEditorReflectionPath(assetPath);
    if (!EditorAssetPathUtils::EnsureDirectoryForFile(reflectionPath))
    {
        return;
    }

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* root = doc.NewElement("GameAsset");
    root->SetAttribute("FileType", kAnimationGraphEditorReflectionFileType);
    doc.InsertFirstChild(root);

    tinyxml2::XMLElement* canvasElement = doc.NewElement("Canvas");
    canvasElement->SetAttribute("Scale", scale_);
    canvasElement->SetAttribute("ScrollX", scrolling_.x);
    canvasElement->SetAttribute("ScrollY", scrolling_.y);
    root->InsertEndChild(canvasElement);

    tinyxml2::XMLElement* statesElement = doc.NewElement("States");
    for (size_t stateIndex = 0; stateIndex < editorStates_.size(); ++stateIndex)
    {
        const EditorAnimationState& state = editorStates_[stateIndex];
        tinyxml2::XMLElement* stateElement = doc.NewElement("State");
        stateElement->SetAttribute("Index", static_cast<int>(stateIndex));
        stateElement->SetAttribute("PosX", state.pos.x);
        stateElement->SetAttribute("PosY", state.pos.y);
        statesElement->InsertEndChild(stateElement);

        tinyxml2::XMLElement* nodesElement = doc.NewElement("Nodes");
        const auto& stateNodes = stateNodes_[state.animState.get()];
        for (size_t nodeIndex = 0; nodeIndex < stateNodes.size(); ++nodeIndex)
        {
            const EditorAnimationNode& node = stateNodes[nodeIndex];
            tinyxml2::XMLElement* nodeElement = doc.NewElement("Node");
            nodeElement->SetAttribute("Index", static_cast<int>(nodeIndex));
            nodeElement->SetAttribute("PosX", node.pos.x);
            nodeElement->SetAttribute("PosY", node.pos.y);
            nodesElement->InsertEndChild(nodeElement);
        }
        stateElement->InsertEndChild(nodesElement);
    }
    root->InsertEndChild(statesElement);

    doc.SaveFile(reflectionPath.c_str());
}

bool AnimationGraphPanel::LoadEditorReflection(const std::string& assetPath)
{
    const std::string reflectionPath = EditorAssetPathUtils::ToEditorReflectionPath(assetPath);
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(reflectionPath.c_str()) != tinyxml2::XML_SUCCESS)
    {
        return false;
    }

    tinyxml2::XMLElement* root = doc.FirstChildElement("GameAsset");
    const char* fileType = root ? root->Attribute("FileType") : nullptr;
    if (!root || !fileType || std::string(fileType) != kAnimationGraphEditorReflectionFileType)
    {
        return false;
    }

    tinyxml2::XMLElement* canvasElement = root->FirstChildElement("Canvas");
    if (canvasElement)
    {
        scale_ = canvasElement->FloatAttribute("Scale", 1.0f);
        scrolling_.x = canvasElement->FloatAttribute("ScrollX", 0.0f);
        scrolling_.y = canvasElement->FloatAttribute("ScrollY", 0.0f);
    }

    tinyxml2::XMLElement* statesElement = root->FirstChildElement("States");
    for (tinyxml2::XMLElement* stateElement = statesElement ? statesElement->FirstChildElement("State") : nullptr;
        stateElement != nullptr;
        stateElement = stateElement->NextSiblingElement("State"))
    {
        const int stateIndex = stateElement->IntAttribute("Index", -1);
        if (stateIndex < 0 || stateIndex >= static_cast<int>(editorStates_.size()))
        {
            continue;
        }

        editorStates_[stateIndex].pos.x = stateElement->FloatAttribute("PosX", editorStates_[stateIndex].pos.x);
        editorStates_[stateIndex].pos.y = stateElement->FloatAttribute("PosY", editorStates_[stateIndex].pos.y);

        tinyxml2::XMLElement* nodesElement = stateElement->FirstChildElement("Nodes");
        auto& stateNodes = stateNodes_[editorStates_[stateIndex].animState.get()];
        for (tinyxml2::XMLElement* nodeElement = nodesElement ? nodesElement->FirstChildElement("Node") : nullptr;
            nodeElement != nullptr;
            nodeElement = nodeElement->NextSiblingElement("Node"))
        {
            const int nodeIndex = nodeElement->IntAttribute("Index", -1);
            if (nodeIndex < 0 || nodeIndex >= static_cast<int>(stateNodes.size()))
            {
                continue;
            }

            stateNodes[nodeIndex].pos.x = nodeElement->FloatAttribute("PosX", stateNodes[nodeIndex].pos.x);
            stateNodes[nodeIndex].pos.y = nodeElement->FloatAttribute("PosY", stateNodes[nodeIndex].pos.y);
        }
    }

    return true;
}

void AnimationGraphPanel::DrawPropertiesPanel()
{
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Selection Properties");
    ImGui::Separator();

    if (selectionType_ == EditorAnimationSelectionType::State)
    {
        auto it = std::find_if(editorStates_.begin(), editorStates_.end(), [&](const auto& s) { return s.id == selectedId_; });
        if (it != editorStates_.end())
        {
            ImGui::LabelText("Type", "Animation State");
            char buf[64]; strcpy_s(buf, it->animState->name.c_str());
            if (ImGui::InputText("State Name", buf, sizeof(buf))) {
                it->animState->name = buf;
                it->name = buf;
            }
        }
    }
    else if (selectionType_ == EditorAnimationSelectionType::Node)
    {
        auto& nodes = stateNodes_[viewingState_.get()];
        auto it = std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) { return n.id == selectedId_; });
        if (it != nodes.end())
        {
            ImGui::LabelText("Type", "Animation Node");
            AnimationNode* node = it->animNode.get();

            const char* nodeTypeNames[] = { "Clip", "Blend Space 1D", "Blend Space 2D" };
            int nodeTypeIndex = AnimationNodeTypeToIndex(node->type);
            if (ImGui::Combo("Node Type", &nodeTypeIndex, nodeTypeNames, IM_ARRAYSIZE(nodeTypeNames)))
            {
                ConfigureNodeForType(*node, AnimationNodeTypeFromIndex(nodeTypeIndex));
            }

            ImGui::Checkbox("Looping", &it->animNode->loop);
            ImGui::DragFloat("Play Rate", &node->playRate, 0.01f, 0.0f, 10.0f, "%.2f");
            ImGui::DragFloat("Parameter Smoothing", &node->parameterSmoothingSpeed, 0.1f, 0.0f, 60.0f, "%.2f");
            DrawStringInput("Sync Group", node->syncGroup);
            ImGui::Separator();

            if (node->type == AnimationNodeType::Clip)
            {
                DrawStringInput("Animation Clip", node->animationName);
            }
            else if (node->type == AnimationNodeType::BlendSpace1D)
            {
                DrawStringInput("Parameter", node->parameterName);
                ImGui::Text("Blend Points");
                if (ImGui::Button("+ Add 1D Point"))
                {
                    BlendSpace1DPoint point;
                    point.value = node->blendSpace1DPoints.empty() ? 0.f : node->blendSpace1DPoints.back().value + 0.1f;
                    node->blendSpace1DPoints.push_back(point);
                }

                for (size_t pointIndex = 0; pointIndex < node->blendSpace1DPoints.size(); ++pointIndex)
                {
                    ImGui::PushID(static_cast<int>(pointIndex));
                    BlendSpace1DPoint& point = node->blendSpace1DPoints[pointIndex];
                    ImGui::DragFloat("Value", &point.value, 0.001f);
                    DrawStringInput("Clip", point.animationName);
                    if (ImGui::Button("Remove Point"))
                    {
                        node->blendSpace1DPoints.erase(node->blendSpace1DPoints.begin() + pointIndex);
                        ImGui::PopID();
                        break;
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
            else if (node->type == AnimationNodeType::BlendSpace2D)
            {
                DrawStringInput("Parameter X", node->parameterXName);
                DrawStringInput("Parameter Y", node->parameterYName);
                ImGui::Text("Blend Points");
                if (ImGui::Button("+ Add 2D Point"))
                {
                    BlendSpace2DPoint point;
                    node->blendSpace2DPoints.push_back(point);
                }

                for (size_t pointIndex = 0; pointIndex < node->blendSpace2DPoints.size(); ++pointIndex)
                {
                    ImGui::PushID(static_cast<int>(pointIndex));
                    BlendSpace2DPoint& point = node->blendSpace2DPoints[pointIndex];
                    ImGui::DragFloat("X", &point.x, 0.001f);
                    ImGui::DragFloat("Y", &point.y, 0.001f);
                    DrawStringInput("Clip", point.animationName);
                    if (ImGui::Button("Remove Point"))
                    {
                        node->blendSpace2DPoints.erase(node->blendSpace2DPoints.begin() + pointIndex);
                        ImGui::PopID();
                        break;
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }

            it->name = BuildNodeDisplayName(node);

            if (viewingState_->GetEntryNode() == it->animNode)
            {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "[Entry Node]");
            }
            else if (ImGui::Button("Set as Entry Node"))
            {
                viewingState_->SetEntryNode(it->animNode);
            }
        }
    }
    else if (selectionType_ == EditorAnimationSelectionType::StateLink || selectionType_ == EditorAnimationSelectionType::NodeLink)
    {
        ImGui::LabelText("Type", "Transition Link");

        std::vector<AnimationCondition>* conditions = nullptr;
        bool* transitDone = nullptr;
        float* duration = nullptr;

        if (selectionType_ == EditorAnimationSelectionType::StateLink) {
            auto it = std::find_if(stateLinks_.begin(), stateLinks_.end(), [&](const auto& l) { return l.id == selectedId_; });
            if (it != stateLinks_.end()) { conditions = &it->transition->conditions; transitDone = &it->transition->transitWhenAnimationDone; duration = &it->transition->duration; }
        }
        else {
            auto& links = stateNodeLinks_[viewingState_.get()];
            auto it = std::find_if(links.begin(), links.end(), [&](const auto& l) { return l.id == selectedId_; });
            if (it != links.end()) { conditions = &it->transition->conditions; transitDone = &it->transition->transitWhenAnimationDone; duration = &it->transition->duration; }
        }

        if (conditions && transitDone && duration)
        {
            ImGui::Checkbox("Transit on Exit", transitDone);
            ImGui::DragFloat("Crossfade Duration", duration, 0.01f, 0.0f, 5.0f, "%.2f");
            *duration = GoknarMath::Max(*duration, 0.f);
            ImGui::Separator();
            ImGui::Text("Conditions");
            if (ImGui::Button("+ Add Condition"))
            {
                if (!activeGraph_->variables.empty())
                {
                    const auto& firstVariable = *activeGraph_->variables.begin();
                    conditions->push_back({ firstVariable.first, CompareOp::Equal, MakeDefaultConditionValueForVariable(firstVariable.second) });
                }
                else
                {
                    conditions->push_back({ "NewVar", CompareOp::Equal, false });
                }
            }

            for (size_t i = 0; i < conditions->size(); ++i)
            {
                ImGui::PushID((int)i);
                auto& cond = (*conditions)[i];

                if (ImGui::BeginCombo("Variable", cond.variableName.empty() ? "<none>" : cond.variableName.c_str()))
                {
                    for (const auto& variablePair : activeGraph_->variables)
                    {
                        const bool selected = cond.variableName == variablePair.first;
                        if (ImGui::Selectable(variablePair.first.c_str(), selected))
                        {
                            cond.variableName = variablePair.first;
                            cond.targetValue = MakeDefaultConditionValueForVariable(variablePair.second);
                        }
                        if (selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                DrawStringInput("Variable Name", cond.variableName);

                const char* opNames[] = { "==", "!=", ">", "<", ">=", "<=" };
                int currentOp = (int)cond.operation;
                if (ImGui::Combo("Operator", &currentOp, opNames, 6)) cond.operation = (CompareOp)currentOp;

                DrawAnimationVariableValueEditor("Value", cond.targetValue);

                if (ImGui::Button("Remove")) conditions->erase(conditions->begin() + i);
                ImGui::Separator();
                ImGui::PopID();
            }
        }
    }
    else
    {
        ImGui::TextDisabled("Select an item to edit.");
    }
}

void AnimationGraphPanel::DrawDetailsSidebar()
{
    ImGui::TextDisabled("Layout Data");
}

void AnimationGraphPanel::DrawNodeCanvas()
{
    if (currentViewMode_ == EditorAnimationViewMode::InsideState)
    {
        if (ImGui::Button("<- Back to State Machine"))
        {
            currentViewMode_ = EditorAnimationViewMode::StateMachine;
            selectionType_ = EditorAnimationSelectionType::None;
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Viewing State: %s", viewingState_->name.c_str());
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "State Machine Root");
    }

    ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();

    if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f)
    {
        float mouseWheel = ImGui::GetIO().MouseWheel;
        float oldScale = scale_;
        scale_ = ImClamp(scale_ + mouseWheel * 0.05f, minScale_, maxScale_);

        ImVec2 mousePosInCanvas = ImVec2(ImGui::GetMousePos().x - canvas_p0.x, ImGui::GetMousePos().y - canvas_p0.y);
        ImVec2 mousePosInWorld = ImVec2((mousePosInCanvas.x - scrolling_.x) / oldScale, (mousePosInCanvas.y - scrolling_.y) / oldScale);
        scrolling_.x = mousePosInCanvas.x - (mousePosInWorld.x * scale_);
        scrolling_.y = mousePosInCanvas.y - (mousePosInWorld.y * scale_);
    }

    ImVec2 offset = ImVec2(canvas_p0.x + scrolling_.x, canvas_p0.y + scrolling_.y);

    float GRID_SZ = 64.0f * scale_;
    ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
    for (float x = fmodf(scrolling_.x, GRID_SZ); x < canvas_sz.x; x += GRID_SZ) draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p0.y + canvas_sz.y), GRID_COLOR);
    for (float y = fmodf(scrolling_.y, GRID_SZ); y < canvas_sz.y; y += GRID_SZ) draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + y), GRID_COLOR);

    draw_list->ChannelsSplit(2);

    // DRAW LINKS
    draw_list->ChannelsSetCurrent(0);

    auto DrawLinks = [&](auto& linkList, EditorAnimationSelectionType linkType, auto& nodeList) {
        for (auto& link : linkList)
        {
            ImVec2 startPos, endPos, startSize, endSize;
            bool foundStart = false, foundEnd = false;

            for (auto& n : nodeList) {
                if (n.id == link.startId) { startPos = n.pos; startSize = n.size; foundStart = true; }
                if (n.id == link.endId) { endPos = n.pos; endSize = n.size; foundEnd = true; }
            }

            if (foundStart && foundEnd)
            {
                ImVec2 p1 = ImVec2(offset.x + (startPos.x + startSize.x) * scale_, offset.y + (startPos.y + startSize.y / 2.0f) * scale_);
                ImVec2 p2 = ImVec2(offset.x + endPos.x * scale_, offset.y + (endPos.y + endSize.y / 2.0f) * scale_);
                ImVec2 cp1 = ImVec2(p1.x + 50.0f * scale_, p1.y);
                ImVec2 cp2 = ImVec2(p2.x - 50.0f * scale_, p2.y);

                bool isSelected = (selectionType_ == linkType && selectedId_ == link.id);
                ImU32 color = isSelected ? IM_COL32(255, 255, 0, 255) : IM_COL32(200, 200, 100, 255);
                draw_list->AddBezierCubic(p1, cp1, cp2, p2, color, (isSelected ? 4.0f : 2.0f) * scale_);

                if (ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemActive())
                {
                    ImVec2 mPos = ImGui::GetMousePos();
                    ImVec2 closest = ImBezierCubicClosestPointCasteljau(p1, cp1, cp2, p2, mPos, ImGui::GetStyle().CurveTessellationTol);
                    float distSq = (mPos.x - closest.x) * (mPos.x - closest.x) + (mPos.y - closest.y) * (mPos.y - closest.y);
                    if (distSq < (64.0f * scale_ * scale_)) { selectionType_ = linkType; selectedId_ = link.id; }
                }
            }
        }
        };

    if (currentViewMode_ == EditorAnimationViewMode::StateMachine) DrawLinks(stateLinks_, EditorAnimationSelectionType::StateLink, editorStates_);
    else DrawLinks(stateNodeLinks_[viewingState_.get()], EditorAnimationSelectionType::NodeLink, stateNodes_[viewingState_.get()]);

    // DRAW NODES/STATES
    draw_list->ChannelsSetCurrent(1);

    auto DrawElements = [&](auto& list, EditorAnimationSelectionType type) {
        // Deduce the exact type of the elements inside the list at compile time
        using ElementType = std::decay_t<decltype(list[0])>;

        for (auto& element : list)
        {
            ImGui::PushID(element.id);
            ImVec2 rect_min = ImVec2(offset.x + element.pos.x * scale_, offset.y + element.pos.y * scale_);
            ImVec2 scaled_size = ImVec2(element.size.x * scale_, element.size.y * scale_);
            ImVec2 rect_max = ImVec2(rect_min.x + scaled_size.x, rect_min.y + scaled_size.y);

            const bool isSelected = selectionType_ == type && selectedId_ == element.id;
            ImU32 bg_col = isSelected ? IM_COL32(80, 80, 120, 255) : IM_COL32(50, 50, 50, 255);

            // ONLY compile this block if the element is an EditorAnimationNode
            if constexpr (std::is_same_v<ElementType, EditorAnimationNode>)
            {
                const bool isEntry = viewingState_ && viewingState_->GetEntryNode() == element.animNode;
                bg_col = GetNodeColor(element.animNode.get(), isSelected, isEntry);
            }

            draw_list->AddRectFilled(rect_min, rect_max, bg_col, 4.0f * scale_);
            draw_list->AddRect(rect_min, rect_max, IM_COL32(100, 100, 100, 255), 4.0f * scale_);

            ImGui::SetCursorScreenPos(ImVec2(rect_min.x + 10.0f * scale_, rect_min.y + 15.0f * scale_));
            ImGui::SetWindowFontScale(scale_);
            if constexpr (std::is_same_v<ElementType, EditorAnimationNode>)
            {
                ImGui::TextDisabled("%s", AnimationNodeTypeToLabel(element.animNode->type));
                ImGui::SetCursorScreenPos(ImVec2(rect_min.x + 10.0f * scale_, rect_min.y + 34.0f * scale_));
                ImGui::Text("%s", element.name.c_str());
            }
            else
            {
                ImGui::Text("%s", element.name.c_str());
            }
            ImGui::SetWindowFontScale(1.0f);

            ImVec2 in_pin = ImVec2(rect_min.x, rect_min.y + scaled_size.y / 2.0f);
            ImVec2 out_pin = ImVec2(rect_max.x, rect_min.y + scaled_size.y / 2.0f);
            draw_list->AddCircleFilled(in_pin, 5.0f * scale_, IM_COL32(180, 180, 180, 255));
            draw_list->AddCircleFilled(out_pin, 5.0f * scale_, IM_COL32(180, 180, 180, 255));

            ImGui::SetCursorScreenPos(rect_min);
            if (ImGui::InvisibleButton("body", scaled_size)) { selectionType_ = type; selectedId_ = element.id; }

            // ONLY compile this block if the element is an EditorAnimationState
            if constexpr (std::is_same_v<ElementType, EditorAnimationState>)
            {
                if (hud_->WasLastItemDoubleClicked(ImGuiMouseButton_Left))
                {
                    currentViewMode_ = EditorAnimationViewMode::InsideState;
                    viewingState_ = element.animState;
                    selectionType_ = EditorAnimationSelectionType::None;
                }
            }

            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) { element.pos.x += ImGui::GetIO().MouseDelta.x / scale_; element.pos.y += ImGui::GetIO().MouseDelta.y / scale_; }

            ImGui::SetCursorScreenPos(ImVec2(out_pin.x - 10.0f * scale_, out_pin.y - 10.0f * scale_));
            ImGui::InvisibleButton("out_p", ImVec2(20.0f * scale_, 20.0f * scale_));
            if (ImGui::IsItemActive()) { isDraggingLink_ = true; draggingStartId_ = element.id; }

            if (isDraggingLink_ && ImGui::IsMouseHoveringRect(ImVec2(in_pin.x - 10.0f * scale_, in_pin.y - 10.0f * scale_), ImVec2(in_pin.x + 10.0f * scale_, in_pin.y + 10.0f * scale_)))
            {
                if (ImGui::IsMouseReleased(0) && draggingStartId_ != element.id)
                {
                    // ONLY compile this block if the element is an EditorAnimationState
                    if constexpr (std::is_same_v<ElementType, EditorAnimationState>)
                    {
                        EditorAnimationStateLink link; link.id = nextId_++; link.startId = draggingStartId_; link.endId = element.id;
                        link.transition = std::make_shared<AnimationTransition<AnimationState>>(); link.transition->target = element.animState;
                        for (auto& s : editorStates_) if (s.id == draggingStartId_) s.animState->outboundConnections.push_back(link.transition);
                        stateLinks_.push_back(link);
                    }
                    // ONLY compile this block if the element is an EditorAnimationNode
                    else if constexpr (std::is_same_v<ElementType, EditorAnimationNode>)
                    {
                        EditorAnimationNodeLink link; link.id = nextId_++; link.startId = draggingStartId_; link.endId = element.id;
                        link.transition = std::make_shared<AnimationTransition<AnimationNode>>(); link.transition->target = element.animNode;
                        for (auto& n : stateNodes_[viewingState_.get()]) if (n.id == draggingStartId_) n.animNode->outboundConnections.push_back(link.transition);
                        stateNodeLinks_[viewingState_.get()].push_back(link);
                    }
                    isDraggingLink_ = false;
                }
            }
            ImGui::PopID();
        }
        };

    if (currentViewMode_ == EditorAnimationViewMode::StateMachine) DrawElements(editorStates_, EditorAnimationSelectionType::State);
    else DrawElements(stateNodes_[viewingState_.get()], EditorAnimationSelectionType::Node);

    draw_list->ChannelsMerge();

    if (isDraggingLink_)
    {
        ImVec2 p1; bool found = false;
        if (currentViewMode_ == EditorAnimationViewMode::StateMachine) {
            for (auto& s : editorStates_) if (s.id == draggingStartId_) { p1 = ImVec2(offset.x + (s.pos.x + s.size.x) * scale_, offset.y + (s.pos.y + s.size.y / 2) * scale_); found = true; }
        }
        else {
            for (auto& n : stateNodes_[viewingState_.get()]) if (n.id == draggingStartId_) { p1 = ImVec2(offset.x + (n.pos.x + n.size.x) * scale_, offset.y + (n.pos.y + n.size.y / 2) * scale_); found = true; }
        }
        if (found) draw_list->AddBezierCubic(p1, ImVec2(p1.x + 50.0f * scale_, p1.y), ImVec2(ImGui::GetMousePos().x - 50.0f * scale_, ImGui::GetMousePos().y), ImGui::GetMousePos(), IM_COL32(255, 255, 255, 200), 3.0f * scale_);
        if (ImGui::IsMouseReleased(0)) isDraggingLink_ = false;
    }

    if (ImGui::IsWindowHovered())
    {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) { scrolling_.x += ImGui::GetIO().MouseDelta.x; scrolling_.y += ImGui::GetIO().MouseDelta.y; }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered()) ImGui::OpenPopup("CanvasMenu");
    }

    if (ImGui::BeginPopup("CanvasMenu"))
    {
        if (currentViewMode_ == EditorAnimationViewMode::StateMachine && ImGui::MenuItem("Add State"))
        {
            EditorAnimationState s; s.id = nextId_++; s.name = "New State";
            s.pos = ImVec2((ImGui::GetIO().MouseClickedPos[1].x - offset.x) / scale_, (ImGui::GetIO().MouseClickedPos[1].y - offset.y) / scale_);
            s.size = ImVec2(160, 50); s.animState = std::make_shared<AnimationState>(); s.animState->name = "New State";
            editorStates_.push_back(s); activeGraph_->AddState(s.animState);
        }
        else if (currentViewMode_ == EditorAnimationViewMode::InsideState && ImGui::MenuItem("Add Node"))
        {
            AddNodeToViewingState(
                AnimationNodeType::Clip,
                ImVec2((ImGui::GetIO().MouseClickedPos[1].x - offset.x) / scale_, (ImGui::GetIO().MouseClickedPos[1].y - offset.y) / scale_));
        }
        else if (currentViewMode_ == EditorAnimationViewMode::InsideState && ImGui::BeginMenu("Add Typed Node"))
        {
            const ImVec2 nodePosition(
                (ImGui::GetIO().MouseClickedPos[1].x - offset.x) / scale_,
                (ImGui::GetIO().MouseClickedPos[1].y - offset.y) / scale_);

            if (ImGui::MenuItem("Clip"))
            {
                AddNodeToViewingState(AnimationNodeType::Clip, nodePosition);
            }
            if (ImGui::MenuItem("Blend Space 1D"))
            {
                AddNodeToViewingState(AnimationNodeType::BlendSpace1D, nodePosition);
            }
            if (ImGui::MenuItem("Blend Space 2D"))
            {
                AddNodeToViewingState(AnimationNodeType::BlendSpace2D, nodePosition);
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
}
