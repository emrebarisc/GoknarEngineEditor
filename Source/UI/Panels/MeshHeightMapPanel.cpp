#include "MeshHeightMapPanel.h"

#include "imgui.h"
#include "tinyxml2.h"

#include "Goknar/Core.h"
#include "Goknar/Engine.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Contents/Image.h"
#include "Goknar/Geometry/Box.h"
#include "Goknar/IO/IOManager.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Model/MeshUnit.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Physics/Components/HeightMapCollisionComponent.h"

#include "UI/EditorAssetPathUtils.h"
#include "UI/EditorContext.h"
#include "UI/EditorHUD.h"
#include "UI/EditorWidgets.h"
#include "UI/Panels/AssetSelectorPanel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <sstream>

namespace
{
	constexpr int MinHeightMapResolution = 2;
	constexpr int MaxHeightMapResolution = 4096;
	constexpr float TerrainPlaneEpsilon = 0.00001f;
	constexpr float MinimumEncodedMaxHeight = 0.001f;

	void CopyToBuffer(const std::string& value, std::array<char, 260>& buffer)
	{
		buffer.fill('\0');
		const size_t copyLength = (std::min)(value.size(), buffer.size() - 1);
		std::memcpy(buffer.data(), value.c_str(), copyLength);
	}

	bool StartsWith(const std::string& value, const std::string& prefix)
	{
		return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
	}

	std::string NormalizeGenericPath(const std::filesystem::path& path)
	{
		return path.lexically_normal().generic_string();
	}

	std::string EnsurePngExtension(std::string path)
	{
		std::filesystem::path outputPath(path);
		if (outputPath.extension().generic_string() != ".png")
		{
			outputPath.replace_extension(".png");
		}

		return NormalizeGenericPath(outputPath);
	}

	bool IsPointInsideProjectedTriangle(
		float x,
		float y,
		const Vector3& vertex0,
		const Vector3& vertex1,
		const Vector3& vertex2,
		float inverseDenominator,
		float& outHeight)
	{
		const float weight0 =
			((vertex1.y - vertex2.y) * (x - vertex2.x) + (vertex2.x - vertex1.x) * (y - vertex2.y)) * inverseDenominator;
		const float weight1 =
			((vertex2.y - vertex0.y) * (x - vertex2.x) + (vertex0.x - vertex2.x) * (y - vertex2.y)) * inverseDenominator;
		const float weight2 = 1.0f - weight0 - weight1;

		if (weight0 < -TerrainPlaneEpsilon || weight1 < -TerrainPlaneEpsilon || weight2 < -TerrainPlaneEpsilon)
		{
			return false;
		}

		outHeight = weight0 * vertex0.z + weight1 * vertex1.z + weight2 * vertex2.z;
		return true;
	}

	int ClampResolution(int value)
	{
		return std::clamp(value, MinHeightMapResolution, MaxHeightMapResolution);
	}

	int ClampGridIndex(float value, int maxIndex)
	{
		return std::clamp(static_cast<int>(value), 0, maxIndex);
	}

	void SetElementText(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* parent, const char* elementName, const char* text)
	{
		tinyxml2::XMLElement* element = parent->FirstChildElement(elementName);
		if (!element)
		{
			element = document.NewElement(elementName);
			parent->InsertEndChild(element);
		}

		element->SetText(text);
	}

	std::string GetElementText(const tinyxml2::XMLElement* element)
	{
		const char* text = element ? element->GetText() : nullptr;
		return text ? text : "";
	}
}

MeshHeightMapPanel::MeshHeightMapPanel(EditorHUD* hud) :
	IEditorPanel("Mesh Height Map", hud)
{
	isOpen_ = false;
	CopyToBuffer("Textures/HeightMaps/HM_NewHeightMap.png", outputPathBuffer_);
}

void MeshHeightMapPanel::Draw()
{
	ImGui::Begin(title_.c_str(), &isOpen_);

	ImGui::Text("Source Mesh:");
	ImGui::SameLine();
	ImGui::TextWrapped("%s", sourceMeshPath_.empty() ? "" : sourceMeshPath_.c_str());
	ImGui::SameLine();
	if (ImGui::Button("Select Mesh"))
	{
		EditorContext::Get()->assetSelectorFilter = EditorAssetType::StaticMesh;
		AssetSelectorPanel::OnAssetSelected =
			Delegate<void(const std::string&)>::Create<MeshHeightMapPanel, &MeshHeightMapPanel::OnSourceMeshSelected>(this);
		hud_->ShowPanel<AssetSelectorPanel>();
	}

	heightStickWidth_ = ClampResolution(heightStickWidth_);
	heightStickLength_ = ClampResolution(heightStickLength_);

	ImGui::Text("HeightStickWidth:");
	ImGui::SameLine();
	if (EditorWidgets::DrawInputInt("##HeightMapConverterWidth", heightStickWidth_))
	{
		heightStickWidth_ = ClampResolution(heightStickWidth_);
	}

	ImGui::Text("HeightStickLength:");
	ImGui::SameLine();
	if (EditorWidgets::DrawInputInt("##HeightMapConverterLength", heightStickLength_))
	{
		heightStickLength_ = ClampResolution(heightStickLength_);
	}

	ImGui::Text("Output:");
	ImGui::SameLine();
	ImGui::InputText("##HeightMapConverterOutput", outputPathBuffer_.data(), outputPathBuffer_.size());

	ImGui::Checkbox("Apply to selected HeightMapCollisionComponent", &applyToSelectedHeightMapComponent_);

	if (ImGui::Button("Convert"))
	{
		ConvertSelectedMesh();
	}

	if (!statusMessage_.empty())
	{
		ImGui::Separator();
		ImGui::TextWrapped("%s", statusMessage_.c_str());
	}

	if (!lastConversionResult_.grayscaleHeightData.empty())
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Conversion Result:");
		ImGui::Text("HeightStickWidth: %d", lastConversionResult_.heightStickWidth);
		ImGui::Text("HeightStickLength: %d", lastConversionResult_.heightStickLength);
		ImGui::Text("MinHeight: %.6f", lastConversionResult_.minHeight);
		ImGui::Text("MaxHeight: %.6f", lastConversionResult_.maxHeight);
		ImGui::Text("Width: %.6f", lastConversionResult_.width);
		ImGui::Text("Length: %.6f", lastConversionResult_.length);
		ImGui::Text("HeightScale: %.6f", lastConversionResult_.heightScale);

		if (lastConversionResult_.uncoveredSampleCount > 0)
		{
			ImGui::Text("Uncovered Samples: %d", lastConversionResult_.uncoveredSampleCount);
		}
	}

	ImGui::End();
}

void MeshHeightMapPanel::OnSourceMeshSelected(const std::string& path)
{
	sourceMeshPath_ = EditorAssetPathUtils::ToContentRelativePath(path);
	sourceMesh_ = engine->GetResourceManager()->GetContent<StaticMesh>(sourceMeshPath_);
	SetDefaultOutputPathForMesh(sourceMeshPath_);

	statusMessage_ = sourceMesh_ ? "Mesh selected." : "Failed to load selected static mesh.";
	EditorContext::Get()->assetSelectorFilter = EditorAssetType::None;
}

void MeshHeightMapPanel::SetDefaultOutputPathForMesh(const std::string& meshPath)
{
	if (meshPath.empty())
	{
		return;
	}

	std::string stem = std::filesystem::path(meshPath).stem().generic_string();
	if (StartsWith(stem, "SM_"))
	{
		stem = stem.substr(3);
	}

	CopyToBuffer("Textures/HeightMaps/HM_" + stem + ".png", outputPathBuffer_);
}

void MeshHeightMapPanel::ConvertSelectedMesh()
{
	if (!sourceMesh_ && !sourceMeshPath_.empty())
	{
		sourceMesh_ = engine->GetResourceManager()->GetContent<StaticMesh>(sourceMeshPath_);
	}

	std::string error;
	ConversionResult result;
	if (!BuildHeightMap(sourceMesh_, result, error))
	{
		statusMessage_ = error;
		return;
	}

	const std::string outputRelativePath = GetNormalizedOutputRelativePath();
	std::string outputAbsolutePath;
	if (!WriteHeightMapPng(result, outputRelativePath, outputAbsolutePath, error))
	{
		statusMessage_ = error;
		return;
	}

	UpsertHeightMapAssetMetadata(outputRelativePath);
	EditorContext::Get()->BuildFileTree();

	Image* generatedImage = CreateGeneratedHeightMapImage(outputRelativePath, result);
	if (applyToSelectedHeightMapComponent_)
	{
		ApplyResultToSelectedHeightMapComponent(result, generatedImage);
	}

	lastConversionResult_ = result;

	std::ostringstream statusStream;
	statusStream << "Wrote " << outputRelativePath << ".";
	if (!generatedImage)
	{
		statusStream << " The image file was written, but it could not be loaded into the resource manager.";
	}
	statusMessage_ = statusStream.str();
}

bool MeshHeightMapPanel::BuildHeightMap(StaticMesh* mesh, ConversionResult& outResult, std::string& outError) const
{
	if (!mesh)
	{
		outError = "Select a static mesh first.";
		return false;
	}

	const int heightStickWidth = ClampResolution(heightStickWidth_);
	const int heightStickLength = ClampResolution(heightStickLength_);
	const Box& bounds = mesh->GetAABB();
	const Vector3& boundsMin = bounds.GetMin();
	const Vector3& boundsMax = bounds.GetMax();
	const float meshWidth = boundsMax.x - boundsMin.x;
	const float meshLength = boundsMax.y - boundsMin.y;

	if (meshWidth <= TerrainPlaneEpsilon || meshLength <= TerrainPlaneEpsilon)
	{
		outError = "The selected mesh needs non-zero X/Y bounds.";
		return false;
	}

	const size_t sampleCount = static_cast<size_t>(heightStickWidth) * static_cast<size_t>(heightStickLength);
	std::vector<float> sampledHeights(sampleCount, -std::numeric_limits<float>::infinity());
	std::vector<unsigned char> coveredSamples(sampleCount, 0);

	const float gridScaleX = static_cast<float>(heightStickWidth - 1) / meshWidth;
	const float gridScaleY = static_cast<float>(heightStickLength - 1) / meshLength;

	for (const MeshUnit* subMesh : mesh->GetSubMeshes())
	{
		if (!subMesh || !subMesh->GetVerticesPointer() || !subMesh->GetFacesPointer())
		{
			continue;
		}

		const VertexArray& vertices = *subMesh->GetVerticesPointer();
		for (const Face& face : *subMesh->GetFacesPointer())
		{
			if (face.vertexIndices[0] >= vertices.size() ||
				face.vertexIndices[1] >= vertices.size() ||
				face.vertexIndices[2] >= vertices.size())
			{
				continue;
			}

			const Vector3& vertex0 = vertices[face.vertexIndices[0]].position;
			const Vector3& vertex1 = vertices[face.vertexIndices[1]].position;
			const Vector3& vertex2 = vertices[face.vertexIndices[2]].position;
			const float denominator =
				(vertex1.y - vertex2.y) * (vertex0.x - vertex2.x) +
				(vertex2.x - vertex1.x) * (vertex0.y - vertex2.y);

			if (std::abs(denominator) <= TerrainPlaneEpsilon)
			{
				continue;
			}

			const float triangleMinX = (std::min)({ vertex0.x, vertex1.x, vertex2.x });
			const float triangleMaxX = (std::max)({ vertex0.x, vertex1.x, vertex2.x });
			const float triangleMinY = (std::min)({ vertex0.y, vertex1.y, vertex2.y });
			const float triangleMaxY = (std::max)({ vertex0.y, vertex1.y, vertex2.y });

			const int startX = ClampGridIndex(std::floor((triangleMinX - boundsMin.x) * gridScaleX), heightStickWidth - 1);
			const int endX = ClampGridIndex(std::ceil((triangleMaxX - boundsMin.x) * gridScaleX), heightStickWidth - 1);
			const int startY = ClampGridIndex(std::floor((triangleMinY - boundsMin.y) * gridScaleY), heightStickLength - 1);
			const int endY = ClampGridIndex(std::ceil((triangleMaxY - boundsMin.y) * gridScaleY), heightStickLength - 1);
			const float inverseDenominator = 1.0f / denominator;

			for (int sampleY = startY; sampleY <= endY; ++sampleY)
			{
				const float yAlpha = static_cast<float>(sampleY) / static_cast<float>(heightStickLength - 1);
				const float sampleYPosition = boundsMin.y + yAlpha * meshLength;

				for (int sampleX = startX; sampleX <= endX; ++sampleX)
				{
					const float xAlpha = static_cast<float>(sampleX) / static_cast<float>(heightStickWidth - 1);
					const float sampleXPosition = boundsMin.x + xAlpha * meshWidth;

					float height = 0.0f;
					if (!IsPointInsideProjectedTriangle(sampleXPosition, sampleYPosition, vertex0, vertex1, vertex2, inverseDenominator, height))
					{
						continue;
					}

					const size_t sampleIndex = static_cast<size_t>(sampleY) * static_cast<size_t>(heightStickWidth) + static_cast<size_t>(sampleX);
					if (!coveredSamples[sampleIndex] || sampledHeights[sampleIndex] < height)
					{
						sampledHeights[sampleIndex] = height;
						coveredSamples[sampleIndex] = 1;
					}
				}
			}
		}
	}

	int coveredSampleCount = 0;
	float minSampleHeight = std::numeric_limits<float>::max();
	float maxSampleHeight = -std::numeric_limits<float>::max();
	for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
	{
		if (!coveredSamples[sampleIndex])
		{
			continue;
		}

		++coveredSampleCount;
		minSampleHeight = (std::min)(minSampleHeight, sampledHeights[sampleIndex]);
		maxSampleHeight = (std::max)(maxSampleHeight, sampledHeights[sampleIndex]);
	}

	if (coveredSampleCount == 0)
	{
		outError = "No height samples could be projected from this mesh.";
		return false;
	}

	const int uncoveredSampleCount = static_cast<int>(sampleCount) - coveredSampleCount;
	for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
	{
		if (!coveredSamples[sampleIndex])
		{
			sampledHeights[sampleIndex] = minSampleHeight;
		}
	}

	const float heightRange = (std::max)(maxSampleHeight - minSampleHeight, MinimumEncodedMaxHeight);
	std::vector<unsigned char> collisionHeightData(sampleCount * sizeof(float), 0);
	std::vector<unsigned char> grayscaleHeightData(sampleCount, 0);
	for (int sampleY = 0; sampleY < heightStickLength; ++sampleY)
	{
		for (int sampleX = 0; sampleX < heightStickWidth; ++sampleX)
		{
			const size_t sourceIndex = static_cast<size_t>(sampleY) * static_cast<size_t>(heightStickWidth) + static_cast<size_t>(sampleX);
			const size_t pngIndex = static_cast<size_t>(heightStickLength - 1 - sampleY) * static_cast<size_t>(heightStickWidth) + static_cast<size_t>(sampleX);
			const float sampledHeight = sampledHeights[sourceIndex];
			const float normalizedHeight = (sampledHeight - minSampleHeight) / heightRange;
			const int grayscaleValue = std::clamp(static_cast<int>(std::round(normalizedHeight * 255.0f)), 0, 255);

			// Bullet's PHY_FLOAT heightfield data should retain the mesh's actual Z values.
			std::memcpy(collisionHeightData.data() + sourceIndex * sizeof(float), &sampledHeight, sizeof(float));
			grayscaleHeightData[pngIndex] = static_cast<unsigned char>(grayscaleValue);
		}
	}

	outResult.collisionHeightData = std::move(collisionHeightData);
	outResult.grayscaleHeightData = std::move(grayscaleHeightData);
	outResult.heightStickWidth = heightStickWidth;
	outResult.heightStickLength = heightStickLength;
	outResult.minSourceHeight = minSampleHeight;
	outResult.maxSourceHeight = maxSampleHeight;

	// btHeightfieldTerrainShape's min/max values should describe the source
	// mesh's complete vertical bounds, not the normalized PNG range. Using
	// the AABB also retains extrema that may fall between grid sample points.
	outResult.minHeight = boundsMin.z;
	outResult.maxHeight = boundsMax.z;
	outResult.width = meshWidth;
	outResult.length = meshLength;
	outResult.heightScale = 1.0f;
	outResult.uncoveredSampleCount = uncoveredSampleCount;
	return true;
}

bool MeshHeightMapPanel::WriteHeightMapPng(
	const ConversionResult& result,
	const std::string& outputRelativePath,
	std::string& outAbsolutePath,
	std::string& outError) const
{
	if (outputRelativePath.empty())
	{
		outError = "Enter an output path under Content.";
		return false;
	}

	const std::filesystem::path contentRootPath(EditorAssetPathUtils::GetContentRootPath());
	const std::filesystem::path outputPath = (contentRootPath / outputRelativePath).lexically_normal();
	std::string normalizedContentRoot = NormalizeGenericPath(contentRootPath);
	if (!normalizedContentRoot.empty() && normalizedContentRoot.back() != '/')
	{
		normalizedContentRoot += "/";
	}
	const std::string normalizedOutputPath = NormalizeGenericPath(outputPath);
	if (!StartsWith(normalizedOutputPath, normalizedContentRoot))
	{
		outError = "Output path must stay under the project Content folder.";
		return false;
	}

	if (!EditorAssetPathUtils::EnsureDirectoryForFile(normalizedOutputPath))
	{
		outError = "Could not create the output directory.";
		return false;
	}

	if (!IOManager::WritePng(
		normalizedOutputPath.c_str(),
		result.heightStickWidth,
		result.heightStickLength,
		1,
		result.grayscaleHeightData.data()))
	{
		outError = "Failed to write the height map PNG.";
		return false;
	}

	outAbsolutePath = normalizedOutputPath;
	return true;
}

Image* MeshHeightMapPanel::CreateGeneratedHeightMapImage(const std::string& outputRelativePath, const ConversionResult& result) const
{
	const size_t bufferSize = result.collisionHeightData.size();
	if (bufferSize == 0)
	{
		return nullptr;
	}

	unsigned char* imageBuffer = new unsigned char[bufferSize];
	std::memcpy(imageBuffer, result.collisionHeightData.data(), bufferSize);

	Image* image = new Image(
		ContentDir + outputRelativePath,
		result.heightStickWidth,
		result.heightStickLength,
		static_cast<int>(sizeof(float)),
		imageBuffer);
	image->SetPath(ContentDir + outputRelativePath);
	image->SetName(std::filesystem::path(outputRelativePath).stem().generic_string());
	image->SetTextureUsage(TextureUsage::Height);
	image->SetCanUseTextureAtlas(false);
	image->SetUploadToGPU(false);
	engine->GetResourceManager()->GetResourceContainer()->AddImage(image);
	return image;
}

void MeshHeightMapPanel::ApplyResultToSelectedHeightMapComponent(const ConversionResult& result, Image* image)
{
	if (!image || EditorContext::Get()->selectedObjectType != EditorSelectionType::Object)
	{
		return;
	}

	ObjectBase* selectedObject = static_cast<ObjectBase*>(EditorContext::Get()->selectedObject);
	HeightMapCollisionComponent* component = selectedObject ? selectedObject->GetFirstComponentOfType<HeightMapCollisionComponent>() : nullptr;
	if (!component)
	{
		return;
	}

	component->SetHeightMapImage(image);
	component->SetHeightStickWidth(result.heightStickWidth);
	component->SetHeightStickLength(result.heightStickLength);
	component->SetMinHeight(result.minHeight);
	component->SetMaxHeight(result.maxHeight);
	component->SetWidth(result.width);
	component->SetLength(result.length);
	component->SetHeightScale(result.heightScale);
	EditorContext::Get()->MarkSceneDirty("Height map collision changed");
}

void MeshHeightMapPanel::UpsertHeightMapAssetMetadata(const std::string& outputRelativePath) const
{
	const std::string assetContainerPath = NormalizeGenericPath(std::filesystem::path(EditorAssetPathUtils::GetContentRootPath()) / "AssetContainer");

	tinyxml2::XMLDocument document;
	tinyxml2::XMLError loadResult = document.LoadFile(assetContainerPath.c_str());
	if (loadResult != tinyxml2::XML_SUCCESS)
	{
		document.Clear();
		tinyxml2::XMLElement* root = document.NewElement("AssetContainer");
		document.InsertEndChild(root);
		root->InsertEndChild(document.NewElement("Assets"));
	}

	tinyxml2::XMLElement* root = document.FirstChildElement("AssetContainer");
	if (!root)
	{
		document.Clear();
		root = document.NewElement("AssetContainer");
		document.InsertEndChild(root);
	}

	tinyxml2::XMLElement* assetsElement = root->FirstChildElement("Assets");
	if (!assetsElement)
	{
		assetsElement = document.NewElement("Assets");
		root->InsertEndChild(assetsElement);
	}

	tinyxml2::XMLElement* textureElement = nullptr;
	for (tinyxml2::XMLElement* candidate = assetsElement->FirstChildElement("Texture");
		candidate != nullptr;
		candidate = candidate->NextSiblingElement("Texture"))
	{
		if (EditorAssetPathUtils::ToContentRelativePath(GetElementText(candidate->FirstChildElement("Path"))) == outputRelativePath)
		{
			textureElement = candidate;
			break;
		}
	}

	if (!textureElement)
	{
		textureElement = document.NewElement("Texture");
		assetsElement->InsertEndChild(textureElement);
	}

	textureElement->SetAttribute("Name", std::filesystem::path(outputRelativePath).stem().generic_string().c_str());
	textureElement->SetAttribute("Usage", "Height");
	textureElement->SetAttribute("UseTextureAtlas", "false");
	textureElement->SetAttribute("UploadToGPU", "false");
	SetElementText(document, textureElement, "Path", outputRelativePath.c_str());
	document.SaveFile(assetContainerPath.c_str());
}

std::string MeshHeightMapPanel::GetNormalizedOutputRelativePath() const
{
	const std::string outputPath(outputPathBuffer_.data());
	std::filesystem::path relativePath(EditorAssetPathUtils::ToContentRelativePath(outputPath));
	if (relativePath.is_absolute())
	{
		return "";
	}

	return EnsurePngExtension(relativePath.generic_string());
}
