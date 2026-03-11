#include "pch.h"

#include <sstream>
#include <queue>

#include "AnimationGraphPanel.h"
#include "UI/EditorHUD.h"
#include "Goknar/Animation/AnimationCondition.h"
#include "Goknar/Animation/AnimationGraph.h"
#include "Goknar/Animation/AnimationSerializer.h"
#include "Goknar/Animation/AnimationDeserializer.h"

#include "imgui_internal.h"
#include "tinyxml2.h"

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
}

AnimationGraphPanel::~AnimationGraphPanel()
{
}

void AnimationGraphPanel::Init()
{
    // Default empty state machine setup
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
    rootNode.size = ImVec2(150, 50);
    rootNode.animNode = std::make_shared<AnimationNode>();
    rootNode.animNode->animationName = "Idle";

    rootState.animState->SetEntryNode(rootNode.animNode);

    editorStates_.push_back(rootState);
    stateNodes_[rootState.animState.get()].push_back(rootNode);
    activeGraph_->AddState(rootState.animState);
}

void AnimationGraphPanel::Draw()
{
    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title_.c_str(), &isOpen_)) { ImGui::End(); return; }

    // Updated File Paths
    if (ImGui::Button("Save Graph")) SaveGraphToXML("Animations/AG_DefaultCharacter");
    ImGui::SameLine();
    if (ImGui::Button("Load Graph")) LoadGraphFromXML("Animations/AG_DefaultCharacter");
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
                else if constexpr (std::is_same_v<T, float>) ImGui::DragFloat("Value", &arg, 0.1f);
                else if constexpr (std::is_same_v<T, Vector2>) ImGui::DragFloat2("Value", &arg.x, 0.1f);
                else if constexpr (std::is_same_v<T, Vector2i>) ImGui::DragInt2("Value", &arg.x);
                else if constexpr (std::is_same_v<T, Vector3>) ImGui::DragFloat3("Value", &arg.x, 0.1f);
                else if constexpr (std::is_same_v<T, Vector3i>) ImGui::DragInt3("Value", &arg.x);
                else if constexpr (std::is_same_v<T, Vector4>) ImGui::DragFloat4("Value", &arg.x, 0.1f);
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
    for (const auto& transition : node->outboundConnections) CollectNodes(transition->target, visited, outNodes);
}

void AnimationGraphPanel::SaveGraphToXML(const std::string& filepath)
{
    AnimationSerializer serializer;
    serializer.Serialize(activeGraph_.get(), filepath);
}

void AnimationGraphPanel::LoadGraphFromXML(const std::string& filepath)
{
    activeGraph_ = std::make_shared<AnimationGraph>();
    AnimationDeserializer deserializer;
    if (!deserializer.Deserialize(activeGraph_.get(), filepath)) return;

    editorStates_.clear(); stateLinks_.clear();
    stateNodes_.clear(); stateNodeLinks_.clear();
    selectionType_ = EditorAnimationSelectionType::None;
    currentViewMode_ = EditorAnimationViewMode::StateMachine;
    viewingState_ = nullptr;

    std::unordered_map<const AnimationState*, int> stateToEditorId;
    std::unordered_map<const AnimationNode*, int> nodeToEditorId;

    // ==========================================
    // 1. STATE MACHINE AUTO-LAYOUT
    // ==========================================
    std::unordered_map<const AnimationState*, int> stateDepths;
    std::unordered_map<int, int> stateRowCounters;
    std::queue<std::pair<const AnimationState*, int>> stateQ;
    std::unordered_set<const AnimationState*> stateVisited;

    // Start BFS with the first state (root)
    if (!activeGraph_->GetStates().empty()) {
        auto rootState = activeGraph_->GetStates().front();
        stateQ.push({ rootState.get(), 0 });
        stateVisited.insert(rootState.get());
    }

    // Push any disconnected/floating states
    for (const auto& s : activeGraph_->GetStates()) {
        if (stateVisited.find(s.get()) == stateVisited.end()) {
            stateQ.push({ s.get(), 0 });
            stateVisited.insert(s.get());
        }
    }

    // Calculate hierarchical depths for states
    while (!stateQ.empty()) {
        auto [curr, depth] = stateQ.front(); stateQ.pop();
        stateDepths[curr] = depth;
        for (const auto& t : curr->outboundConnections) {
            if (stateVisited.find(t->target.get()) == stateVisited.end()) {
                stateVisited.insert(t->target.get());
                stateQ.push({ t->target.get(), depth + 1 });
            }
        }
    }

    // Rebuild UI States based on depth
    for (const auto& state : activeGraph_->GetStates())
    {
        EditorAnimationState es;
        es.id = nextId_++;
        es.name = state->name;
        es.animState = state;
        es.size = ImVec2(160, 50);

        int d = stateDepths[state.get()];
        int r = stateRowCounters[d]++;
        es.pos = ImVec2(50.0f + d * 250.0f, 50.0f + r * 150.0f);

        stateToEditorId[state.get()] = es.id;
        editorStates_.push_back(es);

        // ==========================================
        // 2. INTERNAL NODES AUTO-LAYOUT
        // ==========================================
        std::unordered_set<const AnimationNode*> visitedNodesForFlat;
        std::vector<std::shared_ptr<AnimationNode>> flatNodes;
        CollectNodes(state->GetEntryNode(), visitedNodesForFlat, flatNodes);

        std::unordered_map<const AnimationNode*, int> nodeDepths;
        std::unordered_map<int, int> nodeRowCounters;
        std::queue<std::pair<const AnimationNode*, int>> nodeQ;
        std::unordered_set<const AnimationNode*> nodeVisited;

        auto entryNode = state->GetEntryNode();
        if (entryNode) {
            nodeQ.push({ entryNode.get(), 0 });
            nodeVisited.insert(entryNode.get());
        }

        // Push any disconnected/floating nodes inside this state
        for (const auto& n : flatNodes) {
            if (nodeVisited.find(n.get()) == nodeVisited.end()) {
                nodeQ.push({ n.get(), 0 });
                nodeVisited.insert(n.get());
            }
        }

        // Calculate hierarchical depths for nodes
        while (!nodeQ.empty()) {
            auto [curr, depth] = nodeQ.front(); nodeQ.pop();
            nodeDepths[curr] = depth;
            for (const auto& t : curr->outboundConnections) {
                if (nodeVisited.find(t->target.get()) == nodeVisited.end()) {
                    nodeVisited.insert(t->target.get());
                    nodeQ.push({ t->target.get(), depth + 1 });
                }
            }
        }

        // Rebuild UI Nodes based on depth
        for (const auto& node : flatNodes)
        {
            EditorAnimationNode en;
            en.id = nextId_++;
            en.name = node->animationName.empty() ? "Node" : node->animationName;
            en.animNode = node;
            en.size = ImVec2(160, 50);

            int nd = nodeDepths[node.get()];
            int nr = nodeRowCounters[nd]++;
            en.pos = ImVec2(50.0f + nd * 250.0f, 50.0f + nr * 150.0f);

            nodeToEditorId[node.get()] = en.id;
            stateNodes_[state.get()].push_back(en);
        }
    }

    // ==========================================
    // 3. RECONSTRUCT LINKS
    // ==========================================
    for (const auto& state : activeGraph_->GetStates())
    {
        // State Links
        for (const auto& trans : state->outboundConnections)
        {
            EditorAnimationStateLink el;
            el.id = nextId_++;
            el.startId = stateToEditorId[state.get()];
            el.endId = stateToEditorId[trans->target.get()];
            el.transition = trans;
            stateLinks_.push_back(el);
        }

        // Node Links
        std::unordered_set<const AnimationNode*> visitedNodesForFlat;
        std::vector<std::shared_ptr<AnimationNode>> flatNodes;
        CollectNodes(state->GetEntryNode(), visitedNodesForFlat, flatNodes);

        for (const auto& node : flatNodes)
        {
            for (const auto& trans : node->outboundConnections)
            {
                EditorAnimationNodeLink el;
                el.id = nextId_++;
                el.startId = nodeToEditorId[node.get()];
                el.endId = nodeToEditorId[trans->target.get()];
                el.transition = trans;
                stateNodeLinks_[state.get()].push_back(el);
            }
        }
    }
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
            char buf[64]; strcpy_s(buf, it->animNode->animationName.c_str());
            if (ImGui::InputText("FBX Name", buf, sizeof(buf))) {
                it->animNode->animationName = buf;
                it->name = buf;
            }
            ImGui::Checkbox("Looping", &it->animNode->loop);

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

        if (selectionType_ == EditorAnimationSelectionType::StateLink) {
            auto it = std::find_if(stateLinks_.begin(), stateLinks_.end(), [&](const auto& l) { return l.id == selectedId_; });
            if (it != stateLinks_.end()) { conditions = &it->transition->conditions; transitDone = &it->transition->transitWhenAnimationDone; }
        }
        else {
            auto& links = stateNodeLinks_[viewingState_.get()];
            auto it = std::find_if(links.begin(), links.end(), [&](const auto& l) { return l.id == selectedId_; });
            if (it != links.end()) { conditions = &it->transition->conditions; transitDone = &it->transition->transitWhenAnimationDone; }
        }

        if (conditions && transitDone)
        {
            ImGui::Checkbox("Transit on Exit", transitDone);
            ImGui::Separator();
            ImGui::Text("Conditions");
            if (ImGui::Button("+ Add Condition")) conditions->push_back({ "NewVar", CompareOp::Equal, false });

            for (size_t i = 0; i < conditions->size(); ++i)
            {
                ImGui::PushID((int)i);
                auto& cond = (*conditions)[i];

                char vBuf[64]; strcpy_s(vBuf, cond.variableName.c_str());
                if (ImGui::InputText("Variable", vBuf, sizeof(vBuf))) cond.variableName = vBuf;

                const char* opNames[] = { "==", "!=", ">", "<", ">=", "<=" };
                int currentOp = (int)cond.operation;
                if (ImGui::Combo("Operator", &currentOp, opNames, 6)) cond.operation = (CompareOp)currentOp;

                float val = 0.0f;
                if (std::holds_alternative<float>(cond.targetValue)) val = std::get<float>(cond.targetValue);
                if (std::holds_alternative<bool>(cond.targetValue)) val = std::get<bool>(cond.targetValue) ? 1.0f : 0.0f;

                if (ImGui::DragFloat("Value", &val)) {
                    if (std::holds_alternative<bool>(cond.targetValue)) cond.targetValue = (val > 0.5f);
                    else cond.targetValue = val;
                }

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

            ImU32 bg_col = (selectionType_ == type && selectedId_ == element.id) ? IM_COL32(80, 80, 120, 255) : IM_COL32(50, 50, 50, 255);

            // ONLY compile this block if the element is an EditorAnimationNode
            if constexpr (std::is_same_v<ElementType, EditorAnimationNode>)
            {
                // Give the Entry Node a greener highlight
                if (viewingState_ && viewingState_->GetEntryNode() == element.animNode) {
                    bg_col = (selectionType_ == type && selectedId_ == element.id) ? IM_COL32(100, 120, 80, 255) : IM_COL32(50, 100, 50, 255);
                }
            }

            draw_list->AddRectFilled(rect_min, rect_max, bg_col, 4.0f * scale_);
            draw_list->AddRect(rect_min, rect_max, IM_COL32(100, 100, 100, 255), 4.0f * scale_);

            ImGui::SetCursorScreenPos(ImVec2(rect_min.x + 10.0f * scale_, rect_min.y + 15.0f * scale_));
            ImGui::SetWindowFontScale(scale_);
            ImGui::Text("%s", element.name.c_str());
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
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
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
            EditorAnimationNode n; n.id = nextId_++; n.name = "New Node";
            n.pos = ImVec2((ImGui::GetIO().MouseClickedPos[1].x - offset.x) / scale_, (ImGui::GetIO().MouseClickedPos[1].y - offset.y) / scale_);
            n.size = ImVec2(160, 50); n.animNode = std::make_shared<AnimationNode>();
            if (!viewingState_->GetEntryNode()) viewingState_->SetEntryNode(n.animNode); // Make first node the entry node
            stateNodes_[viewingState_.get()].push_back(n);
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
}