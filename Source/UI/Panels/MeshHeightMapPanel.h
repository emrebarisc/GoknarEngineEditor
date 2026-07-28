#pragma once

#include "EditorPanel.h"

#include <array>
#include <string>
#include <vector>

class Image;
class StaticMesh;

class MeshHeightMapPanel : public IEditorPanel
{
public:
	explicit MeshHeightMapPanel(EditorHUD* hud);
	void Draw() override;

private:
	struct ConversionResult
	{
		std::vector<unsigned char> collisionHeightData;
		std::vector<unsigned char> grayscaleHeightData;
		int heightStickWidth{ 0 };
		int heightStickLength{ 0 };
		float minSourceHeight{ 0.0f };
		float maxSourceHeight{ 0.0f };
		float minHeight{ 0.0f };
		float maxHeight{ 0.0f };
		float width{ 0.0f };
		float length{ 0.0f };
		float heightScale{ 1.0f };
		int uncoveredSampleCount{ 0 };
	};

	void OnSourceMeshSelected(const std::string& path);
	void SetDefaultOutputPathForMesh(const std::string& meshPath);
	void ConvertSelectedMesh();
	bool BuildHeightMap(StaticMesh* mesh, ConversionResult& outResult, std::string& outError) const;
	bool WriteHeightMapPng(const ConversionResult& result, const std::string& outputRelativePath, std::string& outAbsolutePath, std::string& outError) const;
	Image* CreateGeneratedHeightMapImage(const std::string& outputRelativePath, const ConversionResult& result) const;
	void ApplyResultToSelectedHeightMapComponent(const ConversionResult& result, Image* image);
	void UpsertHeightMapAssetMetadata(const std::string& outputRelativePath) const;
	std::string GetNormalizedOutputRelativePath() const;

	StaticMesh* sourceMesh_{ nullptr };
	std::string sourceMeshPath_;
	std::array<char, 260> outputPathBuffer_{};
	int heightStickWidth_{ 129 };
	int heightStickLength_{ 129 };
	bool applyToSelectedHeightMapComponent_{ true };
	std::string statusMessage_;
	ConversionResult lastConversionResult_{};
};
