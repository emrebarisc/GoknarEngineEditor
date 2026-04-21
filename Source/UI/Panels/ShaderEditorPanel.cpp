#include "ShaderEditorPanel.h"

#include <functional>
#include <unordered_set>
#include <cctype>
#include <sstream>
#include <algorithm>

#include "tinyxml2.h"
#include "Goknar/Log.h"

#include "Goknar/Renderer/RenderTarget.h"
#include "Goknar/Renderer/Texture.h"
#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Camera.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Model/StaticMeshInstance.h"
#include "Goknar/Engine.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Materials/MaterialSerializer.h"
#include "Objects/MeshViewerCameraObject.h"
#include "Controllers/MeshViewerCameraController.h"
#include "UI/EditorUtils.h"

constexpr unsigned int SHADER_EDITOR_RENDER_MASK = 0x20000000;

ShaderEditorPanel::ShaderEditorPanel(EditorHUD* hud)
    : IEditorPanel("Shader Graph Editor", hud)
{
    // --- Initialize Preview Camera ---
    cameraObject_ = new MeshViewerCameraObject();
    cameraObject_->SetName("__ShaderEditor__PreviewCamera");
    cameraObject_->GetCameraComponent()->GetCamera()->SetCameraType(CameraType::RenderTarget);
    cameraObject_->GetCameraComponent()->GetCamera()->SetRenderMask(SHADER_EDITOR_RENDER_MASK);
    cameraObject_->SetWorldPosition({ 0.f, 0.f, 100.f });

    // --- Initialize Preview Render Target ---
    renderTarget_ = new RenderTarget();
    renderTarget_->SetCamera(cameraObject_->GetCameraComponent()->GetCamera());
    renderTarget_->SetRerenderShadowMaps(false);

    // --- Initialize Preview Object ---
    viewedObject_ = new ObjectBase();
    viewedObject_->SetName("__ShaderEditor__PreviewTarget");
    viewedObject_->SetWorldPosition({ 0.f, 0.f, 100.f });

    staticMeshComponent_ = viewedObject_->AddSubComponent<StaticMeshComponent>();
    staticMeshComponent_->GetMeshInstance()->SetRenderMask(SHADER_EDITOR_RENDER_MASK);

    StaticMesh* previewMesh = EditorUtils::GetEditorContent<StaticMesh>("Meshes/SM_MaterialSphere.fbx");
    if (previewMesh)
    {
        staticMeshComponent_->SetMesh(previewMesh);
        staticMeshComponent_->SetIsActive(true);
    }
}

ShaderEditorPanel::~ShaderEditorPanel()
{
    delete renderTarget_;
    cameraObject_->Destroy();
    viewedObject_->Destroy();
}

void ShaderEditorPanel::Init()
{
    ShaderNode masterNode;
    masterNode.id = nextId_++;
    masterNode.name = "Material Output";
    masterNode.typeCategory = "Master";
    masterNode.pos = ImVec2(500, 300);
    masterNode.size = ImVec2(200, 150);

    masterNode.inputs.push_back({ nextId_++, masterNode.id, "Base Color", ShaderPinType::Vector4, ShaderPinKind::Input, Vector4(1.f) });
    masterNode.inputs.push_back({ nextId_++, masterNode.id, "Emissive", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f) });
    masterNode.inputs.push_back({ nextId_++, masterNode.id, "Normal", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f, 0.f, 1.f) });
    masterNode.inputs.push_back({ nextId_++, masterNode.id, "World Position Offset", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f) });

    masterNodeId_ = masterNode.id;
    nodes_.push_back(masterNode);

    renderTarget_->Init();
    renderTarget_->SetFrameSize(previewSize_);

    cameraObject_->GetController()->ResetViewWithBoundingBox(viewedObject_, staticMeshComponent_->GetMeshInstance()->GetMesh()->GetAABB());
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
        MaterialInitializationData* initData = activeMaterial_->GetInitializationData();

        if (initData)
        {
            CompileGraphToMaterial(initData);
            MaterialSerializer().Serialize("M_GeneratedMaterial.xml", activeMaterial_);
            activeMaterial_->Build(nullptr);
            activeMaterial_->PreInit();
            activeMaterial_->Init();
            activeMaterial_->PostInit();

            if (staticMeshComponent_->GetMeshInstance())
            {
                staticMeshComponent_->GetMeshInstance()->SetMaterial(MaterialInstance::Create(activeMaterial_));
            }
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

        float colWidth = ImGui::GetContentRegionAvail().x;
        float previewHeight = colWidth + ImGui::GetTextLineHeight() + (ImGui::GetStyle().ItemSpacing.y * 4.0f) + 4.0f;

        float propsHeight = ImGui::GetContentRegionAvail().y - previewHeight;
        if (propsHeight < 10.0f) propsHeight = 10.0f;

        ImGui::BeginChild("PropertiesSidebarChild", ImVec2(0, propsHeight));
        DrawPropertiesSidebar();
        ImGui::EndChild();

        DrawPreview();

        ImGui::EndTable();
    }
    ImGui::End();
}

void ShaderEditorPanel::DrawPreview()
{
    ImVec2 previewAreaSize = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x);

    if (previewAreaSize.x <= 0.0f || previewAreaSize.y <= 0.0f)
    {
        return;
    }

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Material Preview");
    ImGui::Separator();

    if (previewSize_.x != previewAreaSize.x || previewSize_.y != previewAreaSize.y)
    {
        previewSize_ = EditorUtils::ToVector2(previewAreaSize);
        renderTarget_->SetFrameSize({ previewAreaSize.x, previewAreaSize.y });
    }

    Texture* renderTargetTexture = renderTarget_->GetTexture();
    ImGui::Image(
        (ImTextureID)(intptr_t)renderTargetTexture->GetRendererTextureId(),
        ImVec2(previewSize_.x, previewSize_.y),
        ImVec2{ 0.f, 1.f },
        ImVec2{ 1.f, 0.f }
    );

    bool isHovered = ImGui::IsItemHovered();
    cameraObject_->GetController()->SetIsActive(isHovered);

    EditorUtils::DrawWorldAxis(cameraObject_->GetCameraComponent()->GetCamera());

    ImGui::Separator();
}

void ShaderEditorPanel::DrawNodeCanvas()
{
    ImGui::BeginChild("shader_scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    ImVec2 offset = ImVec2(canvas_p0.x + scrolling_.x, canvas_p0.y + scrolling_.y);

    // --- Canvas Panning & Zooming ---
    if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f)
    {
        float mouseWheel = ImGui::GetIO().MouseWheel;
        float oldScale = scale_;
        scale_ = GoknarMath::Clamp(scale_ + mouseWheel * 0.05f, 0.2f, 3.0f);

        ImVec2 mousePosInCanvas = ImVec2(ImGui::GetMousePos().x - canvas_p0.x, ImGui::GetMousePos().y - canvas_p0.y);
        ImVec2 mousePosInWorld = ImVec2((mousePosInCanvas.x - scrolling_.x) / oldScale, (mousePosInCanvas.y - scrolling_.y) / oldScale);
        scrolling_.x = mousePosInCanvas.x - (mousePosInWorld.x * scale_);
        scrolling_.y = mousePosInCanvas.y - (mousePosInWorld.y * scale_);
    }

    float GRID_SZ = 64.0f * scale_;
    ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
    for (float x = fmodf(scrolling_.x, GRID_SZ); x < canvas_sz.x; x += GRID_SZ) draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p0.y + canvas_sz.y), GRID_COLOR);
    for (float y = fmodf(scrolling_.y, GRID_SZ); y < canvas_sz.y; y += GRID_SZ) draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + y), GRID_COLOR);

    draw_list->ChannelsSplit(2);

    // --- Draw Links ---
    draw_list->ChannelsSetCurrent(0);
    for (auto& link : links_)
    {
        ImVec2 startLocal = GetPinPosition(link.startPinId);
        ImVec2 endLocal = GetPinPosition(link.endPinId);

        ImVec2 startPos = ImVec2(offset.x + startLocal.x * scale_, offset.y + startLocal.y * scale_);
        ImVec2 endPos = ImVec2(offset.x + endLocal.x * scale_, offset.y + endLocal.y * scale_);

        ImVec2 cp1 = ImVec2(startPos.x + 50.0f * scale_, startPos.y);
        ImVec2 cp2 = ImVec2(endPos.x - 50.0f * scale_, endPos.y);

        ImU32 color = (selectedLinkId_ == link.id) ? IM_COL32(255, 200, 0, 255) : IM_COL32(200, 200, 200, 255);
        draw_list->AddBezierCubic(startPos, cp1, cp2, endPos, color, 3.0f * scale_);
    }

    // --- Draw Nodes ---
    draw_list->ChannelsSetCurrent(1);
    for (auto& node : nodes_)
    {
        ImGui::PushID(node.id);
        ImVec2 rect_min = ImVec2(offset.x + node.pos.x * scale_, offset.y + node.pos.y * scale_);
        ImVec2 scaled_size = ImVec2(node.size.x * scale_, node.size.y * scale_);
        ImVec2 rect_max = ImVec2(rect_min.x + scaled_size.x, rect_min.y + scaled_size.y);

        bool isSelected = selectedNodeIds_.find(node.id) != selectedNodeIds_.end();
        ImU32 bg_col = isSelected ? IM_COL32(75, 75, 100, 255) : IM_COL32(40, 40, 40, 255);
        if (node.typeCategory == "Master") bg_col = isSelected ? IM_COL32(60, 100, 60, 255) : IM_COL32(40, 80, 40, 255);

        draw_list->AddRectFilled(rect_min, rect_max, bg_col, 4.0f * scale_);
        draw_list->AddRectFilled(rect_min, ImVec2(rect_max.x, rect_min.y + 25.0f * scale_), IM_COL32(20, 20, 20, 255), 4.0f * scale_);
        draw_list->AddRect(rect_min, rect_max, IM_COL32(100, 100, 100, 255), 4.0f * scale_);

        ImGui::SetCursorScreenPos(ImVec2(rect_min.x + 8.0f * scale_, rect_min.y + 6.0f * scale_));
        ImGui::SetWindowFontScale(scale_);
        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "%s", node.name.c_str());
        ImGui::SetWindowFontScale(1.0f);

        // --- FULL NODE HITBOX ---
        // By spanning the entire node size, the user can click anywhere on the node body to select/drag
        ImGui::SetCursorScreenPos(rect_min);
        
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton((std::string("node_body_") + std::to_string(node.id)).c_str(), scaled_size);

        if (ImGui::IsItemClicked(0))

        if (ImGui::IsItemClicked(0))
        {
            if (ImGui::GetIO().KeyCtrl)
            {
                if (isSelected) selectedNodeIds_.erase(node.id);
                else selectedNodeIds_.insert(node.id);
            }
            else
            {
                // If we click a node that isn't selected, clear and select just this one.
                // If we click a node that IS selected, we do nothing to preserve the multi-selection for dragging.
                if (!isSelected) {
                    selectedNodeIds_.clear();
                    selectedNodeIds_.insert(node.id);
                }
            }
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
        {
            float dx = ImGui::GetIO().MouseDelta.x / scale_;
            float dy = ImGui::GetIO().MouseDelta.y / scale_;

            if (isSelected)
            {
                for (int selId : selectedNodeIds_)
                {
                    ShaderNode* selNode = FindNode(selId);
                    if (selNode) {
                        selNode->pos.x += dx;
                        selNode->pos.y += dy;
                    }
                }
            }
            else
            {
                node.pos.x += dx;
                node.pos.y += dy;
            }
        }

        // Inline Float Constant Editor
        if (node.name == "Float Constant" && !node.outputs.empty())
        {
            ImGui::SetCursorScreenPos(ImVec2(rect_min.x + 15.0f * scale_, rect_min.y + 35.0f * scale_ - 10.0f * scale_));
            ImGui::PushItemWidth(60.0f * scale_);

            float val = 0.0f;
            if (std::holds_alternative<float>(node.outputs[0].defaultValue)) val = std::get<float>(node.outputs[0].defaultValue);

            if (ImGui::DragFloat((std::string("##out_val_") + std::to_string(node.id)).c_str(), &val, 0.01f))
            {
                node.outputs[0].defaultValue = val;
            }
            ImGui::PopItemWidth();
        }

        // Input Pins
        float pinY = 35.0f * scale_;
        for (auto& input : node.inputs)
        {
            ImVec2 pinPos = ImVec2(rect_min.x, rect_min.y + pinY);
            ImU32 pinCol = (input.type == ShaderPinType::Any) ? IM_COL32(200, 200, 200, 255) : IM_COL32(150, 150, 250, 255);
            draw_list->AddCircleFilled(pinPos, 6.0f * scale_, pinCol);

            ImGui::SetCursorScreenPos(ImVec2(rect_min.x + 15.0f * scale_, rect_min.y + pinY - 8.0f * scale_));
            ImGui::SetWindowFontScale(scale_);
            ImGui::Text("%s", input.name.c_str());

            bool isConnected = false;
            for (const auto& link : links_) {
                if (link.endPinId == input.id) { isConnected = true; break; }
            }

            if (!isConnected && (input.type == ShaderPinType::Float || input.type == ShaderPinType::Any))
            {
                ImGui::SetCursorScreenPos(ImVec2(rect_min.x + 60.0f * scale_, rect_min.y + pinY - 10.0f * scale_));
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

            ImGui::SetCursorScreenPos(ImVec2(pinPos.x - 10.0f * scale_, pinPos.y - 10.0f * scale_));
            ImGui::InvisibleButton((std::string("in_") + std::to_string(input.id)).c_str(), ImVec2(20.0f * scale_, 20.0f * scale_));

            if (isDraggingLink_ && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseReleased(0))
            {
                links_.erase(std::remove_if(links_.begin(), links_.end(),
                    [&input](const ShaderLink& link) { return link.endPinId == input.id; }), links_.end());

                links_.push_back({ nextId_++, draggingStartPinId_, input.id });
                isDraggingLink_ = false;
            }
            pinY += 25.0f * scale_;
        }

        // Output Pins
        pinY = 35.0f * scale_;
        for (auto& output : node.outputs)
        {
            ImVec2 pinPos = ImVec2(rect_max.x, rect_min.y + pinY);
            ImU32 pinCol = (output.type == ShaderPinType::Any) ? IM_COL32(200, 200, 200, 255) : IM_COL32(250, 150, 150, 255);
            draw_list->AddCircleFilled(pinPos, 6.0f * scale_, pinCol);

            float textSize = ImGui::CalcTextSize(output.name.c_str()).x * scale_;
            ImGui::SetCursorScreenPos(ImVec2(rect_max.x - textSize - 15.0f * scale_, rect_min.y + pinY - 8.0f * scale_));
            ImGui::SetWindowFontScale(scale_);
            ImGui::Text("%s", output.name.c_str());
            ImGui::SetWindowFontScale(1.0f);

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

    // --- Canvas Click & Marquee Initiation ---
    // Evaluated at the END so we know for sure the mouse isn't over any nodes/pins.
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
    {
        if (!ImGui::GetIO().KeyCtrl)
        {
            selectedNodeIds_.clear();
        }
        selectedLinkId_ = -1;

        isDraggingSelection_ = true;
        selectionStart_ = ImVec2((ImGui::GetMousePos().x - offset.x) / scale_,
            (ImGui::GetMousePos().y - offset.y) / scale_);
        preDragSelection_ = selectedNodeIds_;
    }

    // --- Marquee Selection Drawing & Logic ---
    if (isDraggingSelection_)
    {
        selectedNodeIds_ = preDragSelection_;

        ImVec2 currentPos = ImVec2((ImGui::GetMousePos().x - offset.x) / scale_,
            (ImGui::GetMousePos().y - offset.y) / scale_);

        ImVec2 min_pos = ImVec2(std::min(selectionStart_.x, currentPos.x), std::min(selectionStart_.y, currentPos.y));
        ImVec2 max_pos = ImVec2(std::max(selectionStart_.x, currentPos.x), std::max(selectionStart_.y, currentPos.y));

        ImVec2 screen_min = ImVec2(offset.x + min_pos.x * scale_, offset.y + min_pos.y * scale_);
        ImVec2 screen_max = ImVec2(offset.x + max_pos.x * scale_, offset.y + max_pos.y * scale_);

        draw_list->AddRectFilled(screen_min, screen_max, IM_COL32(130, 170, 255, 40));
        draw_list->AddRect(screen_min, screen_max, IM_COL32(130, 170, 255, 200));

        for (auto& node : nodes_)
        {
            ImVec2 node_min = node.pos;
            ImVec2 node_max = ImVec2(node.pos.x + node.size.x, node.pos.y + node.size.y);

            bool overlap = (min_pos.x < node_max.x && max_pos.x > node_min.x &&
                min_pos.y < node_max.y && max_pos.y > node_min.y);

            if (overlap)
            {
                selectedNodeIds_.insert(node.id);
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            isDraggingSelection_ = false;
        }
    }

    // --- Dragging Wire ---
    if (isDraggingLink_)
    {
        ImVec2 startLocal = GetPinPosition(draggingStartPinId_);
        ImVec2 startPos = ImVec2(offset.x + startLocal.x * scale_, offset.y + startLocal.y * scale_);
        ImVec2 endPos = ImGui::GetMousePos();
        draw_list->AddBezierCubic(startPos, ImVec2(startPos.x + 50.0f * scale_, startPos.y), ImVec2(endPos.x - 50.0f * scale_, endPos.y), endPos, IM_COL32(255, 255, 255, 200), 3.0f * scale_);

        if (ImGui::IsMouseReleased(0)) isDraggingLink_ = false;
    }

    // --- Middle Click Pan ---
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
    {
        scrolling_.x += ImGui::GetIO().MouseDelta.x;
        scrolling_.y += ImGui::GetIO().MouseDelta.y;
    }

    // --- Context Menu ---
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered())
    {
        ImGui::OpenPopup("CanvasMenu");
    }

    if (ImGui::BeginPopup("CanvasMenu"))
    {
        ImVec2 openPos = ImGui::GetIO().MouseClickedPos[1];
        ImVec2 spawnPos = ImVec2((openPos.x - offset.x) / scale_, (openPos.y - offset.y) / scale_);

        if (ImGui::BeginMenu("Engine Variables"))
        {
            if (ImGui::MenuItem("elapsedTime")) nodes_.push_back(SpawnNode("Variables", "elapsedTime", spawnPos));
            if (ImGui::MenuItem("textureUV")) nodes_.push_back(SpawnNode("Variables", "textureUV", spawnPos));
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Math"))
        {
            if (ImGui::MenuItem("Add")) nodes_.push_back(SpawnNode("Math", "Add", spawnPos));
            if (ImGui::MenuItem("Subtract")) nodes_.push_back(SpawnNode("Math", "Subtract", spawnPos));
            if (ImGui::MenuItem("Multiply")) nodes_.push_back(SpawnNode("Math", "Multiply", spawnPos));
            if (ImGui::MenuItem("Divide")) nodes_.push_back(SpawnNode("Math", "Divide", spawnPos));
            if (ImGui::MenuItem("Modulo")) nodes_.push_back(SpawnNode("Math", "Modulo", spawnPos));
            if (ImGui::MenuItem("Mix")) nodes_.push_back(SpawnNode("Math", "Mix", spawnPos));
            ImGui::Separator();
            if (ImGui::MenuItem("Sine")) nodes_.push_back(SpawnNode("Math", "Sine", spawnPos));
            if (ImGui::MenuItem("Cosine")) nodes_.push_back(SpawnNode("Math", "Cosine", spawnPos));
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Texture"))
        {
            if (ImGui::MenuItem("Texture Sample")) nodes_.push_back(SpawnNode("Texture", "Texture Sample", spawnPos));
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

    // --- Keyboard Shortcuts (Copy, Paste, Delete) ---
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
    {
        bool ctrlPressed = ImGui::GetIO().KeyCtrl;

        // COPY (Ctrl + C)
        if (ctrlPressed && ImGui::IsKeyPressed(ImGuiKey_C))
        {
            clipboardNodes_.clear();
            clipboardLinks_.clear();

            std::unordered_set<int> copiedPinIds;

            for (int nodeId : selectedNodeIds_)
            {
                if (nodeId == masterNodeId_) continue;

                ShaderNode* node = FindNode(nodeId);
                if (node)
                {
                    clipboardNodes_.push_back(*node);
                    for (const auto& pin : node->inputs) copiedPinIds.insert(pin.id);
                    for (const auto& pin : node->outputs) copiedPinIds.insert(pin.id);
                }
            }

            for (const auto& link : links_)
            {
                if (copiedPinIds.find(link.startPinId) != copiedPinIds.end() &&
                    copiedPinIds.find(link.endPinId) != copiedPinIds.end())
                {
                    clipboardLinks_.push_back(link);
                }
            }
        }

        // PASTE (Ctrl + V)
        if (ctrlPressed && ImGui::IsKeyPressed(ImGuiKey_V))
        {
            if (!clipboardNodes_.empty())
            {
                selectedNodeIds_.clear();

                ImVec2 minPos(FLT_MAX, FLT_MAX);
                for (const auto& node : clipboardNodes_)
                {
                    minPos.x = std::min(minPos.x, node.pos.x);
                    minPos.y = std::min(minPos.y, node.pos.y);
                }

                ImVec2 mouseCanvasPos = ImVec2((ImGui::GetMousePos().x - offset.x) / scale_,
                    (ImGui::GetMousePos().y - offset.y) / scale_);
                ImVec2 posOffset(mouseCanvasPos.x - minPos.x, mouseCanvasPos.y - minPos.y);

                std::unordered_map<int, int> oldToNewPinIds;

                for (auto node : clipboardNodes_)
                {
                    node.id = nextId_++;
                    node.pos.x += posOffset.x;
                    node.pos.y += posOffset.y;

                    for (auto& pin : node.inputs)
                    {
                        int newPinId = nextId_++;
                        oldToNewPinIds[pin.id] = newPinId;
                        pin.id = newPinId;
                        pin.nodeId = node.id;
                    }
                    for (auto& pin : node.outputs)
                    {
                        int newPinId = nextId_++;
                        oldToNewPinIds[pin.id] = newPinId;
                        pin.id = newPinId;
                        pin.nodeId = node.id;
                    }

                    nodes_.push_back(node);
                    selectedNodeIds_.insert(node.id);
                }

                for (auto link : clipboardLinks_)
                {
                    link.id = nextId_++;
                    link.startPinId = oldToNewPinIds[link.startPinId];
                    link.endPinId = oldToNewPinIds[link.endPinId];
                    links_.push_back(link);
                }
            }
        }

        // DELETE (Del or Backspace)
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))
        {
            if (!selectedNodeIds_.empty())
            {
                selectedNodeIds_.erase(masterNodeId_);

                if (!selectedNodeIds_.empty())
                {
                    std::vector<int> nodePinIds;

                    for (int nodeId : selectedNodeIds_) {
                        ShaderNode* nodeToDelete = FindNode(nodeId);
                        if (nodeToDelete) {
                            for (const auto& pin : nodeToDelete->inputs) nodePinIds.push_back(pin.id);
                            for (const auto& pin : nodeToDelete->outputs) nodePinIds.push_back(pin.id);
                        }
                    }

                    links_.erase(std::remove_if(links_.begin(), links_.end(),
                        [&nodePinIds](const ShaderLink& link) {
                            return std::find(nodePinIds.begin(), nodePinIds.end(), link.startPinId) != nodePinIds.end() ||
                                std::find(nodePinIds.begin(), nodePinIds.end(), link.endPinId) != nodePinIds.end();
                        }), links_.end());

                    nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
                        [this](const ShaderNode& node) { return selectedNodeIds_.find(node.id) != selectedNodeIds_.end(); }), nodes_.end());

                    selectedNodeIds_.clear();
                }
            }
            else if (selectedLinkId_ != -1)
            {
                links_.erase(std::remove_if(links_.begin(), links_.end(),
                    [this](const ShaderLink& link) { return link.id == selectedLinkId_; }), links_.end());

                selectedLinkId_ = -1;
            }
        }
    }

    ImGui::EndChild();
}

void ShaderEditorPanel::DrawPropertiesSidebar()
{
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Node Properties");
    ImGui::Separator();

    if (selectedNodeIds_.size() == 1)
    {
        int singleSelectedId = *selectedNodeIds_.begin();
        ShaderNode* node = FindNode(singleSelectedId);
        if (node)
        {
            ImGui::Text("Name: %s", node->name.c_str());
            ImGui::Text("Type: %s", node->typeCategory.c_str());

            if (node->typeCategory == "Texture")
            {
                ImGui::Separator();
                ImGui::Text("Texture Settings");
                char buf[256];
                strncpy(buf, node->stringData.c_str(), sizeof(buf));
                buf[sizeof(buf) - 1] = 0;
                if (ImGui::InputText("Sampler Name", buf, sizeof(buf)))
                {
                    node->stringData = buf;
                }
            }

            if (node->name == "Float Constant" && !node->outputs.empty())
            {
                ImGui::Separator();
                ImGui::Text("Constant Value");
                float val = 0.0f;
                if (std::holds_alternative<float>(node->outputs[0].defaultValue))
                    val = std::get<float>(node->outputs[0].defaultValue);

                if (ImGui::DragFloat("Value", &val, 0.01f))
                    node->outputs[0].defaultValue = val;
            }

            ImGui::Separator();
            ImGui::Text("Inputs");

            for (auto& pin : node->inputs)
            {
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

                if (isConnected)
                {
                    ImGui::TextDisabled("%s: Linked to %s", pin.name.c_str(), connectedNodeName.c_str());
                }
                else
                {
                    if (pin.type == ShaderPinType::Float || pin.type == ShaderPinType::Any)
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
    else if (selectedNodeIds_.size() > 1)
    {
        ImGui::TextDisabled("%zu nodes selected", selectedNodeIds_.size());
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
    node.size = ImVec2(200, 80);

    if (category == "Variables")
    {
        ShaderPinType type = ShaderPinType::Matrix4x4;
        if (name == "elapsedTime") type = ShaderPinType::Float;
        else if (name == "textureUV") type = ShaderPinType::Vector2;

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
        if (name == "Add" || name == "Subtract" || name == "Multiply" || name == "Divide" || name == "Modulo") {
            node.inputs.push_back({ nextId_++, node.id, "A", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
            node.inputs.push_back({ nextId_++, node.id, "B", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
            node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Any, ShaderPinKind::Output });
            node.size = ImVec2(160, 95);
        }
        else if (name == "Mix") {
            node.inputs.push_back({ nextId_++, node.id, "A", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
            node.inputs.push_back({ nextId_++, node.id, "B", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
            node.inputs.push_back({ nextId_++, node.id, "Alpha", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
            node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Any, ShaderPinKind::Output });
            node.size = ImVec2(160, 120);
        }
        else if (name == "Sine" || name == "Cosine") {
            node.inputs.push_back({ nextId_++, node.id, "X", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
            node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Any, ShaderPinKind::Output });
            node.size = ImVec2(160, 65);
        }
    }
    else if (category == "Texture")
    {
        if (name == "Texture Sample") {
            node.stringData = "diffuseTexture";
            node.inputs.push_back({ nextId_++, node.id, "UV", ShaderPinType::Vector2, ShaderPinKind::Input, Vector2(0.f) });
            node.outputs.push_back({ nextId_++, node.id, "Color", ShaderPinType::Vector4, ShaderPinKind::Output });
            node.size = ImVec2(200, 80);
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

    node.outputs.push_back({ nextId_++, node.id, "Value", type, ShaderPinKind::Output });
    return node;
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
    if (!outMaterialData) return;

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

    std::unordered_map<int, ShaderPinType> resolvedPinTypes;

    auto GetGLSLTypeString = [](ShaderPinType type) -> std::string {
        switch (type) {
        case ShaderPinType::Float: return "float";
        case ShaderPinType::Vector2: return "vec2";
        case ShaderPinType::Vector3: return "vec3";
        case ShaderPinType::Vector4: return "vec4";
        case ShaderPinType::Matrix4x4: return "mat4";
        default: return "float";
        }
        };

    auto PromoteTypes = [](ShaderPinType a, ShaderPinType b) -> ShaderPinType {
        if (a == b) return a;
        if (a == ShaderPinType::Float) return b;
        if (b == ShaderPinType::Float) return a;
        return std::max(a, b);
        };

    auto GetPinValueAndType = [&](const ShaderPin& pin) -> std::pair<std::string, ShaderPinType>
        {
            for (const auto& link : links_)
            {
                if (link.endPinId == pin.id)
                {
                    ShaderPin* outputPin = FindPin(link.startPinId);
                    ShaderNode* sourceNode = FindNode(outputPin->nodeId);

                    if (sourceNode->typeCategory == "Variables")
                    {
                        if (sourceNode->name == "elapsedTime") return { "elapsedTime", ShaderPinType::Float };
                        if (sourceNode->name == "textureUV") return { "textureUV", ShaderPinType::Vector2 };
                    }

                    ShaderPinType actualType = resolvedPinTypes[outputPin->id];
                    std::string varName = "node_" + std::to_string(sourceNode->id) + "_out_" + std::to_string(outputPin->id);
                    return { varName, actualType };
                }
            }

            if (pin.type == ShaderPinType::Any || pin.type == ShaderPinType::Float) {
                float val = 0.0f;
                if (std::holds_alternative<float>(pin.defaultValue)) val = std::get<float>(pin.defaultValue);
                return { std::to_string(val) + "f", ShaderPinType::Float };
            }

            return { "0.0f", ShaderPinType::None };
        };

    auto CompileSubGraph = [&](const std::vector<std::string>& targetMasterPins, std::string& outCalculation)
        {
            std::vector<ShaderNode*> executionOrder;
            std::unordered_set<int> visitedNodes;

            std::function<void(int)> backtrace = [&](int nodeId)
                {
                    if (visitedNodes.find(nodeId) != visitedNodes.end()) return;
                    visitedNodes.insert(nodeId);

                    ShaderNode* node = FindNode(nodeId);
                    if (!node) return;

                    for (const auto& inputPin : node->inputs)
                    {
                        for (const auto& link : links_)
                        {
                            if (link.endPinId == inputPin.id)
                            {
                                ShaderPin* connectedOutput = FindPin(link.startPinId);
                                if (connectedOutput) backtrace(connectedOutput->nodeId);
                            }
                        }
                    }

                    if (nodeId != masterNodeId_) executionOrder.push_back(node);
                };

            for (const auto& masterInput : master->inputs)
            {
                if (std::find(targetMasterPins.begin(), targetMasterPins.end(), masterInput.name) != targetMasterPins.end())
                {
                    for (const auto& link : links_)
                    {
                        if (link.endPinId == masterInput.id)
                        {
                            ShaderPin* connectedOutput = FindPin(link.startPinId);
                            if (connectedOutput) backtrace(connectedOutput->nodeId);
                        }
                    }
                }
            }

            if (executionOrder.empty()) return;

            std::string glslBody = "\n\t// --- Generated Node Graph Calculations ---\n";

            for (ShaderNode* node : executionOrder)
            {
                if (node->typeCategory == "Variables") continue;

                glslBody += "\t// Node: " + node->name + "\n";
                std::string outVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[0].id);

                if (node->typeCategory == "Math")
                {
                    if (node->name == "Add" || node->name == "Subtract" || node->name == "Multiply" || node->name == "Divide" || node->name == "Modulo")
                    {
                        auto [valA, typeA] = GetPinValueAndType(node->inputs[0]);
                        auto [valB, typeB] = GetPinValueAndType(node->inputs[1]);

                        ShaderPinType outType = PromoteTypes(typeA, typeB);
                        resolvedPinTypes[node->outputs[0].id] = outType;

                        std::string op = (node->name == "Add") ? "+" : (node->name == "Subtract") ? "-" : (node->name == "Multiply") ? "*" : "/";

                        if (node->name == "Modulo") {
                            glslBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = mod(" + valA + ", " + valB + ");\n";
                        }
                        else {
                            glslBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = " + valA + " " + op + " " + valB + ";\n";
                        }
                    }
                    else if (node->name == "Mix")
                    {
                        auto [valA, typeA] = GetPinValueAndType(node->inputs[0]);
                        auto [valB, typeB] = GetPinValueAndType(node->inputs[1]);
                        auto [valAlpha, typeAlpha] = GetPinValueAndType(node->inputs[2]);

                        ShaderPinType outType = PromoteTypes(typeA, typeB);
                        resolvedPinTypes[node->outputs[0].id] = outType;

                        glslBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = mix(" + valA + ", " + valB + ", " + valAlpha + ");\n";
                    }
                    else if (node->name == "Sine" || node->name == "Cosine")
                    {
                        auto [val, type] = GetPinValueAndType(node->inputs[0]);
                        resolvedPinTypes[node->outputs[0].id] = type;

                        std::string func = (node->name == "Sine") ? "sin" : "cos";
                        glslBody += "\t" + GetGLSLTypeString(type) + " " + outVar + " = " + func + "(" + val + ");\n";
                    }
                }
                else if (node->typeCategory == "Texture")
                {
                    if (node->name == "Texture Sample")
                    {
                        auto [uv, uvType] = GetPinValueAndType(node->inputs[0]);
                        resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Vector4;

                        std::string samplerName = node->stringData.empty() ? "diffuseTexture" : node->stringData;
                        glslBody += "\tvec4 " + outVar + " = texture(" + samplerName + ", " + uv + ");\n";
                    }
                }
                else if (node->typeCategory == "Constants")
                {
                    if (node->name == "Float Constant") {
                        resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Float;
                        auto [val, type] = GetPinValueAndType(node->outputs[0]);
                        glslBody += "\tfloat " + outVar + " = " + val + ";\n";
                    }
                    else if (node->name == "Vector2 Constant") {
                        resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Vector2;
                        auto [x, tX] = GetPinValueAndType(node->inputs[0]);
                        auto [y, tY] = GetPinValueAndType(node->inputs[1]);
                        glslBody += "\tvec2 " + outVar + " = vec2(" + x + ", " + y + ");\n";
                    }
                    else if (node->name == "Vector3 Constant") {
                        resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Vector3;
                        auto [x, tX] = GetPinValueAndType(node->inputs[0]);
                        auto [y, tY] = GetPinValueAndType(node->inputs[1]);
                        auto [z, tZ] = GetPinValueAndType(node->inputs[2]);
                        glslBody += "\tvec3 " + outVar + " = vec3(" + x + ", " + y + ", " + z + ");\n";
                    }
                    else if (node->name == "Vector4 Constant") {
                        resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Vector4;
                        auto [x, tX] = GetPinValueAndType(node->inputs[0]);
                        auto [y, tY] = GetPinValueAndType(node->inputs[1]);
                        auto [z, tZ] = GetPinValueAndType(node->inputs[2]);
                        auto [w, tW] = GetPinValueAndType(node->inputs[3]);
                        glslBody += "\tvec4 " + outVar + " = vec4(" + x + ", " + y + ", " + z + ", " + w + ");\n";
                    }
                }
            }

            outCalculation = glslBody;
        };

    std::string vertexCalc;
    CompileSubGraph({ "World Position Offset" }, vertexCalc);
    outMaterialData->vertexPositionOffset.calculation = vertexCalc;

    std::string fragmentCalc;
    CompileSubGraph({ "Base Color", "Emissive", "Normal" }, fragmentCalc);
    outMaterialData->baseColor.calculation = fragmentCalc;

    auto FormatMasterOutput = [](const std::string& val, ShaderPinType actualType, ShaderPinType expectedType) -> std::string
        {
            if (actualType == expectedType) return val;

            if (expectedType == ShaderPinType::Vector4 && actualType == ShaderPinType::Vector3) return "vec4(" + val + ", 1.0f)";
            if (expectedType == ShaderPinType::Vector4 && actualType == ShaderPinType::Float) return "vec4(" + val + ")";
            if (expectedType == ShaderPinType::Vector3 && actualType == ShaderPinType::Vector4) return val + ".xyz";
            if (expectedType == ShaderPinType::Vector3 && actualType == ShaderPinType::Float) return "vec3(" + val + ")";

            return val;
        };

    for (const auto& masterInput : master->inputs)
    {
        auto [finalValue, finalType] = GetPinValueAndType(masterInput);

        if (finalType == ShaderPinType::None)
        {
            continue;
        }

        std::string formattedResult = FormatMasterOutput(finalValue, finalType, masterInput.type);

        if (masterInput.name == "Base Color") outMaterialData->baseColor.result = formattedResult + ";";
        else if (masterInput.name == "Emissive") outMaterialData->emmisiveColor.result = formattedResult + ";";
        else if (masterInput.name == "Normal") outMaterialData->fragmentNormal.result = formattedResult + ";";
        else if (masterInput.name == "World Position Offset") outMaterialData->vertexPositionOffset.result = formattedResult + ";";
    }
}