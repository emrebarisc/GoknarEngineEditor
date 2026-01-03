#include "GeometryBuffersPanel.h"

#include "imgui.h"

#include "Goknar/Renderer/Renderer.h"
#include "Goknar/Renderer/RenderTarget.h"
#include "Goknar/Renderer/Texture.h"

#include "UI/EditorContext.h"

void GeometryBuffersPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImVec2 windowSize = ImGui::GetWindowSize();
	ImVec2 imageSize = { (float)windowSize.x * 0.3334f, (float)windowSize.y * 0.5f };

	ImVec2 imagePosition = { 0.f, 0.f };

	ImGui::SetCursorPos(imagePosition);
	Texture* diffuseTexture = EditorContext::Get()->viewportRenderTarget->GetDeferredRenderingData()->geometryBufferData->diffuseTexture;
	Texture* emmisiveColorTexture = EditorContext::Get()->viewportRenderTarget->GetDeferredRenderingData()->geometryBufferData->emmisiveColorTexture;
	Texture* specularTexture = EditorContext::Get()->viewportRenderTarget->GetDeferredRenderingData()->geometryBufferData->specularTexture;
	Texture* worldPositionTexture = EditorContext::Get()->viewportRenderTarget->GetDeferredRenderingData()->geometryBufferData->worldPositionTexture;
	Texture* worldNormalTexture = EditorContext::Get()->viewportRenderTarget->GetDeferredRenderingData()->geometryBufferData->worldNormalTexture;

	ImGui::Image(
		(ImTextureID)(intptr_t)diffuseTexture->GetRendererTextureId(),
		imageSize,
		ImVec2{ 0.f, 1.f },
		ImVec2{ 1.f, 0.f }
	);

	imagePosition = { imageSize.x, 0.f };
	ImGui::SetCursorPos(imagePosition);
	ImGui::Image(
		(ImTextureID)(intptr_t)emmisiveColorTexture->GetRendererTextureId(),
		imageSize,
		ImVec2{ 0.f, 1.f },
		ImVec2{ 1.f, 0.f }
	);

	imagePosition = { 2.f * imageSize.x, 0.f };
	ImGui::SetCursorPos(imagePosition);
	ImGui::Image(
		(ImTextureID)(intptr_t)specularTexture->GetRendererTextureId(),
		imageSize,
		ImVec2{ 0.f, 1.f },
		ImVec2{ 1.f, 0.f }
	);

	imagePosition = { 0.f, imageSize.y };
	ImGui::SetCursorPos(imagePosition);
	ImGui::Image(
		(ImTextureID)(intptr_t)worldPositionTexture->GetRendererTextureId(),
		imageSize,
		ImVec2{ 0.f, 1.f },
		ImVec2{ 1.f, 0.f }
	);

	imagePosition = { imageSize.x, imageSize.y };
	ImGui::SetCursorPos(imagePosition);
	ImGui::Image(
		(ImTextureID)(intptr_t)worldNormalTexture->GetRendererTextureId(),
		imageSize,
		ImVec2{ 0.f, 1.f },
		ImVec2{ 1.f, 0.f }
	);

	ImGui::End();
}