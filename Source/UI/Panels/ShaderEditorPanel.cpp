#include "ShaderEditorPanel.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <unordered_set>

#include "UI/EditorHUD.h"
#include "imgui_internal.h"

#include "Goknar/Renderer/ShaderTypes.h"
#include "Goknar/Materials/Material.h"
#include "Goknar/Materials/MaterialSerializer.h"


ShaderEditorPanel::ShaderEditorPanel(EditorHUD* hud)
    : IEditorPanel("Shader Graph Editor", hud)
{
}

void ShaderEditorPanel::Init()
{
    // Create the Master "Result" Node (like Unreal's Main Material Node)
    ShaderNode masterNode;
    masterNode.id = nextId_++;
    masterNode.name = "Material Output";
    masterNode.typeCategory = "Master";
    masterNode.pos = ImVec2(500, 300);
    masterNode.size = ImVec2(200, 150);

    masterNode.inputs.push_back({ nextId_++, masterNode.id, "Base Color", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(1.f) });
    masterNode.inputs.push_back({ nextId_++, masterNode.id, "Emissive", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f) });
    masterNode.inputs.push_back({ nextId_++, masterNode.id, "Normal", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f, 0.f, 1.f) });
    masterNode.inputs.push_back({ nextId_++, masterNode.id, "World Position Offset", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f) });

    masterNodeId_ = masterNode.id;
    nodes_.push_back(masterNode);

    // Create a sample Constant Node for demonstration
    ShaderNode colorNode;
    colorNode.id = nextId_++;
    colorNode.name = "Vector3 Constant";
    colorNode.typeCategory = "Constants";
    colorNode.pos = ImVec2(100, 300);
    colorNode.size = ImVec2(150, 80);
    
    colorNode.outputs.push_back({ nextId_++, colorNode.id, "Out", ShaderPinType::Vector3, ShaderPinKind::Output });
    nodes_.push_back(colorNode);
}

void ShaderEditorPanel::Draw()
{
    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title_.c_str(), &isOpen_)) { ImGui::End(); return; }

    if (ImGui::Button("Compile Material"))
    {
        if (activeMaterial_)
        {
            delete activeMaterial_;
        }

        activeMaterial_ = new Material();

        // 2. Retrieve the temporary InitializationData from the Material
        MaterialInitializationData* initData = activeMaterial_->GetInitializationData();

        if (initData)
        {
            // 3. Run the Graph Compiler to populate initData with GLSL strings
            CompileGraphToMaterial(initData);

            MaterialSerializer().Serialize("M_TestMaterial", activeMaterial_);

            // 4. Call the Material's Build function to generate and compile actual Shaders
            // We pass a null or dummy mesh unit if we just want to compile the shaders
            activeMaterial_->Build(nullptr);

            // 5. Re-initialize the shader on the GPU
            activeMaterial_->PreInit();
            activeMaterial_->Init();
            activeMaterial_->PostInit();
        }
    }

    ImGui::Separator();

    static ImGuiTableFlags rootFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;
    if (ImGui::BeginTable("ShaderEditorLayout", 2, rootFlags))
    {
        ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthFixed, 300.0f);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        DrawNodeCanvas();

        ImGui::TableSetColumnIndex(1);
        DrawPropertiesSidebar();

        ImGui::EndTable();
    }
    ImGui::End();
}

void ShaderEditorPanel::DrawNodeCanvas()
{
    ImGui::BeginChild("shader_scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    ImVec2 offset = ImVec2(canvas_p0.x + scrolling_.x, canvas_p0.y + scrolling_.y);

    // --- NEW: DESELECT ON EMPTY CLICK ---
    // If the left mouse is clicked on the canvas background (and not on a node/pin/link)
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        // ImGui::IsAnyItemActive() returns true if a button, slider, or node-drag is happening
        if (!ImGui::IsAnyItemActive())
        {
            selectedNodeId_ = -1;
            selectedLinkId_ = -1;
        }
    }

    // Canvas panning/zooming logic (matches your Animation graph)
    if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f)
    {
        float mouseWheel = ImGui::GetIO().MouseWheel;
        float oldScale = scale_;
        scale_ = ImClamp(scale_ + mouseWheel * 0.05f, 0.2f, 3.0f);

        ImVec2 mousePosInCanvas = ImVec2(ImGui::GetMousePos().x - canvas_p0.x, ImGui::GetMousePos().y - canvas_p0.y);
        ImVec2 mousePosInWorld = ImVec2((mousePosInCanvas.x - scrolling_.x) / oldScale, (mousePosInCanvas.y - scrolling_.y) / oldScale);
        scrolling_.x = mousePosInCanvas.x - (mousePosInWorld.x * scale_);
        scrolling_.y = mousePosInCanvas.y - (mousePosInWorld.y * scale_);
    }

    // Grid
    float GRID_SZ = 64.0f * scale_;
    ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
    for (float x = fmodf(scrolling_.x, GRID_SZ); x < canvas_sz.x; x += GRID_SZ) draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p0.y + canvas_sz.y), GRID_COLOR);
    for (float y = fmodf(scrolling_.y, GRID_SZ); y < canvas_sz.y; y += GRID_SZ) draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + y), GRID_COLOR);

    draw_list->ChannelsSplit(2);
    
    // --- DRAW LINKS ---
    draw_list->ChannelsSetCurrent(0);
    for (auto& link : links_)
    {
        ImVec2 startLocal = GetPinPosition(link.startPinId);
        ImVec2 endLocal = GetPinPosition(link.endPinId);

        // Convert canvas-local coordinates to screen-space coordinates
        ImVec2 startPos = ImVec2(offset.x + startLocal.x * scale_, offset.y + startLocal.y * scale_);
        ImVec2 endPos = ImVec2(offset.x + endLocal.x * scale_, offset.y + endLocal.y * scale_);

        ImVec2 cp1 = ImVec2(startPos.x + 50.0f * scale_, startPos.y);
        ImVec2 cp2 = ImVec2(endPos.x - 50.0f * scale_, endPos.y);

        ImU32 color = (selectedLinkId_ == link.id) ? IM_COL32(255, 200, 0, 255) : IM_COL32(200, 200, 200, 255);
        draw_list->AddBezierCubic(startPos, cp1, cp2, endPos, color, 3.0f * scale_);
    }

    // --- DRAW NODES ---
    draw_list->ChannelsSetCurrent(1);
    for (auto& node : nodes_)
    {
        ImGui::PushID(node.id);
        ImVec2 rect_min = ImVec2(offset.x + node.pos.x * scale_, offset.y + node.pos.y * scale_);
        ImVec2 scaled_size = ImVec2(node.size.x * scale_, node.size.y * scale_);
        ImVec2 rect_max = ImVec2(rect_min.x + scaled_size.x, rect_min.y + scaled_size.y);

        ImU32 bg_col = (selectedNodeId_ == node.id) ? IM_COL32(75, 75, 100, 255) : IM_COL32(40, 40, 40, 255);
        if (node.typeCategory == "Master") bg_col = IM_COL32(40, 80, 40, 255);

        // Background and Header
        draw_list->AddRectFilled(rect_min, rect_max, bg_col, 4.0f * scale_);
        draw_list->AddRectFilled(rect_min, ImVec2(rect_max.x, rect_min.y + 25.0f * scale_), IM_COL32(20, 20, 20, 255), 4.0f * scale_);
        draw_list->AddRect(rect_min, rect_max, IM_COL32(100, 100, 100, 255), 4.0f * scale_);

        ImGui::SetCursorScreenPos(ImVec2(rect_min.x + 8.0f * scale_, rect_min.y + 6.0f * scale_));
        ImGui::SetWindowFontScale(scale_);
        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "%s", node.name.c_str());
        ImGui::SetWindowFontScale(1.0f);

        // Node Dragging Hitbox (Header Only) - This prevents blocking UI inputs
        ImGui::SetCursorScreenPos(rect_min);
        // We restrict the invisible button height to 25.0f, which is the height of your header
        ImGui::InvisibleButton("node_header", ImVec2(scaled_size.x, 25.0f * scale_));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
        {
            node.pos.x += ImGui::GetIO().MouseDelta.x / scale_;
            node.pos.y += ImGui::GetIO().MouseDelta.y / scale_;
        }
        // Also allow selecting the node by clicking the header
        if (ImGui::IsItemClicked()) selectedNodeId_ = node.id;


        // --- NEW: INLINE OUTPUT EDITORS ---
        // For nodes like Float Constant that have no inputs, but need to edit their raw output value
        if (node.name == "Float Constant" && !node.outputs.empty())
        {
            // Align the box vertically with the output pin
            ImGui::SetCursorScreenPos(ImVec2(rect_min.x + 15.0f * scale_, rect_min.y + 35.0f * scale_ - 10.0f * scale_));
            ImGui::PushItemWidth(60.0f * scale_);

            float val = 0.0f;
            if (std::holds_alternative<float>(node.outputs[0].defaultValue))
            {
                val = std::get<float>(node.outputs[0].defaultValue);
            }

            if (ImGui::DragFloat((std::string("##out_val_") + std::to_string(node.id)).c_str(), &val, 0.01f))
            {
                node.outputs[0].defaultValue = val;
            }
            ImGui::PopItemWidth();
        }

        // --- DRAW INPUTS (Left) ---
        float pinY = 35.0f * scale_;
        for (auto& input : node.inputs)
        {
            ImVec2 pinPos = ImVec2(rect_min.x, rect_min.y + pinY);
            draw_list->AddCircleFilled(pinPos, 6.0f * scale_, IM_COL32(150, 150, 250, 255));

            ImGui::SetCursorScreenPos(ImVec2(rect_min.x + 15.0f * scale_, rect_min.y + pinY - 8.0f * scale_));
            ImGui::SetWindowFontScale(scale_);
            ImGui::Text("%s", input.name.c_str());

            // Check if connected
            bool isConnected = false;
            for (const auto& link : links_) {
                if (link.endPinId == input.id) { isConnected = true; break; }
            }

            // Draw inline input box if NOT connected
            if (!isConnected && input.type == ShaderPinType::Float)
            {
                ImGui::SetCursorScreenPos(ImVec2(rect_min.x + 40.0f * scale_, rect_min.y + pinY - 10.0f * scale_));
                ImGui::PushItemWidth(60.0f * scale_);

                float val = 0.0f;
                if (std::holds_alternative<float>(input.defaultValue)) val = std::get<float>(input.defaultValue);

                if (ImGui::DragFloat((std::string("##in_val_") + std::to_string(input.id)).c_str(), &val, 0.01f))
                {
                    input.defaultValue = val;
                }
                ImGui::PopItemWidth();
            }
            ImGui::SetWindowFontScale(1.0f);

            // Pin Hitbox
            ImGui::SetCursorScreenPos(ImVec2(pinPos.x - 10.0f * scale_, pinPos.y - 10.0f * scale_));
            ImGui::InvisibleButton((std::string("in_") + std::to_string(input.id)).c_str(), ImVec2(20.0f * scale_, 20.0f * scale_));

            // Detect when the mouse is released over this input pin while dragging a wire
            if (isDraggingLink_ && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseReleased(0))
            {
                // --- NEW: SINGLE INPUT ENFORCEMENT ---
                // Search for and remove any existing link that ends at this specific input pin
                links_.erase(std::remove_if(links_.begin(), links_.end(),
                    [&input](const ShaderLink& link) {
                        return link.endPinId == input.id;
                    }), links_.end());

                // Now add the new link
                links_.push_back({ nextId_++, draggingStartPinId_, input.id });
                isDraggingLink_ = false;
            }
            pinY += 25.0f * scale_;
        }

        // --- DRAW OUTPUTS (Right) ---
        pinY = 35.0f * scale_;
        for (auto& output : node.outputs)
        {
            ImVec2 pinPos = ImVec2(rect_max.x, rect_min.y + pinY);
            draw_list->AddCircleFilled(pinPos, 6.0f * scale_, IM_COL32(250, 150, 150, 255));

            float textSize = ImGui::CalcTextSize(output.name.c_str()).x * scale_;
            ImGui::SetCursorScreenPos(ImVec2(rect_max.x - textSize - 15.0f * scale_, rect_min.y + pinY - 8.0f * scale_));
            ImGui::SetWindowFontScale(scale_);
            ImGui::Text("%s", output.name.c_str());
            ImGui::SetWindowFontScale(1.0f);

            // Pin Hitbox
            ImGui::SetCursorScreenPos(ImVec2(pinPos.x - 10.0f * scale_, pinPos.y - 10.0f * scale_));
            ImGui::InvisibleButton((std::string("out_") + std::to_string(output.id)).c_str(), ImVec2(20.0f * scale_, 20.0f * scale_));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
            {
                isDraggingLink_ = true;
                draggingStartPinId_ = output.id;
            }
            pinY += 25.0f * scale_;
        }

        ImGui::PopID();
    }
    draw_list->ChannelsMerge();

    // Draw active dragging link
    if (isDraggingLink_)
    {
        ImVec2 startLocal = GetPinPosition(draggingStartPinId_);
        ImVec2 startPos = ImVec2(offset.x + startLocal.x * scale_, offset.y + startLocal.y * scale_);
        ImVec2 endPos = ImGui::GetMousePos();
        draw_list->AddBezierCubic(startPos, ImVec2(startPos.x + 50.0f * scale_, startPos.y), ImVec2(endPos.x - 50.0f * scale_, endPos.y), endPos, IM_COL32(255, 255, 255, 200), 3.0f * scale_);

        if (ImGui::IsMouseReleased(0)) isDraggingLink_ = false;
    }

    // Canvas panning
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
    {
        scrolling_.x += ImGui::GetIO().MouseDelta.x;
        scrolling_.y += ImGui::GetIO().MouseDelta.y;
    }

    // Right Click to open menu
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered())
    {
        ImGui::OpenPopup("CanvasMenu");
    }

    if (ImGui::BeginPopup("CanvasMenu"))
    {
        // Get mouse position at the time of the right click (Index 1 = Right Mouse Button)
        ImVec2 openPos = ImGui::GetIO().MouseClickedPos[1];
        // Convert screen coordinates to canvas space coordinates
        ImVec2 spawnPos = ImVec2((openPos.x - offset.x) / scale_, (openPos.y - offset.y) / scale_);

        if (ImGui::BeginMenu("Variables"))
        {
            if (ImGui::BeginMenu("Positioning"))
            {
                if (ImGui::MenuItem("Bone Transformation Matrix")) nodes_.push_back(SpawnNode("Variables", "Bone Transformation Matrix", spawnPos));
                if (ImGui::MenuItem("World Transformation Matrix")) nodes_.push_back(SpawnNode("Variables", "World Transformation Matrix", spawnPos));
                if (ImGui::MenuItem("Relative Transformation Matrix")) nodes_.push_back(SpawnNode("Variables", "Relative Transformation Matrix", spawnPos));
                if (ImGui::MenuItem("Model Matrix")) nodes_.push_back(SpawnNode("Variables", "Model Matrix", spawnPos));
                if (ImGui::MenuItem("View Projection Matrix")) nodes_.push_back(SpawnNode("Variables", "View Projection Matrix", spawnPos));
                if (ImGui::MenuItem("Transformation Matrix")) nodes_.push_back(SpawnNode("Variables", "Transformation Matrix", spawnPos));
                if (ImGui::MenuItem("View Position")) nodes_.push_back(SpawnNode("Variables", "View Position", spawnPos));
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Vertex Shader Outs"))
            {
                if (ImGui::MenuItem("Final Model Matrix")) nodes_.push_back(SpawnNode("Variables", "Final Model Matrix", spawnPos));
                if (ImGui::MenuItem("Fragment Position World Space")) nodes_.push_back(SpawnNode("Variables", "Fragment Position World Space", spawnPos));
                if (ImGui::MenuItem("Fragment Position Screen Space")) nodes_.push_back(SpawnNode("Variables", "Fragment Position Screen Space", spawnPos));
                if (ImGui::MenuItem("Vertex Normal")) nodes_.push_back(SpawnNode("Variables", "Vertex Normal", spawnPos));
                if (ImGui::MenuItem("Vertex Color")) nodes_.push_back(SpawnNode("Variables", "Vertex Color", spawnPos));
                if (ImGui::MenuItem("Bone IDs")) nodes_.push_back(SpawnNode("Variables", "Bone IDs", spawnPos));
                if (ImGui::MenuItem("Weights")) nodes_.push_back(SpawnNode("Variables", "Weights", spawnPos));
                if (ImGui::MenuItem("Directional Light Space Pos")) nodes_.push_back(SpawnNode("Variables", "Directional Light Space Pos", spawnPos));
                if (ImGui::MenuItem("Spot Light Space Pos")) nodes_.push_back(SpawnNode("Variables", "Spot Light Space Pos", spawnPos));
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Math"))
        {
            if (ImGui::MenuItem("Add (Vector3)")) nodes_.push_back(SpawnNode("Math", "Add (Vector3)", spawnPos));
            if (ImGui::MenuItem("Multiply (Vector3)")) nodes_.push_back(SpawnNode("Math", "Multiply (Vector3)", spawnPos));
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Constants"))
        {
            if (ImGui::MenuItem("Float Constant")) nodes_.push_back(SpawnNode("Constants", "Float Constant", spawnPos));
            if (ImGui::MenuItem("Vector2 Constant")) nodes_.push_back(SpawnNode("Constants", "Vector2 Constant", spawnPos));
            if (ImGui::MenuItem("Vector3 Constant")) nodes_.push_back(SpawnNode("Constants", "Vector3 Constant", spawnPos));
            if (ImGui::MenuItem("Vector4 Constant")) nodes_.push_back(SpawnNode("Constants", "Vector4 Constant", spawnPos));
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    // --- HANDLE DELETIONS (Nodes and Links) ---
    // Only process deletes if our canvas window is actively focused
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)))
    {
        // 1. Delete Selected Node
        if (selectedNodeId_ != -1 && selectedNodeId_ != masterNodeId_)
        {
            ShaderNode* nodeToDelete = FindNode(selectedNodeId_);
            if (nodeToDelete)
            {
                // Collect all pin IDs belonging to this node
                std::vector<int> nodePinIds;
                for (const auto& pin : nodeToDelete->inputs) nodePinIds.push_back(pin.id);
                for (const auto& pin : nodeToDelete->outputs) nodePinIds.push_back(pin.id);

                // Remove all links connected to any of these pins
                links_.erase(std::remove_if(links_.begin(), links_.end(),
                    [&nodePinIds](const ShaderLink& link) {
                        return std::find(nodePinIds.begin(), nodePinIds.end(), link.startPinId) != nodePinIds.end() ||
                            std::find(nodePinIds.begin(), nodePinIds.end(), link.endPinId) != nodePinIds.end();
                    }), links_.end());

                // Remove the node itself
                nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
                    [this](const ShaderNode& node) { return node.id == selectedNodeId_; }), nodes_.end());

                selectedNodeId_ = -1;
            }
        }
        // 2. Delete Selected Link (if no node is selected but a link is)
        else if (selectedLinkId_ != -1)
        {
            links_.erase(std::remove_if(links_.begin(), links_.end(),
                [this](const ShaderLink& link) { return link.id == selectedLinkId_; }), links_.end());

            selectedLinkId_ = -1;
        }
    }

    ImGui::EndChild();
}

void ShaderEditorPanel::DrawPropertiesSidebar()
{
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Node Properties");
    ImGui::Separator();

    if (selectedNodeId_ != -1)
    {
        ShaderNode* node = FindNode(selectedNodeId_);
        if (node)
        {
            ImGui::Text("Name: %s", node->name.c_str());
            ImGui::Text("Type: %s", node->typeCategory.c_str());

            ImGui::Separator();
            ImGui::Text("Inputs");

            for (auto& pin : node->inputs)
            {
                // 1. Check if the pin is connected
                bool isConnected = false;
                std::string connectedNodeName = "";

                for (const auto& link : links_) {
                    if (link.endPinId == pin.id) {
                        isConnected = true;
                        ShaderPin* startPin = FindPin(link.startPinId);
                        if (startPin) {
                            ShaderNode* startNode = FindNode(startPin->nodeId);
                            if (startNode) connectedNodeName = startNode->name;
                        }
                        break;
                    }
                }

                ImGui::PushID(pin.id);

                // 2. If connected, hide the input widget and just show info text
                if (isConnected)
                {
                    ImGui::TextDisabled("%s: Linked to %s", pin.name.c_str(), connectedNodeName.c_str());
                }
                // 3. If not connected, show the editable widgets
                else
                {
                    if (pin.type == ShaderPinType::Vector3)
                    {
                        Vector3 val = Vector3(0.f);
                        if (std::holds_alternative<Vector3>(pin.defaultValue))
                            val = std::get<Vector3>(pin.defaultValue);

                        if (ImGui::ColorEdit3(pin.name.c_str(), &val.x))
                            pin.defaultValue = val;
                    }
                    else if (pin.type == ShaderPinType::Float)
                    {
                        float val = 0.0f;
                        if (std::holds_alternative<float>(pin.defaultValue))
                            val = std::get<float>(pin.defaultValue);

                        if (ImGui::DragFloat(pin.name.c_str(), &val, 0.01f))
                            pin.defaultValue = val;
                    }
                }

                ImGui::PopID();
            }
        }
    }
    else
    {
        ImGui::TextDisabled("Select a node to edit.");
    }
}

ShaderNode ShaderEditorPanel::SpawnNode(const std::string& category, const std::string& name, ImVec2 pos)
{
    ShaderNode node;
    node.id = nextId_++;
    node.name = name;
    node.typeCategory = category;
    node.pos = pos;
    node.size = ImVec2(200, 80); // Default size, gets adjusted below

    if (category == "Variables")
    {
        ShaderPinType type = ShaderPinType::Matrix4x4;

        // Determine the correct type based on the variable name
        if (name == "View Position" || name == "Vertex Normal") type = ShaderPinType::Vector3;
        else if (name == "Fragment Position World Space" || name == "Fragment Position Screen Space" ||
            name == "Vertex Color" || name == "Weights" ||
            name == "Directional Light Space Pos" || name == "Spot Light Space Pos") type = ShaderPinType::Vector4;
        else if (name == "Bone IDs") type = ShaderPinType::Vector4i;

        node.outputs.push_back({ nextId_++, node.id, "Value", type, ShaderPinKind::Output });
        node.size = ImVec2(220, 60);
    }
    else if (category == "Constants")
    {
        if (name == "Float Constant") {
            node.outputs.push_back({ nextId_++, node.id, "Value", ShaderPinType::Float, ShaderPinKind::Output, 0.0f });
            node.size = ImVec2(150, 60);
        }
        else if (name == "Vector2 Constant") {
            node.inputs.push_back({ nextId_++, node.id, "X", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
            node.inputs.push_back({ nextId_++, node.id, "Y", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
            node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Vector2, ShaderPinKind::Output });
            node.size = ImVec2(160, 95);
        }
        else if (name == "Vector3 Constant") {
            node.inputs.push_back({ nextId_++, node.id, "X", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
            node.inputs.push_back({ nextId_++, node.id, "Y", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
            node.inputs.push_back({ nextId_++, node.id, "Z", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
            node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Vector3, ShaderPinKind::Output });
            node.size = ImVec2(160, 120);
        }
        else if (name == "Vector4 Constant") {
            node.inputs.push_back({ nextId_++, node.id, "X", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
            node.inputs.push_back({ nextId_++, node.id, "Y", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
            node.inputs.push_back({ nextId_++, node.id, "Z", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
            node.inputs.push_back({ nextId_++, node.id, "W", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
            node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Vector4, ShaderPinKind::Output });
            node.size = ImVec2(160, 145);
        }
    }
    else if (category == "Math")
    {
        if (name == "Add (Vector3)" || name == "Multiply (Vector3)") {
            node.inputs.push_back({ nextId_++, node.id, "A", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f) });
            node.inputs.push_back({ nextId_++, node.id, "B", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f) });
            node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Vector3, ShaderPinKind::Output });
            node.size = ImVec2(180, 100);
        }
    }

    return node;
}

ShaderNode ShaderEditorPanel::CreateVariableNode(const std::string& name, ShaderPinType type, ImVec2 pos)
{
    ShaderNode node;
    node.id = nextId_++;
    node.name = name;
    node.typeCategory = "Variables";
    node.pos = pos;
    node.size = ImVec2(200, 60);

    // Variable nodes only have ONE output pin and zero inputs.
    node.outputs.push_back({ nextId_++, node.id, "Value", type, ShaderPinKind::Output });

    return node;
}

// Example of how you would populate them (e.g., in a Context Menu or Init list)
void ShaderEditorPanel::PopulateVariableNodes()
{
    // --- POSITIONING ---
    nodes_.push_back(CreateVariableNode("Bone Transformation Matrix", ShaderPinType::Matrix4x4, ImVec2(100, 100)));
    nodes_.push_back(CreateVariableNode("World Transformation Matrix", ShaderPinType::Matrix4x4, ImVec2(100, 200)));
    nodes_.push_back(CreateVariableNode("Relative Transformation Matrix", ShaderPinType::Matrix4x4, ImVec2(100, 300)));
    nodes_.push_back(CreateVariableNode("Model Matrix", ShaderPinType::Matrix4x4, ImVec2(100, 400)));
    nodes_.push_back(CreateVariableNode("View Projection Matrix", ShaderPinType::Matrix4x4, ImVec2(100, 500)));
    nodes_.push_back(CreateVariableNode("Transformation Matrix", ShaderPinType::Matrix4x4, ImVec2(100, 600)));
    nodes_.push_back(CreateVariableNode("View Position", ShaderPinType::Vector3, ImVec2(100, 700)));

    // --- VERTEX SHADER OUTS (Fragment Shader Ins) ---
    nodes_.push_back(CreateVariableNode("Final Model Matrix", ShaderPinType::Matrix4x4, ImVec2(400, 100)));
    nodes_.push_back(CreateVariableNode("Fragment Position World Space", ShaderPinType::Vector4, ImVec2(400, 200)));
    nodes_.push_back(CreateVariableNode("Fragment Position Screen Space", ShaderPinType::Vector4, ImVec2(400, 300)));
    nodes_.push_back(CreateVariableNode("Vertex Normal", ShaderPinType::Vector3, ImVec2(400, 400)));
    nodes_.push_back(CreateVariableNode("Vertex Color", ShaderPinType::Vector4, ImVec2(400, 500)));
    nodes_.push_back(CreateVariableNode("Bone IDs", ShaderPinType::Vector4i, ImVec2(400, 600)));
    nodes_.push_back(CreateVariableNode("Weights", ShaderPinType::Vector4, ImVec2(400, 700)));

    // Light Space Arrays (Treating as generic/Vector4 representations for the graph)
    nodes_.push_back(CreateVariableNode("Directional Light Space Pos", ShaderPinType::Vector4, ImVec2(400, 800)));
    nodes_.push_back(CreateVariableNode("Spot Light Space Pos", ShaderPinType::Vector4, ImVec2(400, 900)));
}

ImVec2 ShaderEditorPanel::GetPinPosition(int pinId)
{
    for (auto& node : nodes_)
    {
        float pinY = 35.0f;
        for (auto& input : node.inputs) {
            if (input.id == pinId) return ImVec2(node.pos.x, node.pos.y + pinY);
            pinY += 25.0f;
        }

        pinY = 35.0f;
        for (auto& output : node.outputs) {
            if (output.id == pinId) return ImVec2(node.pos.x + node.size.x, node.pos.y + pinY);
            pinY += 25.0f;
        }
    }
    return ImVec2(0, 0);
}

ShaderPin* ShaderEditorPanel::FindPin(int pinId)
{
    for (auto& node : nodes_) {
        for (auto& input : node.inputs) if (input.id == pinId) return &input;
        for (auto& output : node.outputs) if (output.id == pinId) return &output;
    }
    return nullptr;
}

ShaderNode* ShaderEditorPanel::FindNode(int nodeId)
{
    for (auto& node : nodes_) {
        if (node.id == nodeId) return &node;
    }
    return nullptr;
}

void ShaderEditorPanel::CompileGraphToMaterial(MaterialInitializationData* outMaterialData)
{
    if (!outMaterialData)
    {
        return;
    }

    // 1. Clear previous generation
    outMaterialData->baseColor.calculation = "";
    outMaterialData->baseColor.result = "";
    outMaterialData->emmisiveColor.calculation = "";
    outMaterialData->emmisiveColor.result = "";
    outMaterialData->fragmentNormal.calculation = "";
    outMaterialData->fragmentNormal.result = "";
    outMaterialData->vertexPositionOffset.calculation = "";
    outMaterialData->vertexPositionOffset.result = "";

    ShaderNode* master = FindNode(masterNodeId_);
    if (!master) return;

    // 2. Topological Sort (Depth-First Search)
    // We order the nodes so dependencies are calculated first, ensuring variables exist before they are used.
    std::vector<ShaderNode*> orderedNodes;
    std::unordered_set<int> visitedNodes;

    // Recursive lambda to walk backward through the graph links
    std::function<void(int)> resolveDependencies = [&](int nodeId)
        {
            // Prevent infinite loops from cyclic connections
            if (visitedNodes.find(nodeId) != visitedNodes.end()) return;
            visitedNodes.insert(nodeId);

            ShaderNode* node = FindNode(nodeId);
            if (!node) return;

            // Check all inputs of this node to see what they are connected to
            for (const auto& inputPin : node->inputs)
            {
                for (const auto& link : links_)
                {
                    if (link.endPinId == inputPin.id)
                    {
                        ShaderPin* connectedOutput = FindPin(link.startPinId);
                        if (connectedOutput)
                        {
                            resolveDependencies(connectedOutput->nodeId);
                        }
                    }
                }
            }

            // Add to ordered list AFTER dependencies are resolved
            // We exclude the master node because it doesn't generate its own local variables
            if (nodeId != masterNodeId_)
            {
                orderedNodes.push_back(node);
            }
        };

    // Kick off the sort from the master node
    resolveDependencies(masterNodeId_);

    // 3. GLSL Code Generation
    std::string glslBody = "\n\t// --- ShaderEditorPanel Generated Graph ---\n";

    // Helper lambda to get the value of a pin
    auto GetPinValue = [&](const ShaderPin& pin) -> std::string
        {
            for (const auto& link : links_)
            {
                if (link.endPinId == pin.id)
                {
                    ShaderPin* outputPin = FindPin(link.startPinId);
                    ShaderNode* sourceNode = FindNode(outputPin->nodeId);

                    // --- Intercept Engine Variables ---
                    // If the source node is a Variable, inject the Goknar Engine macro directly
                    if (sourceNode->typeCategory == "Variables")
                    {
                        // POSITIONING
                        if (sourceNode->name == "Bone Transformation Matrix") return std::string(SHADER_VARIABLE_NAMES::POSITIONING::BONE_TRANSFORMATION_MATRIX);
                        if (sourceNode->name == "World Transformation Matrix") return std::string(SHADER_VARIABLE_NAMES::POSITIONING::WORLD_TRANSFORMATION_MATRIX);
                        if (sourceNode->name == "Relative Transformation Matrix") return std::string(SHADER_VARIABLE_NAMES::POSITIONING::RELATIVE_TRANSFORMATION_MATRIX);
                        if (sourceNode->name == "Model Matrix") return std::string(SHADER_VARIABLE_NAMES::POSITIONING::MODEL_MATRIX);
                        if (sourceNode->name == "View Projection Matrix") return std::string(SHADER_VARIABLE_NAMES::POSITIONING::VIEW_PROJECTION_MATRIX);
                        if (sourceNode->name == "Transformation Matrix") return std::string(SHADER_VARIABLE_NAMES::POSITIONING::TRANSFORMATION_MATRIX);
                        if (sourceNode->name == "View Position") return std::string(SHADER_VARIABLE_NAMES::POSITIONING::VIEW_POSITION);

                        // VERTEX SHADER OUTS
                        if (sourceNode->name == "Final Model Matrix") return std::string(SHADER_VARIABLE_NAMES::VERTEX_SHADER_OUTS::FINAL_MODEL_MATRIX);
                        if (sourceNode->name == "Fragment Position World Space") return std::string(SHADER_VARIABLE_NAMES::VERTEX_SHADER_OUTS::FRAGMENT_POSITION_WORLD_SPACE);
                        if (sourceNode->name == "Fragment Position Screen Space") return std::string(SHADER_VARIABLE_NAMES::VERTEX_SHADER_OUTS::FRAGMENT_POSITION_SCREEN_SPACE);
                        if (sourceNode->name == "Vertex Normal") return std::string(SHADER_VARIABLE_NAMES::VERTEX_SHADER_OUTS::VERTEX_NORMAL);
                        if (sourceNode->name == "Vertex Color") return std::string(SHADER_VARIABLE_NAMES::VERTEX_SHADER_OUTS::VERTEX_COLOR);
                        if (sourceNode->name == "Bone IDs") return std::string(SHADER_VARIABLE_NAMES::VERTEX_SHADER_OUTS::BONE_IDS);
                        if (sourceNode->name == "Weights") return std::string(SHADER_VARIABLE_NAMES::VERTEX_SHADER_OUTS::WEIGHTS);
                        if (sourceNode->name == "Directional Light Space Pos") return std::string(SHADER_VARIABLE_NAMES::VERTEX_SHADER_OUTS::DIRECTIONAL_LIGHT_SPACE_FRAGMENT_POSITIONS);
                        if (sourceNode->name == "Spot Light Space Pos") return std::string(SHADER_VARIABLE_NAMES::VERTEX_SHADER_OUTS::SPOT_LIGHT_SPACE_FRAGMENT_POSITIONS);
                    }

                    // If it's a standard math/logic node, return the generated local variable
                    return "node_" + std::to_string(sourceNode->id) + "_out_" + std::to_string(outputPin->id);
                }
            }

            // Not connected: Parse the default value
            if (pin.type == ShaderPinType::Vector3)
            {
                Vector3 v = std::get<Vector3>(pin.defaultValue);
                return "vec3(" + std::to_string(v.x) + "f, " + std::to_string(v.y) + "f, " + std::to_string(v.z) + "f)";
            }
            else if (pin.type == ShaderPinType::Float)
            {
                float f = std::get<float>(pin.defaultValue);
                return std::to_string(f) + "f";
            }
            // ... Handle Vector2, Vector4, Matrix4x4 defaults here if needed

            return "0.0f";
        };

    // Iterate through the sorted nodes and generate their specific GLSL operations
    for (ShaderNode* node : orderedNodes)
    {
        // Skip code generation for Variable nodes, as they don't need local variable declarations.
        if (node->typeCategory == "Variables")
        {
            continue;
        }

        glslBody += "\t// Node: " + node->name + "\n";

        if (node->typeCategory == "Constants")
        {
            if (node->name == "Float Constant")
            {
                std::string outVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[0].id);
                float val = 0.0f;
                if (std::holds_alternative<float>(node->outputs[0].defaultValue)) val = std::get<float>(node->outputs[0].defaultValue);
                glslBody += "\tfloat " + outVar + " = " + std::to_string(val) + "f;\n";
            }
            else if (node->name == "Vector2 Constant")
            {
                std::string x = GetPinValue(node->inputs[0]);
                std::string y = GetPinValue(node->inputs[1]);
                std::string outVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[0].id);
                glslBody += "\tvec2 " + outVar + " = vec2(" + x + ", " + y + ");\n";
            }
            else if (node->name == "Vector3 Constant")
            {
                // Fetch the values or connected variable names from the X, Y, Z pins
                std::string x = GetPinValue(node->inputs[0]);
                std::string y = GetPinValue(node->inputs[1]);
                std::string z = GetPinValue(node->inputs[2]);

                std::string outVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[0].id);

                // Construct the GLSL string dynamically
                glslBody += "\tvec3 " + outVar + " = vec3(" + x + ", " + y + ", " + z + ");\n";
            }
            else if (node->name == "Vector4 Constant")
            {
                std::string x = GetPinValue(node->inputs[0]);
                std::string y = GetPinValue(node->inputs[1]);
                std::string z = GetPinValue(node->inputs[2]);
                std::string w = GetPinValue(node->inputs[3]);
                std::string outVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[0].id);
                glslBody += "\tvec4 " + outVar + " = vec4(" + x + ", " + y + ", " + z + ", " + w + ");\n";
            }
        }
        else if (node->typeCategory == "Math")
        {
            if (node->name == "Add (Vector3)")
            {
                std::string a = GetPinValue(node->inputs[0]);
                std::string b = GetPinValue(node->inputs[1]);
                std::string outVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[0].id);

                glslBody += "\tvec3 " + outVar + " = " + a + " + " + b + ";\n";
            }
            else if (node->name == "Multiply (Vector3)")
            {
                std::string a = GetPinValue(node->inputs[0]);
                std::string b = GetPinValue(node->inputs[1]);
                std::string outVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[0].id);

                glslBody += "\tvec3 " + outVar + " = " + a + " * " + b + ";\n";
            }
            // ... Add Lerp, Dot, Cross, etc. here
        }

        glslBody += "\n";
    }

    // Assign the generated logic to the calculation blocks
    outMaterialData->baseColor.calculation = glslBody;

    // 4. Assign Final Results to Goknar Outputs
    for (const auto& masterInput : master->inputs)
    {
        std::string finalValue = GetPinValue(masterInput);

        if (masterInput.name == "Base Color")
        {
            outMaterialData->baseColor.result = "vec4(" + finalValue + ", 1.0f);";
        }
        else if (masterInput.name == "Emissive")
        {
            outMaterialData->emmisiveColor.result = finalValue + ";";
        }
        else if (masterInput.name == "Normal")
        {
            outMaterialData->fragmentNormal.result = finalValue + ";";
        }
        else if (masterInput.name == "World Position Offset")
        {
            // Vertex Position Offset goes to a different calculation block in Goknar, 
            // but we can route the result here if the nodes calculated it
            outMaterialData->vertexPositionOffset.result = finalValue + ";";
        }
    }
}