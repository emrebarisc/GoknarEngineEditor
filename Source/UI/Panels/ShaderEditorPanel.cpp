#include "ShaderEditorPanel.h"

#include <functional>
#include <unordered_set>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <filesystem>

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
#include "Goknar/Contents/Image.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Renderer/ShaderTypes.h"
#include "Goknar/Materials/MaterialFunctionSerializer.h"
#include "Goknar/Materials/MaterialSerializer.h"
#include "Goknar/Helpers/SceneParser.h"
#include "Objects/MeshViewerCameraObject.h"
#include "Controllers/MeshViewerCameraController.h"
#include "UI/EditorAssetPathUtils.h"
#include "UI/EditorUtils.h"
#include "UI/EditorHUD.h"
#include "UI/Panels/SystemFileBrowserPanel.h"

constexpr unsigned int SHADER_EDITOR_RENDER_MASK = 0x20000000;

namespace
{
	constexpr const char* kMaterialEditorReflectionFileType = "MaterialEditorReflection";
	constexpr const char* kMaterialFunctionEditorReflectionFileType = "MaterialFunctionEditorReflection";
	constexpr const char* MATERIAL_NODE_METADATA_PREFIX = "// GOKNAR_MATERIAL_NODE|";
	constexpr int kMaxMaterialArraySize = 64;

	std::string GetAssetBrowserDirectory(const std::string& currentAssetPath)
	{
		if (!currentAssetPath.empty())
		{
			const std::filesystem::path absolutePath = std::filesystem::path(EditorAssetPathUtils::GetContentRootPath()) / currentAssetPath;
			return absolutePath.parent_path().lexically_normal().generic_string();
		}

		return EditorAssetPathUtils::GetContentRootPath();
	}

	ShaderValue GetDefaultValueForPinType(ShaderPinType type)
	{
		switch (type)
		{
		case ShaderPinType::Vector2: return Vector2(0.f);
		case ShaderPinType::Vector3: return Vector3(0.f);
		case ShaderPinType::Vector4: return Vector4(0.f);
		case ShaderPinType::Texture: return std::string("");
		case ShaderPinType::Float:
		case ShaderPinType::Any:
		case ShaderPinType::None:
		default:
			return 0.0f;
		}
	}

	bool IsMaterialValueType(ShaderPinType type)
	{
		return
			type == ShaderPinType::Float ||
			type == ShaderPinType::Vector2 ||
			type == ShaderPinType::Vector3 ||
			type == ShaderPinType::Vector4;
	}

	bool IsMaterialVariableDeclarationCategory(const std::string& category)
	{
		return category == "MaterialVariable" || category == "MaterialVariableArray";
	}

	bool IsMaterialVariableAccessorCategory(const std::string& category)
	{
		return
			category == "MaterialVariableGet" ||
			category == "MaterialVariableSet" ||
			category == "MaterialVariableArrayGet" ||
			category == "MaterialVariableArraySet";
	}

	bool IsMaterialVariableGetterCategory(const std::string& category)
	{
		return category == "MaterialVariableGet" || category == "MaterialVariableArrayGet";
	}

	bool IsMaterialVariableSetterCategory(const std::string& category)
	{
		return category == "MaterialVariableSet" || category == "MaterialVariableArraySet";
	}

	bool IsMaterialVariableArrayAccessorCategory(const std::string& category)
	{
		return category == "MaterialVariableArrayGet" || category == "MaterialVariableArraySet";
	}

	bool IsHiddenCanvasNodeCategory(const std::string& category)
	{
		return IsMaterialVariableDeclarationCategory(category);
	}

	std::string SanitizeIdentifier(std::string value)
	{
		for (char& character : value)
		{
			if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
			{
				character = '_';
			}
		}

		if (value.empty())
		{
			value = "material_function";
		}

		if (std::isdigit(static_cast<unsigned char>(value.front())))
		{
			value.insert(value.begin(), '_');
		}

		return value;
	}

	float GetNodeBodyHeightFromPins(size_t inputPinCount, size_t outputPinCount)
	{
		const size_t maxPinCount = std::max(inputPinCount, outputPinCount);
		return std::max(80.0f, 45.0f + static_cast<float>(maxPinCount) * 25.0f);
	}

	std::string GetMaterialFunctionOutputFunctionName(const std::string& baseFunctionName, const MaterialFunctionPinDefinition& outputDefinition, size_t outputIndex)
	{
		return baseFunctionName + "_out_" + std::to_string(outputIndex) + "_" + SanitizeIdentifier(outputDefinition.name);
	}

	const char* ShaderPinTypeToString(ShaderPinType type)
	{
		switch (type)
		{
		case ShaderPinType::None: return "None";
		case ShaderPinType::Float: return "Float";
		case ShaderPinType::Vector2: return "Vector2";
		case ShaderPinType::Vector3: return "Vector3";
		case ShaderPinType::Vector4: return "Vector4";
		case ShaderPinType::Vector4i: return "Vector4i";
		case ShaderPinType::Matrix4x4: return "Matrix4x4";
		case ShaderPinType::Texture: return "Texture";
		case ShaderPinType::Any: return "Any";
		default: return "None";
		}
	}

	ShaderPinType StringToShaderPinType(const std::string& type)
	{
		if (type == "Float") return ShaderPinType::Float;
		if (type == "Vector2") return ShaderPinType::Vector2;
		if (type == "Vector3") return ShaderPinType::Vector3;
		if (type == "Vector4") return ShaderPinType::Vector4;
		if (type == "Vector4i") return ShaderPinType::Vector4i;
		if (type == "Matrix4x4") return ShaderPinType::Matrix4x4;
		if (type == "Texture") return ShaderPinType::Texture;
		if (type == "Any") return ShaderPinType::Any;
		return ShaderPinType::None;
	}

	const char* ShaderPinKindToString(ShaderPinKind kind)
	{
		return kind == ShaderPinKind::Input ? "Input" : "Output";
	}

	ShaderPinKind StringToShaderPinKind(const std::string& kind)
	{
		return kind == "Output" ? ShaderPinKind::Output : ShaderPinKind::Input;
	}

	std::string Trim(const std::string& value)
	{
		const size_t first = value.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
		{
			return "";
		}

		const size_t last = value.find_last_not_of(" \t\r\n");
		return value.substr(first, last - first + 1);
	}

	std::vector<std::string> SplitString(const std::string& value, char delimiter)
	{
		std::vector<std::string> parts;
		std::stringstream stream(value);
		std::string part;
		while (std::getline(stream, part, delimiter))
		{
			parts.push_back(part);
		}
		return parts;
	}

	bool ParseFloatValue(const std::string& value, float& outValue)
	{
		std::stringstream stream(Trim(value));
		stream >> outValue;
		return !stream.fail();
	}

	std::unordered_map<std::string, std::string> ParseMetadataFields(const std::string& metadataText)
	{
		std::unordered_map<std::string, std::string> fields;
		for (const std::string& token : SplitString(metadataText, '|'))
		{
			const size_t separatorIndex = token.find('=');
			if (separatorIndex == std::string::npos)
			{
				continue;
			}

			fields[Trim(token.substr(0, separatorIndex))] = Trim(token.substr(separatorIndex + 1));
		}

		return fields;
	}

	void EnsureValueMatchesType(ShaderValue& value, ShaderPinType type)
	{
		switch (type)
		{
		case ShaderPinType::Vector2:
			if (!std::holds_alternative<Vector2>(value))
			{
				value = Vector2(0.f);
			}
			return;
		case ShaderPinType::Vector3:
			if (!std::holds_alternative<Vector3>(value))
			{
				value = Vector3(0.f);
			}
			return;
		case ShaderPinType::Vector4:
			if (!std::holds_alternative<Vector4>(value))
			{
				value = Vector4(0.f);
			}
			return;
		case ShaderPinType::Float:
		default:
			if (!std::holds_alternative<float>(value))
			{
				value = 0.0f;
			}
			return;
		}
	}

	void EnsureArrayDefaultsMatchNode(ShaderNode& node)
	{
		if (node.typeCategory != "MaterialVariableArray")
		{
			return;
		}

		ShaderPinType elementType = ShaderPinType::Float;
		if (!node.outputs.empty())
		{
			elementType = node.outputs[0].type;
		}
		if (!IsMaterialValueType(elementType))
		{
			elementType = ShaderPinType::Float;
			if (!node.outputs.empty())
			{
				node.outputs[0].type = elementType;
			}
		}

		if (node.arrayDefaultValues.empty())
		{
			node.arrayDefaultValues.push_back(GetDefaultValueForPinType(elementType));
		}

		for (ShaderValue& value : node.arrayDefaultValues)
		{
			EnsureValueMatchesType(value, elementType);
		}
	}

	std::string GetMaterialVariableReferenceName(const ShaderNode& node)
	{
		if (IsMaterialVariableAccessorCategory(node.typeCategory))
		{
			return SanitizeIdentifier(node.stringData.empty() ? node.name : node.stringData);
		}

		return SanitizeIdentifier(node.name);
	}

	const ShaderNode* FindMaterialVariableDeclarationNode(const std::vector<ShaderNode>& nodes, const std::string& name)
	{
		const std::string referenceName = SanitizeIdentifier(name);
		for (const ShaderNode& node : nodes)
		{
			if (!IsMaterialVariableDeclarationCategory(node.typeCategory))
			{
				continue;
			}

			if (SanitizeIdentifier(node.name) == referenceName)
			{
				return &node;
			}
		}

		return nullptr;
	}

	bool DrawShaderValueEditor(const char* label, ShaderPinType type, ShaderValue& value)
	{
		EnsureValueMatchesType(value, type);

		switch (type)
		{
		case ShaderPinType::Vector2:
		{
			Vector2 currentValue = std::get<Vector2>(value);
			float components[2] = { currentValue.x, currentValue.y };
			if (ImGui::DragFloat2(label, components, 0.01f))
			{
				value = Vector2(components[0], components[1]);
				return true;
			}
			return false;
		}
		case ShaderPinType::Vector3:
		{
			Vector3 currentValue = std::get<Vector3>(value);
			float components[3] = { currentValue.x, currentValue.y, currentValue.z };
			if (ImGui::DragFloat3(label, components, 0.01f))
			{
				value = Vector3(components[0], components[1], components[2]);
				return true;
			}
			return false;
		}
		case ShaderPinType::Vector4:
		{
			Vector4 currentValue = std::get<Vector4>(value);
			float components[4] = { currentValue.x, currentValue.y, currentValue.z, currentValue.w };
			if (ImGui::DragFloat4(label, components, 0.01f))
			{
				value = Vector4(components[0], components[1], components[2], components[3]);
				return true;
			}
			return false;
		}
		case ShaderPinType::Float:
		default:
		{
			float currentValue = std::get<float>(value);
			if (ImGui::DragFloat(label, &currentValue, 0.01f))
			{
				value = currentValue;
				return true;
			}
			return false;
		}
		}
	}

	std::string ShaderValueToMetadataString(const ShaderValue& value, ShaderPinType type)
	{
		std::ostringstream stream;
		stream << std::fixed << std::setprecision(6);

		switch (type)
		{
		case ShaderPinType::Vector2:
		{
			const Vector2 typedValue = std::holds_alternative<Vector2>(value) ? std::get<Vector2>(value) : Vector2(0.f);
			stream << typedValue.x << "," << typedValue.y;
			break;
		}
		case ShaderPinType::Vector3:
		{
			const Vector3 typedValue = std::holds_alternative<Vector3>(value) ? std::get<Vector3>(value) : Vector3(0.f);
			stream << typedValue.x << "," << typedValue.y << "," << typedValue.z;
			break;
		}
		case ShaderPinType::Vector4:
		{
			const Vector4 typedValue = std::holds_alternative<Vector4>(value) ? std::get<Vector4>(value) : Vector4(0.f);
			stream << typedValue.x << "," << typedValue.y << "," << typedValue.z << "," << typedValue.w;
			break;
		}
		case ShaderPinType::Float:
		default:
			stream << (std::holds_alternative<float>(value) ? std::get<float>(value) : 0.0f);
			break;
		}

		return stream.str();
	}

	ShaderValue ShaderValueFromMetadataString(ShaderPinType type, const std::string& text)
	{
		const std::vector<std::string> components = SplitString(text, ',');
		switch (type)
		{
		case ShaderPinType::Vector2:
		{
			Vector2 value(0.f);
			if (components.size() >= 2)
			{
				ParseFloatValue(components[0], value.x);
				ParseFloatValue(components[1], value.y);
			}
			return value;
		}
		case ShaderPinType::Vector3:
		{
			Vector3 value(0.f);
			if (components.size() >= 3)
			{
				ParseFloatValue(components[0], value.x);
				ParseFloatValue(components[1], value.y);
				ParseFloatValue(components[2], value.z);
			}
			return value;
		}
		case ShaderPinType::Vector4:
		{
			Vector4 value(0.f);
			if (components.size() >= 4)
			{
				ParseFloatValue(components[0], value.x);
				ParseFloatValue(components[1], value.y);
				ParseFloatValue(components[2], value.z);
				ParseFloatValue(components[3], value.w);
			}
			return value;
		}
		case ShaderPinType::Float:
		default:
		{
			float value = 0.0f;
			ParseFloatValue(text, value);
			return value;
		}
		}
	}

	void SerializeShaderValue(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* parentElement, const char* elementName, const ShaderValue& value)
	{
		tinyxml2::XMLElement* valueElement = doc.NewElement(elementName);
		if (std::holds_alternative<float>(value))
		{
			valueElement->SetAttribute("type", "Float");
			valueElement->SetAttribute("x", std::get<float>(value));
		}
		else if (std::holds_alternative<Vector2>(value))
		{
			const Vector2 typedValue = std::get<Vector2>(value);
			valueElement->SetAttribute("type", "Vector2");
			valueElement->SetAttribute("x", typedValue.x);
			valueElement->SetAttribute("y", typedValue.y);
		}
		else if (std::holds_alternative<Vector3>(value))
		{
			const Vector3 typedValue = std::get<Vector3>(value);
			valueElement->SetAttribute("type", "Vector3");
			valueElement->SetAttribute("x", typedValue.x);
			valueElement->SetAttribute("y", typedValue.y);
			valueElement->SetAttribute("z", typedValue.z);
		}
		else if (std::holds_alternative<Vector4>(value))
		{
			const Vector4 typedValue = std::get<Vector4>(value);
			valueElement->SetAttribute("type", "Vector4");
			valueElement->SetAttribute("x", typedValue.x);
			valueElement->SetAttribute("y", typedValue.y);
			valueElement->SetAttribute("z", typedValue.z);
			valueElement->SetAttribute("w", typedValue.w);
		}
		else if (std::holds_alternative<std::string>(value))
		{
			valueElement->SetAttribute("type", "String");
			valueElement->SetText(std::get<std::string>(value).c_str());
		}

		parentElement->InsertEndChild(valueElement);
	}

	void SerializePinDefaultValue(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* pinElement, const ShaderPin& pin)
	{
		SerializeShaderValue(doc, pinElement, "DefaultValue", pin.defaultValue);
	}

	ShaderValue DeserializeShaderValue(tinyxml2::XMLElement* valueElement)
	{
		if (!valueElement)
		{
			return 0.0f;
		}

		const std::string type = valueElement->Attribute("type") ? valueElement->Attribute("type") : "";
		if (type == "Vector2")
		{
			return Vector2(valueElement->FloatAttribute("x"), valueElement->FloatAttribute("y"));
		}
		if (type == "Vector3")
		{
			return Vector3(valueElement->FloatAttribute("x"), valueElement->FloatAttribute("y"), valueElement->FloatAttribute("z"));
		}
		if (type == "Vector4")
		{
			return Vector4(valueElement->FloatAttribute("x"), valueElement->FloatAttribute("y"), valueElement->FloatAttribute("z"), valueElement->FloatAttribute("w"));
		}
		if (type == "String")
		{
			return std::string(valueElement->GetText() ? valueElement->GetText() : "");
		}

		return valueElement->FloatAttribute("x");
	}

	ShaderValue DeserializePinDefaultValue(tinyxml2::XMLElement* pinElement)
	{
		return DeserializeShaderValue(pinElement ? pinElement->FirstChildElement("DefaultValue") : nullptr);
	}

	std::string BuildMaterialNodeMetadataLine(const ShaderNode& node)
	{
		if (node.typeCategory != "MaterialVariable" && node.typeCategory != "MaterialVariableArray")
		{
			return "";
		}

		const bool isArray = node.typeCategory == "MaterialVariableArray";
		const ShaderPinType valueType =
			isArray ? (node.outputs.empty() ? ShaderPinType::Float : node.outputs[0].type) :
			(node.outputs.empty() ? ShaderPinType::Float : node.outputs[0].type);
		if (!IsMaterialValueType(valueType))
		{
			return "";
		}

		std::string valuesText;
		if (isArray)
		{
			std::vector<std::string> encodedValues;
			for (const ShaderValue& value : node.arrayDefaultValues)
			{
				encodedValues.push_back(ShaderValueToMetadataString(value, valueType));
			}

			for (size_t index = 0; index < encodedValues.size(); ++index)
			{
				if (index > 0)
				{
					valuesText += ";";
				}
				valuesText += encodedValues[index];
			}
		}
		else
		{
			const ShaderValue defaultValue =
				node.outputs.empty() ? GetDefaultValueForPinType(valueType) : node.outputs[0].defaultValue;
			valuesText = ShaderValueToMetadataString(defaultValue, valueType);
		}

		std::ostringstream stream;
		stream <<
			MATERIAL_NODE_METADATA_PREFIX <<
			"kind=" << (isArray ? "Array" : "Variable") <<
			"|name=" << node.name <<
			"|type=" << ShaderPinTypeToString(valueType) <<
			"|storage=" << (node.isUniform ? "Uniform" : "Global") <<
			"|count=" << (isArray ? node.arrayDefaultValues.size() : 1) <<
			"|values=" << valuesText;
		return stream.str();
	}

	std::string BuildMaterialNodeDeclaration(const ShaderNode& node)
	{
		if (node.typeCategory != "MaterialVariable" && node.typeCategory != "MaterialVariableArray")
		{
			return "";
		}

		const bool isArray = node.typeCategory == "MaterialVariableArray";
		const ShaderPinType valueType = node.outputs.empty() ? ShaderPinType::Float : node.outputs[0].type;
		if (!IsMaterialValueType(valueType))
		{
			return "";
		}

		const char* glslType = valueType == ShaderPinType::Float ? "float" :
			valueType == ShaderPinType::Vector2 ? "vec2" :
			valueType == ShaderPinType::Vector3 ? "vec3" :
			"vec4";

		std::string declaration = BuildMaterialNodeMetadataLine(node) + "\n";
		if (node.isUniform)
		{
			declaration += "uniform ";
			declaration += glslType;
			declaration += " ";
			declaration += node.name;
			if (isArray)
			{
				declaration += "[" + std::to_string(node.arrayDefaultValues.size()) + "]";
			}
			declaration += ";\n";
			return declaration;
		}

		declaration += glslType;
		declaration += " ";
		declaration += node.name;
		if (isArray)
		{
			declaration += "[" + std::to_string(node.arrayDefaultValues.size()) + "] = ";
			declaration += glslType;
			declaration += "[" + std::to_string(node.arrayDefaultValues.size()) + "](";
			for (size_t index = 0; index < node.arrayDefaultValues.size(); ++index)
			{
				if (index > 0)
				{
					declaration += ", ";
				}
				declaration += valueType == ShaderPinType::Float ?
					ShaderValueToMetadataString(node.arrayDefaultValues[index], valueType) + "f" :
					std::string(glslType) + "(" + ShaderValueToMetadataString(node.arrayDefaultValues[index], valueType) + ")";
			}
			declaration += ");\n";
		}
		else
		{
			const ShaderValue defaultValue = node.outputs.empty() ? GetDefaultValueForPinType(valueType) : node.outputs[0].defaultValue;
			declaration += " = ";
			declaration += valueType == ShaderPinType::Float ?
				ShaderValueToMetadataString(defaultValue, valueType) + "f" :
				std::string(glslType) + "(" + ShaderValueToMetadataString(defaultValue, valueType) + ")";
			declaration += ";\n";
		}

		return declaration;
	}

	bool TryParseMaterialNodeMetadata(const std::string& line, ShaderNode& outNode)
	{
		const size_t markerIndex = line.find(MATERIAL_NODE_METADATA_PREFIX);
		if (markerIndex == std::string::npos)
		{
			return false;
		}

		const std::unordered_map<std::string, std::string> fields =
			ParseMetadataFields(line.substr(markerIndex + std::char_traits<char>::length(MATERIAL_NODE_METADATA_PREFIX)));

		auto kindIterator = fields.find("kind");
		auto nameIterator = fields.find("name");
		auto typeIterator = fields.find("type");
		auto storageIterator = fields.find("storage");
		auto valuesIterator = fields.find("values");
		if (kindIterator == fields.end() ||
			nameIterator == fields.end() ||
			typeIterator == fields.end() ||
			storageIterator == fields.end() ||
			valuesIterator == fields.end())
		{
			return false;
		}

		const ShaderPinType valueType = StringToShaderPinType(typeIterator->second);
		if (!IsMaterialValueType(valueType))
		{
			return false;
		}

		const bool isArray = kindIterator->second == "Array";
		outNode = {};
		outNode.name = SanitizeIdentifier(nameIterator->second);
		outNode.typeCategory = isArray ? "MaterialVariableArray" : "MaterialVariable";
		outNode.isUniform = storageIterator->second != "Global";

		if (isArray)
		{
			outNode.inputs.push_back({ 0, 0, "Index", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
			outNode.outputs.push_back({ 0, 0, "Value", valueType, ShaderPinKind::Output, GetDefaultValueForPinType(valueType) });
			outNode.outputs.push_back({ 0, 0, "Count", ShaderPinType::Float, ShaderPinKind::Output, 0.0f });
			for (const std::string& entry : SplitString(valuesIterator->second, ';'))
			{
				outNode.arrayDefaultValues.push_back(ShaderValueFromMetadataString(valueType, entry));
			}
			EnsureArrayDefaultsMatchNode(outNode);
		}
		else
		{
			outNode.outputs.push_back({ 0, 0, "Value", valueType, ShaderPinKind::Output, ShaderValueFromMetadataString(valueType, valuesIterator->second) });
		}

		return true;
	}
}

ShaderEditorPanel::ShaderEditorPanel(EditorHUD* hud)
	: IEditorPanel("Shader Graph Editor", hud),
	editorHUD_(hud)
{
	cameraObject_ = new MeshViewerCameraObject();
	cameraObject_->SetName("__Editor__PreviewCamera");
	cameraObject_->GetCameraComponent()->GetCamera()->SetCameraType(CameraType::RenderTarget);
	cameraObject_->GetCameraComponent()->GetCamera()->SetRenderMask(SHADER_EDITOR_RENDER_MASK);
	cameraObject_->SetWorldPosition({ 0.f, 0.f, 100.f });

	renderTarget_ = new RenderTarget();
	renderTarget_->SetCamera(cameraObject_->GetCameraComponent()->GetCamera());
	renderTarget_->SetRerenderShadowMaps(false);

	viewedObject_ = new ObjectBase();
	viewedObject_->SetName("__Editor__PreviewTarget");
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
	SetAssetMode(ShaderEditorAssetMode::Material);
	ResetEditorGraph(ImVec2(500.0f, 300.0f));

	renderTarget_->Init();
	renderTarget_->SetFrameSize(previewSize_);

	cameraObject_->GetController()->ResetViewWithBoundingBox(viewedObject_, staticMeshComponent_->GetMeshInstance()->GetMesh()->GetAABB());
}

void ShaderEditorPanel::ResetInteractionState()
{
	selectedNodeIds_.clear();
	preDragSelection_.clear();
	clipboardNodes_.clear();
	clipboardLinks_.clear();
	selectedNodeId_ = -1;
	selectedLinkId_ = -1;
	scale_ = 1.0f;
	scrolling_ = ImVec2(0.0f, 0.0f);
	isDraggingSelection_ = false;
	isBackgroundClicked_ = false;
	isDraggingLink_ = false;
	autoConnectStartPinId_ = -1;
	draggingStartPinId_ = -1;
}

void ShaderEditorPanel::SetAssetMode(ShaderEditorAssetMode assetMode)
{
	activeAssetMode_ = assetMode;
}

void ShaderEditorPanel::ResetEditorGraph(const ImVec2& masterNodePos)
{
	nodes_.clear();
	links_.clear();
	textures_.clear();
	ResetInteractionState();
	nextId_ = 1;

	ShaderNode masterNode;
	masterNode.id = nextId_++;
	masterNode.name = IsEditingMaterial() ? "Material Output" : "Function Output";
	masterNode.typeCategory = "Master";
	masterNode.pos = masterNodePos;

	if (IsEditingMaterial())
	{
		masterNode.size = ImVec2(200, 150);
		masterNode.inputs.push_back({ nextId_++, masterNode.id, "Base Color", ShaderPinType::Vector4, ShaderPinKind::Input, Vector4(1.f) });
		masterNode.inputs.push_back({ nextId_++, masterNode.id, "Emissive", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f) });
		masterNode.inputs.push_back({ nextId_++, masterNode.id, "Normal", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f, 0.f, 1.f) });
		masterNode.inputs.push_back({ nextId_++, masterNode.id, "World Position Offset", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f) });
	}
	else
	{
		masterNode.size = ImVec2(220, 80);
		masterNode.inputs.push_back({ nextId_++, masterNode.id, "Result", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
	}

	masterNodeId_ = masterNode.id;
	nodes_.push_back(masterNode);
}

ShaderEditorPanel::ShaderEditorSnapshot ShaderEditorPanel::CaptureSnapshot() const
{
	ShaderEditorSnapshot snapshot;
	snapshot.assetMode = activeAssetMode_;
	snapshot.assetPath = currentAssetPath_;
	snapshot.nodes = nodes_;
	snapshot.links = links_;
	snapshot.textures = textures_;
	snapshot.selectedNodeIds = selectedNodeIds_;
	snapshot.ambientReflectance = ambientReflectance_;
	snapshot.specularReflectance = specularReflectance_;
	snapshot.scrolling = scrolling_;
	snapshot.translucency = translucency_;
	snapshot.phongExponent = phongExponent_;
	snapshot.scale = scale_;
	snapshot.selectedNodeId = selectedNodeId_;
	snapshot.selectedLinkId = selectedLinkId_;
	snapshot.masterNodeId = masterNodeId_;
	snapshot.nextId = nextId_;
	snapshot.blendModel = blendModel_;
	snapshot.shadingModel = shadingModel_;
	return snapshot;
}

void ShaderEditorPanel::RestoreSnapshot(const ShaderEditorSnapshot& snapshot)
{
	ResetInteractionState();

	activeAssetMode_ = snapshot.assetMode;
	currentAssetPath_ = snapshot.assetPath;
	nodes_ = snapshot.nodes;
	links_ = snapshot.links;
	textures_ = snapshot.textures;
	selectedNodeIds_ = snapshot.selectedNodeIds;
	ambientReflectance_ = snapshot.ambientReflectance;
	specularReflectance_ = snapshot.specularReflectance;
	scrolling_ = snapshot.scrolling;
	translucency_ = snapshot.translucency;
	phongExponent_ = snapshot.phongExponent;
	scale_ = snapshot.scale;
	selectedNodeId_ = snapshot.selectedNodeId;
	selectedLinkId_ = snapshot.selectedLinkId;
	masterNodeId_ = snapshot.masterNodeId;
	nextId_ = snapshot.nextId;
	blendModel_ = snapshot.blendModel;
	shadingModel_ = snapshot.shadingModel;
	selectedNodeIds_ = snapshot.selectedNodeIds;
	selectedNodeId_ = snapshot.selectedNodeId;
	selectedLinkId_ = snapshot.selectedLinkId;
}

void ShaderEditorPanel::CaptureSnapshotForNavigation()
{
	navigationStack_.push_back(CaptureSnapshot());
}

bool ShaderEditorPanel::RestorePreviousSnapshot()
{
	if (navigationStack_.empty())
	{
		return false;
	}

	const ShaderEditorSnapshot snapshot = navigationStack_.back();
	navigationStack_.pop_back();
	RestoreSnapshot(snapshot);

	if (IsEditingMaterial())
	{
		RebuildActiveMaterialFromGraph();
	}

	return true;
}

void ShaderEditorPanel::OnMaterialOpened(const std::string& path)
{
	navigationStack_.clear();
	SetAssetMode(ShaderEditorAssetMode::Material);

	tinyxml2::XMLDocument doc;
	if (doc.LoadFile(path.c_str()) != tinyxml2::XML_SUCCESS)
	{
		GOKNAR_INFO("ShaderEditorPanel: Selected file is not a valid XML file.");
		return;
	}

	tinyxml2::XMLElement* root = doc.FirstChildElement("GameAsset");
	if (!root)
	{
		GOKNAR_INFO("ShaderEditorPanel: Selected file does not have GameAsset root.");
		return;
	}

	const char* fileType = root->Attribute("FileType");
	if (!fileType || std::string(fileType) != "Material")
	{
		GOKNAR_INFO("ShaderEditorPanel: Selected file is not a Material XML file.");
		return;
	}

	GOKNAR_INFO("ShaderEditorPanel: Successfully loaded Material XML.");

	currentAssetPath_ = EditorAssetPathUtils::ToContentRelativePath(path);
	activeMaterial_ = SceneParser::GetOrCreateSharedMaterial(currentAssetPath_);

	ResetEditorGraph(ImVec2(700.0f, 300.0f));
	ambientReflectance_ = Vector3::ZeroVector;
	specularReflectance_ = Vector3::ZeroVector;
	blendModel_ = MaterialBlendModel::Opaque;
	shadingModel_ = MaterialShadingModel::Default;
	translucency_ = 0.0f;
	phongExponent_ = 1.0f;

	auto GetText = [&](const char* name) -> std::string {
		tinyxml2::XMLElement* el = root->FirstChildElement(name);
		return el && el->GetText() ? el->GetText() : "";
		};

	std::string blendStr = GetText("BlendModel");
	if (blendStr == "Masked") blendModel_ = MaterialBlendModel::Masked;
	else if (blendStr == "Transparent") blendModel_ = MaterialBlendModel::Transparent;
	else blendModel_ = MaterialBlendModel::Opaque;

	std::string shadeStr = GetText("ShadingModel");
	if (shadeStr == "TwoSided") shadingModel_ = MaterialShadingModel::TwoSided;
	else shadingModel_ = MaterialShadingModel::Default;

	std::string transStr = GetText("Translucency");
	if (!transStr.empty()) translucency_ = std::stof(transStr);

	std::string phongStr = GetText("PhongExponent");
	if (!phongStr.empty())
	{
		std::stringstream stream(phongStr);
		float loadedPhongExponent = 1.f;
		stream >> loadedPhongExponent;
		if (!stream.fail() && std::isfinite(loadedPhongExponent) && loadedPhongExponent >= 1.f)
		{
			phongExponent_ = loadedPhongExponent;
		}
	}

	auto ParseVec3 = [&](const char* name, Vector3& vec) {
		std::string s = GetText(name);
		if (!s.empty()) {
			std::stringstream ss(s);
			ss >> vec.x >> vec.y >> vec.z;
		}
		};
	ParseVec3("AmbientReflectance", ambientReflectance_);
	ParseVec3("SpecularReflectance", specularReflectance_);

	ImVec2 nodePos(100, 100);

	tinyxml2::XMLElement* texChild = root->FirstChildElement("Texture");
	while (texChild)
	{
		if (texChild->Attribute("path") && texChild->Attribute("name"))
		{
			std::string tPath = texChild->Attribute("path");
			std::string tName = texChild->Attribute("name");
			textures_.push_back({ tName, tPath });

			ShaderNode texNode = SpawnNode("Texture", "Texture Sample", nodePos);
			texNode.stringData = tName;
			nodes_.push_back(texNode);
			nodePos.y += 100;
		}
		texChild = texChild->NextSiblingElement("Texture");
	}

	auto HasCalculation = [&](const char* name) -> bool {
		tinyxml2::XMLElement* el = root->FirstChildElement(name);
		if (el) {
			tinyxml2::XMLElement* calc = el->FirstChildElement("Calculation");
			std::string c = calc && calc->GetText() ? calc->GetText() : "";
			return c.find_first_not_of(" \n\r\t") != std::string::npos;
		}
		return false;
		};

	auto ParseColorConstant = [&](const char* name, int inputIndex, bool isVec4) {
		const std::string propertyName = name ? name : "";
		if (HasCalculation((propertyName == "DiffuseReflectance" || propertyName == "BaseColorValue") ? "BaseColor" : "EmissiveColor")) return;

		std::string s = GetText(name);
		if (!s.empty()) {
			std::stringstream ss(s);
			if (isVec4) {
				Vector4 c;
				ss >> c.x >> c.y >> c.z >> c.w;
				ShaderNode cn = SpawnNode("Constants", "Vector4 Constant", nodePos);
				cn.inputs[0].defaultValue = c.x; cn.inputs[1].defaultValue = c.y;
				cn.inputs[2].defaultValue = c.z; cn.inputs[3].defaultValue = c.w;
				nodes_.push_back(cn);
				links_.push_back({ nextId_++, cn.outputs[0].id, nodes_[0].inputs[inputIndex].id });
			}
			else {
				Vector3 c;
				ss >> c.x >> c.y >> c.z;
				ShaderNode cn = SpawnNode("Constants", "Vector3 Constant", nodePos);
				cn.inputs[0].defaultValue = c.x; cn.inputs[1].defaultValue = c.y;
				cn.inputs[2].defaultValue = c.z;
				nodes_.push_back(cn);
				links_.push_back({ nextId_++, cn.outputs[0].id, nodes_[0].inputs[inputIndex].id });
			}
			nodePos.y += 150;
		}
		};

	ParseColorConstant("DiffuseReflectance", 0, false);
	ParseColorConstant("BaseColorValue", 0, true);
	ParseColorConstant("EmmisiveColorValue", 1, false);

	auto AddCustomGLSLNode = [&](const char* tag, int masterInputIndex) {
		tinyxml2::XMLElement* el = root->FirstChildElement(tag);
		if (el) {
			std::string calcStr = el->FirstChildElement("Calculation") && el->FirstChildElement("Calculation")->GetText() ? el->FirstChildElement("Calculation")->GetText() : "";
			std::string resStr = el->FirstChildElement("Result") && el->FirstChildElement("Result")->GetText() ? el->FirstChildElement("Result")->GetText() : "";

			if (!calcStr.empty() || (!resStr.empty() && resStr.find("vec") != std::string::npos)) {
				ShaderNode customNode = SpawnNode("Custom", "Custom GLSL", nodePos);
				nodePos.y += 150;
				customNode.stringData = calcStr + "\nRETURN_RESULT:" + resStr;
				nodes_.push_back(customNode);
				links_.push_back({ nextId_++, customNode.outputs[0].id, nodes_[0].inputs[masterInputIndex].id });
			}
		}
		};

	AddCustomGLSLNode("BaseColor", 0);
	AddCustomGLSLNode("EmissiveColor", 1);
	AddCustomGLSLNode("FragmentNormal", 2);
	AddCustomGLSLNode("VertexPositionOffset", 3);

	std::unordered_set<std::string> parsedMaterialNodeNames;
	auto AddMaterialNodesFromShaderText = [&](const std::string& shaderText)
	{
		std::stringstream stream(shaderText);
		std::string line;
		while (std::getline(stream, line))
		{
			ShaderNode parsedNode;
			if (!TryParseMaterialNodeMetadata(line, parsedNode) || parsedMaterialNodeNames.find(parsedNode.name) != parsedMaterialNodeNames.end())
			{
				continue;
			}

			ShaderNode node = SpawnNode(parsedNode.typeCategory, parsedNode.name, nodePos);
			node.name = parsedNode.name;
			node.isUniform = parsedNode.isUniform;
			if (!parsedNode.outputs.empty() && !node.outputs.empty())
			{
				node.outputs[0].type = parsedNode.outputs[0].type;
				node.outputs[0].defaultValue = parsedNode.outputs[0].defaultValue;
			}
			if (node.typeCategory == "MaterialVariableArray")
			{
				node.arrayDefaultValues = parsedNode.arrayDefaultValues;
				EnsureArrayDefaultsMatchNode(node);
			}
			nodes_.push_back(node);
			parsedMaterialNodeNames.insert(node.name);
			nodePos.y += 130.0f;
		}
	};

	AddMaterialNodesFromShaderText(GetText("VertexShaderUniforms"));
	AddMaterialNodesFromShaderText(GetText("FragmentShaderUniforms"));

	if (!LoadEditorReflection(path))
	{
		ApplyHierarchicalLayout();
		SaveEditorReflection(path);
	}

	RebuildActiveMaterialFromGraph();
}

void ShaderEditorPanel::OnMaterialFunctionOpened(const std::string& path)
{
	navigationStack_.clear();
	SetAssetMode(ShaderEditorAssetMode::MaterialFunction);
	currentAssetPath_ = EditorAssetPathUtils::ToContentRelativePath(path);

	MaterialFunction materialFunction;
	if (!LoadMaterialFunctionSignature(currentAssetPath_, materialFunction))
	{
		GOKNAR_INFO("ShaderEditorPanel: Selected file is not a Material Function asset.");
		return;
	}

	ResetEditorGraph(ImVec2(700.0f, 300.0f));
	ApplyMaterialFunctionSignatureToGraph(materialFunction);

	if (!LoadEditorReflection(path))
	{
		ApplyHierarchicalLayout();
		SaveEditorReflection(path);
	}
}

void ShaderEditorPanel::OnMaterialSaved(const std::string& path)
{
	currentAssetPath_ = EditorAssetPathUtils::ToContentRelativePath(path);
	RebuildActiveMaterialFromGraph();
	if (!activeMaterial_)
	{
		return;
	}

	MaterialSerializer::Serialize(currentAssetPath_, activeMaterial_);
	SaveEditorReflection(path);
}

void ShaderEditorPanel::OnMaterialFunctionSaved(const std::string& path)
{
	currentAssetPath_ = EditorAssetPathUtils::ToContentRelativePath(path);

	MaterialFunction materialFunction;
	if (!BuildActiveMaterialFunction(materialFunction))
	{
		return;
	}

	if (!MaterialFunctionSerializer::Serialize(currentAssetPath_, materialFunction))
	{
		GOKNAR_INFO("ShaderEditorPanel: Failed to save Material Function '%s'.", currentAssetPath_.c_str());
		return;
	}

	SaveEditorReflection(path);
}

void ShaderEditorPanel::SaveCurrentMaterial()
{
	if (!IsEditingMaterial())
	{
		return;
	}

	if (currentAssetPath_.empty())
	{
		return;
	}

	OnMaterialSaved(currentAssetPath_);
}

void ShaderEditorPanel::SaveCurrentAsset()
{
	if (currentAssetPath_.empty())
	{
		return;
	}

	if (IsEditingMaterial())
	{
		OnMaterialSaved(currentAssetPath_);
	}
	else
	{
		OnMaterialFunctionSaved(currentAssetPath_);
	}
}

void ShaderEditorPanel::RebuildActiveMaterialFromGraph()
{
	if (!IsEditingMaterial())
	{
		return;
	}

	SynchronizeMaterialVariableAccessorNodes();

	if (activeMaterial_)
	{
		if (!currentAssetPath_.empty())
		{
			activeMaterial_ = SceneParser::GetOrCreateSharedMaterial(currentAssetPath_);
		}
	}
	else if (!currentAssetPath_.empty())
	{
		activeMaterial_ = SceneParser::GetOrCreateSharedMaterial(currentAssetPath_);
	}
	else
	{
		activeMaterial_ = new Material();
	}

	activeMaterial_->ResetForRebuild();

	activeMaterial_->SetBlendModel(blendModel_);
	activeMaterial_->SetShadingModel(shadingModel_);
	activeMaterial_->SetTranslucency(translucency_);
	activeMaterial_->SetPhongExponent(phongExponent_);
	activeMaterial_->SetAmbientReflectance(ambientReflectance_);
	activeMaterial_->SetSpecularReflectance(specularReflectance_);

	MaterialInitializationData* initData = activeMaterial_->GetInitializationData();
	if (!initData)
	{
		return;
	}

	std::string missingTextureUniformDeclarations;
	for (const auto& tex : textures_)
	{
		Image* image = engine->GetResourceManager()->GetContent<Image>(tex.path);
		if (image)
		{
			image->SetName(tex.name);
			activeMaterial_->AddTextureImage(image);
		}
		else
		{
			GOKNAR_INFO("ShaderEditorPanel: Texture '%s' not found at path '%s'. Injecting fallback uniform.", tex.name, tex.path);
			missingTextureUniformDeclarations += "uniform sampler2D " + tex.name + ";\n";
		}
	}

	CompileGraphToMaterial(initData);
	initData->fragmentShaderUniforms += missingTextureUniformDeclarations;

	activeMaterial_->Build(nullptr);
	activeMaterial_->PreInit();
	activeMaterial_->Init();
	activeMaterial_->PostInit();

	if (staticMeshComponent_->GetMeshInstance())
	{
		staticMeshComponent_->GetMeshInstance()->SetMaterial(MaterialInstance::Create(activeMaterial_));
	}
}

void ShaderEditorPanel::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(title_.c_str(), &isOpen_)) { ImGui::End(); return; }

	SynchronizeMaterialVariableAccessorNodes();

	if (IsEditingMaterial() && ImGui::Button("Compile Material"))
	{
		RebuildActiveMaterialFromGraph();
	}

	if (ImGui::Button(GetOpenButtonLabel().c_str()))
	{
		if (SystemFileBrowserPanel* browser = editorHUD_->GetPanel<SystemFileBrowserPanel>())
		{
			browser->OpenFileSelector(
				IsEditingMaterial() ?
					Delegate<void(const std::string&)>::Create<ShaderEditorPanel, &ShaderEditorPanel::OnMaterialOpened>(this) :
					Delegate<void(const std::string&)>::Create<ShaderEditorPanel, &ShaderEditorPanel::OnMaterialFunctionOpened>(this),
				GetAssetBrowserDirectory(currentAssetPath_),
				EditorAssetPathUtils::GetProjectRootPath());
		}
	}

	ImGui::SameLine();

	if (ImGui::Button(GetSaveButtonLabel().c_str()))
	{
		if (!currentAssetPath_.empty())
		{
			SaveCurrentAsset();
		}
		else if (SystemFileBrowserPanel* browser = editorHUD_->GetPanel<SystemFileBrowserPanel>())
		{
			browser->SaveFileSelector(
				IsEditingMaterial() ?
					Delegate<void(const std::string&)>::Create<ShaderEditorPanel, &ShaderEditorPanel::OnMaterialSaved>(this) :
					Delegate<void(const std::string&)>::Create<ShaderEditorPanel, &ShaderEditorPanel::OnMaterialFunctionSaved>(this),
				GetAssetBrowserDirectory(currentAssetPath_),
				"",
				EditorAssetPathUtils::GetProjectRootPath());
		}
	}

	if (!navigationStack_.empty())
	{
		ImGui::SameLine();
		if (ImGui::Button(GetBackButtonLabel().c_str()))
		{
			RestorePreviousSnapshot();
		}
	}

	ImGui::Separator();

	static ImGuiTableFlags rootFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;
	if (ImGui::BeginTable("ShaderEditorLayout", 3, rootFlags))
	{
		ImGui::TableSetupColumn("Material", ImGuiTableColumnFlags_WidthFixed, 300.0f);
		ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthFixed, 300.0f);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::BeginChild("MaterialPropertiesChild");
		if (IsEditingMaterial())
		{
			DrawPreview();
			DrawMaterialProperties();
		}
		else
		{
			DrawMaterialFunctionProperties();
		}
		ImGui::EndChild();

		ImGui::TableSetColumnIndex(1);
		DrawNodeCanvas();

		ImGui::TableSetColumnIndex(2);
		ImGui::BeginChild("PropertiesSidebarChild");
		DrawPropertiesSidebar();
		ImGui::EndChild();

		ImGui::EndTable();
	}
	ImGui::End();
}

void ShaderEditorPanel::SaveEditorReflection(const std::string& assetPath)
{
	const std::string reflectionPath = EditorAssetPathUtils::ToEditorReflectionPath(assetPath);
	if (!EditorAssetPathUtils::EnsureDirectoryForFile(reflectionPath))
	{
		GOKNAR_INFO("ShaderEditorPanel: Failed to create editor reflection directory for '%s'.", reflectionPath.c_str());
		return;
	}

	tinyxml2::XMLDocument doc;
	tinyxml2::XMLElement* root = doc.NewElement("GameAsset");
	root->SetAttribute("FileType", GetCurrentEditorReflectionFileType());
	doc.InsertFirstChild(root);

	tinyxml2::XMLElement* canvasElement = doc.NewElement("Canvas");
	canvasElement->SetAttribute("Scale", scale_);
	canvasElement->SetAttribute("ScrollX", scrolling_.x);
	canvasElement->SetAttribute("ScrollY", scrolling_.y);
	root->InsertEndChild(canvasElement);

	tinyxml2::XMLElement* nodesElement = doc.NewElement("Nodes");
	for (const ShaderNode& node : nodes_)
	{
		tinyxml2::XMLElement* nodeElement = doc.NewElement("Node");
		nodeElement->SetAttribute("Id", node.id);
		nodeElement->SetAttribute("Name", node.name.c_str());
		nodeElement->SetAttribute("Category", node.typeCategory.c_str());
		nodeElement->SetAttribute("PosX", node.pos.x);
		nodeElement->SetAttribute("PosY", node.pos.y);
		nodeElement->SetAttribute("SizeX", node.size.x);
		nodeElement->SetAttribute("SizeY", node.size.y);
		nodeElement->SetAttribute("IsUniform", node.isUniform ? 1 : 0);

		tinyxml2::XMLElement* stringDataElement = doc.NewElement("StringData");
		tinyxml2::XMLText* stringDataText = doc.NewText(node.stringData.c_str());
		stringDataText->SetCData(true);
		stringDataElement->InsertEndChild(stringDataText);
		nodeElement->InsertEndChild(stringDataElement);

		auto SerializePins = [&](const char* containerName, const std::vector<ShaderPin>& pins)
		{
			tinyxml2::XMLElement* pinsElement = doc.NewElement(containerName);
			for (const ShaderPin& pin : pins)
			{
				tinyxml2::XMLElement* pinElement = doc.NewElement("Pin");
				pinElement->SetAttribute("Id", pin.id);
				pinElement->SetAttribute("NodeId", pin.nodeId);
				pinElement->SetAttribute("Name", pin.name.c_str());
				pinElement->SetAttribute("Type", ShaderPinTypeToString(pin.type));
				pinElement->SetAttribute("Kind", ShaderPinKindToString(pin.kind));
				SerializePinDefaultValue(doc, pinElement, pin);
				pinsElement->InsertEndChild(pinElement);
			}
			nodeElement->InsertEndChild(pinsElement);
		};

		SerializePins("Inputs", node.inputs);
		SerializePins("Outputs", node.outputs);

		if (!node.arrayDefaultValues.empty())
		{
			tinyxml2::XMLElement* arrayDefaultsElement = doc.NewElement("ArrayDefaultValues");
			for (const ShaderValue& value : node.arrayDefaultValues)
			{
				SerializeShaderValue(doc, arrayDefaultsElement, "Value", value);
			}
			nodeElement->InsertEndChild(arrayDefaultsElement);
		}

		nodesElement->InsertEndChild(nodeElement);
	}
	root->InsertEndChild(nodesElement);

	tinyxml2::XMLElement* linksElement = doc.NewElement("Links");
	for (const ShaderLink& link : links_)
	{
		tinyxml2::XMLElement* linkElement = doc.NewElement("Link");
		linkElement->SetAttribute("Id", link.id);
		linkElement->SetAttribute("StartPinId", link.startPinId);
		linkElement->SetAttribute("EndPinId", link.endPinId);
		linksElement->InsertEndChild(linkElement);
	}
	root->InsertEndChild(linksElement);

	doc.SaveFile(reflectionPath.c_str());
}

bool ShaderEditorPanel::LoadEditorReflection(const std::string& assetPath)
{
	const std::string reflectionPath = EditorAssetPathUtils::ToEditorReflectionPath(assetPath);
	if (!std::filesystem::exists(reflectionPath))
	{
		return false;
	}

	tinyxml2::XMLDocument doc;
	if (doc.LoadFile(reflectionPath.c_str()) != tinyxml2::XML_SUCCESS)
	{
		return false;
	}

	tinyxml2::XMLElement* root = doc.FirstChildElement("GameAsset");
	const char* fileType = root ? root->Attribute("FileType") : nullptr;
	if (!root || !fileType || std::string(fileType) != GetCurrentEditorReflectionFileType())
	{
		return false;
	}

	nodes_.clear();
	links_.clear();
	selectedNodeIds_.clear();
	preDragSelection_.clear();
	selectedNodeId_ = -1;
	selectedLinkId_ = -1;
	isDraggingSelection_ = false;
	isBackgroundClicked_ = false;
	isDraggingLink_ = false;
	autoConnectStartPinId_ = -1;
	draggingStartPinId_ = -1;

	tinyxml2::XMLElement* canvasElement = root->FirstChildElement("Canvas");
	if (canvasElement)
	{
		scale_ = canvasElement->FloatAttribute("Scale", 1.0f);
		scrolling_.x = canvasElement->FloatAttribute("ScrollX", 0.0f);
		scrolling_.y = canvasElement->FloatAttribute("ScrollY", 0.0f);
	}

	int maxId = 0;
	tinyxml2::XMLElement* nodesElement = root->FirstChildElement("Nodes");
	for (tinyxml2::XMLElement* nodeElement = nodesElement ? nodesElement->FirstChildElement("Node") : nullptr;
		nodeElement != nullptr;
		nodeElement = nodeElement->NextSiblingElement("Node"))
	{
		ShaderNode node;
		node.id = nodeElement->IntAttribute("Id");
		node.name = nodeElement->Attribute("Name") ? nodeElement->Attribute("Name") : "";
		node.typeCategory = nodeElement->Attribute("Category") ? nodeElement->Attribute("Category") : "";
		node.pos = ImVec2(nodeElement->FloatAttribute("PosX"), nodeElement->FloatAttribute("PosY"));
		node.size = ImVec2(nodeElement->FloatAttribute("SizeX"), nodeElement->FloatAttribute("SizeY"));
		node.isUniform = nodeElement->BoolAttribute("IsUniform", true);

		tinyxml2::XMLElement* stringDataElement = nodeElement->FirstChildElement("StringData");
		node.stringData = stringDataElement && stringDataElement->GetText() ? stringDataElement->GetText() : "";

		auto DeserializePins = [&](const char* containerName, std::vector<ShaderPin>& pins)
		{
			tinyxml2::XMLElement* pinsElement = nodeElement->FirstChildElement(containerName);
			for (tinyxml2::XMLElement* pinElement = pinsElement ? pinsElement->FirstChildElement("Pin") : nullptr;
				pinElement != nullptr;
				pinElement = pinElement->NextSiblingElement("Pin"))
			{
				ShaderPin pin{};
				pin.id = pinElement->IntAttribute("Id");
				pin.nodeId = pinElement->IntAttribute("NodeId", node.id);
				pin.name = pinElement->Attribute("Name") ? pinElement->Attribute("Name") : "";
				pin.type = StringToShaderPinType(pinElement->Attribute("Type") ? pinElement->Attribute("Type") : "");
				pin.kind = StringToShaderPinKind(pinElement->Attribute("Kind") ? pinElement->Attribute("Kind") : "Input");
				pin.defaultValue = DeserializePinDefaultValue(pinElement);
				pins.push_back(pin);
				maxId = std::max(maxId, pin.id);
			}
		};

		DeserializePins("Inputs", node.inputs);
		DeserializePins("Outputs", node.outputs);

		tinyxml2::XMLElement* arrayDefaultsElement = nodeElement->FirstChildElement("ArrayDefaultValues");
		for (tinyxml2::XMLElement* valueElement = arrayDefaultsElement ? arrayDefaultsElement->FirstChildElement("Value") : nullptr;
			valueElement != nullptr;
			valueElement = valueElement->NextSiblingElement("Value"))
		{
			node.arrayDefaultValues.push_back(DeserializeShaderValue(valueElement));
		}
		EnsureArrayDefaultsMatchNode(node);

		if (node.typeCategory == "Master" || node.name == "Material Output")
		{
			masterNodeId_ = node.id;
		}

		maxId = std::max(maxId, node.id);
		nodes_.push_back(node);
	}

	tinyxml2::XMLElement* linksElement = root->FirstChildElement("Links");
	for (tinyxml2::XMLElement* linkElement = linksElement ? linksElement->FirstChildElement("Link") : nullptr;
		linkElement != nullptr;
		linkElement = linkElement->NextSiblingElement("Link"))
	{
		ShaderLink link{};
		link.id = linkElement->IntAttribute("Id");
		link.startPinId = linkElement->IntAttribute("StartPinId");
		link.endPinId = linkElement->IntAttribute("EndPinId");
		maxId = std::max(maxId, link.id);
		links_.push_back(link);
	}

	nextId_ = maxId + 1;

	std::unordered_map<int, int> migratedPinIds;
	std::vector<ShaderNode> migratedAccessorNodes;
	for (const ShaderNode& node : nodes_)
	{
		if (!IsMaterialVariableDeclarationCategory(node.typeCategory))
		{
			continue;
		}

		bool hasConnectedPins = false;
		for (const ShaderLink& link : links_)
		{
			const auto pinMatches = [&link](const ShaderPin& pin)
			{
				return link.startPinId == pin.id || link.endPinId == pin.id;
			};

			if (std::any_of(node.inputs.begin(), node.inputs.end(), pinMatches) ||
				std::any_of(node.outputs.begin(), node.outputs.end(), pinMatches))
			{
				hasConnectedPins = true;
				break;
			}
		}

		if (!hasConnectedPins)
		{
			continue;
		}

		ShaderNode accessorNode = CreateMaterialVariableAccessorNode(node, false, node.pos);
		if (node.typeCategory == "MaterialVariable" && !node.outputs.empty() && !accessorNode.outputs.empty())
		{
			migratedPinIds[node.outputs[0].id] = accessorNode.outputs[0].id;
		}
		else if (node.typeCategory == "MaterialVariableArray" && !node.inputs.empty() && accessorNode.inputs.size() >= 1 && node.outputs.size() >= 2 && accessorNode.outputs.size() >= 2)
		{
			migratedPinIds[node.inputs[0].id] = accessorNode.inputs[0].id;
			migratedPinIds[node.outputs[0].id] = accessorNode.outputs[0].id;
			migratedPinIds[node.outputs[1].id] = accessorNode.outputs[1].id;
		}

		migratedAccessorNodes.push_back(accessorNode);
	}

	for (ShaderLink& link : links_)
	{
		if (migratedPinIds.find(link.startPinId) != migratedPinIds.end())
		{
			link.startPinId = migratedPinIds[link.startPinId];
		}

		if (migratedPinIds.find(link.endPinId) != migratedPinIds.end())
		{
			link.endPinId = migratedPinIds[link.endPinId];
		}
	}

	nodes_.insert(nodes_.end(), migratedAccessorNodes.begin(), migratedAccessorNodes.end());
	SynchronizeMaterialVariableAccessorNodes();
	return FindNode(masterNodeId_) != nullptr;
}

void ShaderEditorPanel::ApplyHierarchicalLayout()
{
	if (nodes_.empty())
	{
		return;
	}

	std::unordered_map<int, int> depthByNode;
	std::vector<int> queue;
	depthByNode[masterNodeId_] = 0;
	queue.push_back(masterNodeId_);

	for (size_t queueIndex = 0; queueIndex < queue.size(); ++queueIndex)
	{
		const int currentNodeId = queue[queueIndex];
		for (const ShaderLink& link : links_)
		{
			ShaderPin* endPin = FindPin(link.endPinId);
			ShaderPin* startPin = FindPin(link.startPinId);
			if (!endPin || !startPin || endPin->nodeId != currentNodeId)
			{
				continue;
			}

			if (depthByNode.find(startPin->nodeId) == depthByNode.end())
			{
				depthByNode[startPin->nodeId] = depthByNode[currentNodeId] + 1;
				queue.push_back(startPin->nodeId);
			}
		}
	}

	int maxDepth = 0;
	for (const auto& [nodeId, depth] : depthByNode)
	{
		maxDepth = std::max(maxDepth, depth);
	}

	std::unordered_map<int, int> rowByDepth;
	const float baseX = 120.0f;
	const float baseY = 80.0f;
	const float columnSpacing = 280.0f;
	const float rowSpacing = 180.0f;

	for (ShaderNode& node : nodes_)
	{
		auto depthIterator = depthByNode.find(node.id);
		if (depthIterator == depthByNode.end())
		{
			continue;
		}

		const int depth = depthIterator->second;
		const int row = rowByDepth[depth]++;
		node.pos.x = baseX + (maxDepth - depth) * columnSpacing;
		node.pos.y = baseY + row * rowSpacing;
	}

	const float disconnectedStartY = baseY + static_cast<float>(std::max(static_cast<int>(depthByNode.size()), 1)) * rowSpacing;
	int disconnectedRow = 0;
	for (ShaderNode& node : nodes_)
	{
		if (depthByNode.find(node.id) != depthByNode.end())
		{
			continue;
		}

		node.pos.x = baseX;
		node.pos.y = disconnectedStartY + disconnectedRow * rowSpacing;
		++disconnectedRow;
	}
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

void ShaderEditorPanel::DrawMaterialProperties()
{
	const char* materialValueTypeLabels[] = { "Float", "Vector2", "Vector3", "Vector4" };
	auto GetMaterialValueTypeIndex = [](ShaderPinType type)
	{
		switch (type)
		{
		case ShaderPinType::Vector2: return 1;
		case ShaderPinType::Vector3: return 2;
		case ShaderPinType::Vector4: return 3;
		case ShaderPinType::Float:
		default:
			return 0;
		}
	};
	auto GetMaterialValueTypeFromIndex = [](int index)
	{
		switch (index)
		{
		case 1: return ShaderPinType::Vector2;
		case 2: return ShaderPinType::Vector3;
		case 3: return ShaderPinType::Vector4;
		case 0:
		default:
			return ShaderPinType::Float;
		}
	};

	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Material Settings");
	ImGui::Separator();

	const char* blendModels[] = { "Opaque", "Masked", "Transparent" };
	int currentBlendModel = (int)blendModel_;
	if (ImGui::Combo("Blend Model", &currentBlendModel, blendModels, IM_ARRAYSIZE(blendModels)))
	{
		blendModel_ = (MaterialBlendModel)currentBlendModel;
	}

	const char* shadingModels[] = { "Default", "TwoSided" };
	int currentShadingModel = (int)shadingModel_;
	if (ImGui::Combo("Shading Model", &currentShadingModel, shadingModels, IM_ARRAYSIZE(shadingModels)))
	{
		shadingModel_ = (MaterialShadingModel)currentShadingModel;
	}

	ImGui::DragFloat("Translucency", &translucency_, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Phong Exponent", &phongExponent_, 1.0f, 1.0f, 128.0f);

	ImGui::ColorEdit3("Ambient Reflectance", &ambientReflectance_.x);
	ImGui::ColorEdit3("Specular Reflectance", &specularReflectance_.x);

	ImGui::Separator();
	ImGui::Text("Textures");
	if (ImGui::Button("Add Texture"))
	{
		textures_.push_back({ "diffuseTexture", "Textures/T_Default.png" });
	}

	for (size_t i = 0; i < textures_.size(); ++i)
	{
		ImGui::PushID((int)i);

		char nameBuf[256];
		strncpy(nameBuf, textures_[i].name.c_str(), sizeof(nameBuf));
		nameBuf[sizeof(nameBuf) - 1] = 0;
		if (ImGui::InputText("Sampler Name", nameBuf, sizeof(nameBuf)))
		{
			textures_[i].name = nameBuf;
		}

		char pathBuf[256];
		strncpy(pathBuf, textures_[i].path.c_str(), sizeof(pathBuf));
		pathBuf[sizeof(pathBuf) - 1] = 0;
		if (ImGui::InputText("Path", pathBuf, sizeof(pathBuf)))
		{
			textures_[i].path = pathBuf;
		}

		if (ImGui::Button("Remove"))
		{
			textures_.erase(textures_.begin() + i);
			--i;
		}
		ImGui::Separator();
		ImGui::PopID();
	}

	ImGui::Separator();
	ImGui::Text("Material Variables");
	if (ImGui::Button("Add Variable"))
	{
		nodes_.push_back(SpawnNode("MaterialVariable", "materialVariable" + std::to_string(nextId_), ImVec2(80.0f, 120.0f + (float)nodes_.size() * 30.0f)));
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Array Variable"))
	{
		nodes_.push_back(SpawnNode("MaterialVariableArray", "materialArray" + std::to_string(nextId_), ImVec2(80.0f, 120.0f + (float)nodes_.size() * 30.0f)));
	}
	ImGui::TextDisabled("Use the canvas right-click menu under Variables to place getter/setter nodes.");

	for (size_t nodeIndex = 0; nodeIndex < nodes_.size(); ++nodeIndex)
	{
		ShaderNode& node = nodes_[nodeIndex];
		if (!IsMaterialVariableDeclarationCategory(node.typeCategory))
		{
			continue;
		}

		ImGui::PushID(node.id);
		ImGui::Separator();

		const std::string previousName = SanitizeIdentifier(node.name);
		char nameBuffer[256];
		strncpy(nameBuffer, node.name.c_str(), sizeof(nameBuffer));
		nameBuffer[sizeof(nameBuffer) - 1] = 0;
		if (ImGui::InputText(node.typeCategory == "MaterialVariableArray" ? "Array Name" : "Variable Name", nameBuffer, sizeof(nameBuffer)))
		{
			node.name = SanitizeIdentifier(nameBuffer);
			RenameMaterialVariableAccessorReferences(previousName, node.name);
		}

		if (!node.outputs.empty())
		{
			int typeIndex = GetMaterialValueTypeIndex(node.outputs[0].type);
			if (ImGui::Combo(node.typeCategory == "MaterialVariableArray" ? "Element Type" : "Value Type", &typeIndex, materialValueTypeLabels, IM_ARRAYSIZE(materialValueTypeLabels)))
			{
				node.outputs[0].type = GetMaterialValueTypeFromIndex(typeIndex);
				if (node.typeCategory == "MaterialVariableArray")
				{
					node.outputs[0].defaultValue = GetDefaultValueForPinType(node.outputs[0].type);
					EnsureArrayDefaultsMatchNode(node);
				}
				else
				{
					node.outputs[0].defaultValue = GetDefaultValueForPinType(node.outputs[0].type);
				}

				SynchronizeMaterialVariableAccessorNodes();
			}
		}

		int storageIndex = node.isUniform ? 0 : 1;
		const char* storageLabels[] = { "Uniform", "Global" };
		if (ImGui::Combo("Storage", &storageIndex, storageLabels, IM_ARRAYSIZE(storageLabels)))
		{
			node.isUniform = storageIndex == 0;
			SynchronizeMaterialVariableAccessorNodes();
		}

		if (node.typeCategory == "MaterialVariable")
		{
			if (DrawShaderValueEditor("Default Value", node.outputs[0].type, node.outputs[0].defaultValue))
			{
				SynchronizeMaterialVariableAccessorNodes();
			}
		}
		else
		{
			int arraySize = (int)node.arrayDefaultValues.size();
			if (ImGui::InputInt("Array Size", &arraySize))
			{
				arraySize = std::clamp(arraySize, 1, kMaxMaterialArraySize);
				node.arrayDefaultValues.resize(arraySize, GetDefaultValueForPinType(node.outputs[0].type));
				EnsureArrayDefaultsMatchNode(node);
				SynchronizeMaterialVariableAccessorNodes();
			}

			for (size_t valueIndex = 0; valueIndex < node.arrayDefaultValues.size(); ++valueIndex)
			{
				ImGui::PushID((int)valueIndex);
				if (DrawShaderValueEditor(("Default " + std::to_string(valueIndex)).c_str(), node.outputs[0].type, node.arrayDefaultValues[valueIndex]))
				{
					SynchronizeMaterialVariableAccessorNodes();
				}
				ImGui::PopID();
			}
		}
		if (ImGui::Button("Remove Variable"))
		{
			const std::string referenceName = SanitizeIdentifier(node.name);
			std::unordered_set<int> nodeIdsToRemove;
			for (const ShaderNode& candidateNode : nodes_)
			{
				if (candidateNode.id == node.id)
				{
					nodeIdsToRemove.insert(candidateNode.id);
					continue;
				}

				if (IsMaterialVariableAccessorCategory(candidateNode.typeCategory) &&
					GetMaterialVariableReferenceName(candidateNode) == referenceName)
				{
					nodeIdsToRemove.insert(candidateNode.id);
				}
			}

			std::vector<int> pinIds;
			for (const ShaderNode& candidateNode : nodes_)
			{
				if (nodeIdsToRemove.find(candidateNode.id) == nodeIdsToRemove.end())
				{
					continue;
				}

				for (const ShaderPin& pin : candidateNode.inputs)
				{
					pinIds.push_back(pin.id);
				}
				for (const ShaderPin& pin : candidateNode.outputs)
				{
					pinIds.push_back(pin.id);
				}
			}

			links_.erase(std::remove_if(links_.begin(), links_.end(), [&pinIds](const ShaderLink& link)
				{
					return
						std::find(pinIds.begin(), pinIds.end(), link.startPinId) != pinIds.end() ||
						std::find(pinIds.begin(), pinIds.end(), link.endPinId) != pinIds.end();
				}), links_.end());
			for (int nodeIdToRemove : nodeIdsToRemove)
			{
				selectedNodeIds_.erase(nodeIdToRemove);
				if (selectedNodeId_ == nodeIdToRemove)
				{
					selectedNodeId_ = -1;
				}
			}

			nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
				[&nodeIdsToRemove](const ShaderNode& candidateNode)
				{
					return nodeIdsToRemove.find(candidateNode.id) != nodeIdsToRemove.end();
				}), nodes_.end());
			ImGui::PopID();
			break;
		}

		ImGui::PopID();
	}
}

void ShaderEditorPanel::DrawMaterialFunctionProperties()
{
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Material Function");
	ImGui::Separator();

	ImGui::TextWrapped("Edit the function signature by selecting the output node or any Function Input node.");
	if (!currentAssetPath_.empty())
	{
		ImGui::Spacing();
		ImGui::TextWrapped("Asset: %s", currentAssetPath_.c_str());
	}

	size_t inputCount = 0;
	for (const ShaderNode& node : nodes_)
	{
		if (node.typeCategory == "FunctionInput")
		{
			++inputCount;
		}
	}

	ImGui::Spacing();
	ImGui::Text("Inputs: %zu", inputCount);

	if (ShaderNode* masterNode = FindNode(masterNodeId_))
	{
		ImGui::Text("Outputs: %zu", masterNode->inputs.size());
	}
}

void ShaderEditorPanel::DrawNodeCanvas()
{
	ImGui::BeginChild("shader_scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
	ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
	ImVec2 offset = ImVec2(canvas_p0.x + scrolling_.x, canvas_p0.y + scrolling_.y);

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

	draw_list->ChannelsSetCurrent(0);
	for (auto& link : links_)
	{
		ShaderPin* startPin = FindPin(link.startPinId);
		ShaderPin* endPin = FindPin(link.endPinId);
		ShaderNode* startNode = startPin ? FindNode(startPin->nodeId) : nullptr;
		ShaderNode* endNode = endPin ? FindNode(endPin->nodeId) : nullptr;
		if ((startNode && IsHiddenCanvasNodeCategory(startNode->typeCategory)) ||
			(endNode && IsHiddenCanvasNodeCategory(endNode->typeCategory)))
		{
			continue;
		}

		ImVec2 startLocal = GetPinPosition(link.startPinId);
		ImVec2 endLocal = GetPinPosition(link.endPinId);

		ImVec2 startPos = ImVec2(offset.x + startLocal.x * scale_, offset.y + startLocal.y * scale_);
		ImVec2 endPos = ImVec2(offset.x + endLocal.x * scale_, offset.y + endLocal.y * scale_);

		ImVec2 cp1 = ImVec2(startPos.x + 50.0f * scale_, startPos.y);
		ImVec2 cp2 = ImVec2(endPos.x - 50.0f * scale_, endPos.y);

		ImU32 color = (selectedLinkId_ == link.id) ? IM_COL32(255, 200, 0, 255) : IM_COL32(200, 200, 200, 255);
		draw_list->AddBezierCubic(startPos, cp1, cp2, endPos, color, 3.0f * scale_);
	}

	draw_list->ChannelsSetCurrent(1);
	for (auto& node : nodes_)
	{
		if (IsHiddenCanvasNodeCategory(node.typeCategory))
		{
			continue;
		}

		ImGui::PushID(node.id);
		ImVec2 rect_min = ImVec2(offset.x + node.pos.x * scale_, offset.y + node.pos.y * scale_);
		ImVec2 scaled_size = ImVec2(node.size.x * scale_, node.size.y * scale_);
		ImVec2 rect_max = ImVec2(rect_min.x + scaled_size.x, rect_min.y + scaled_size.y);

		bool isSelected = selectedNodeIds_.find(node.id) != selectedNodeIds_.end();
		ImU32 bg_col = isSelected ? IM_COL32(75, 75, 100, 255) : IM_COL32(40, 40, 40, 255);
		if (node.typeCategory == "Master") bg_col = isSelected ? IM_COL32(60, 100, 60, 255) : IM_COL32(40, 80, 40, 255);
		else if (node.typeCategory == "Custom") bg_col = isSelected ? IM_COL32(100, 60, 100, 255) : IM_COL32(80, 40, 80, 255);
		else if (node.typeCategory == "Functions") bg_col = isSelected ? IM_COL32(70, 95, 130, 255) : IM_COL32(50, 70, 105, 255);
		else if (node.typeCategory == "FunctionInput") bg_col = isSelected ? IM_COL32(110, 85, 40, 255) : IM_COL32(85, 60, 30, 255);
		else if (node.typeCategory == "MaterialVariable") bg_col = isSelected ? IM_COL32(45, 110, 110, 255) : IM_COL32(30, 82, 82, 255);
		else if (node.typeCategory == "MaterialVariableArray") bg_col = isSelected ? IM_COL32(45, 90, 120, 255) : IM_COL32(30, 65, 90, 255);
		else if (node.typeCategory == "MaterialVariableGet" || node.typeCategory == "MaterialVariableSet") bg_col = isSelected ? IM_COL32(55, 125, 125, 255) : IM_COL32(35, 92, 92, 255);
		else if (node.typeCategory == "MaterialVariableArrayGet" || node.typeCategory == "MaterialVariableArraySet") bg_col = isSelected ? IM_COL32(55, 105, 135, 255) : IM_COL32(35, 75, 102, 255);
		else if (node.typeCategory == "Flow") bg_col = isSelected ? IM_COL32(120, 80, 45, 255) : IM_COL32(90, 58, 30, 255);

		draw_list->AddRectFilled(rect_min, rect_max, bg_col, 4.0f * scale_);
		draw_list->AddRectFilled(rect_min, ImVec2(rect_max.x, rect_min.y + 25.0f * scale_), IM_COL32(20, 20, 20, 255), 4.0f * scale_);
		draw_list->AddRect(rect_min, rect_max, IM_COL32(100, 100, 100, 255), 4.0f * scale_);

		ImGui::SetCursorScreenPos(ImVec2(rect_min.x + 8.0f * scale_, rect_min.y + 6.0f * scale_));
		ImGui::SetWindowFontScale(scale_);
		ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "%s", node.name.c_str());
		ImGui::SetWindowFontScale(1.0f);

		ImGui::SetCursorScreenPos(rect_min);
		ImGui::SetNextItemAllowOverlap();
		ImGui::InvisibleButton((std::string("node_body_") + std::to_string(node.id)).c_str(), scaled_size);

		if (ImGui::IsItemClicked(0))
		{
			if (ImGui::GetIO().KeyCtrl)
			{
				if (isSelected) selectedNodeIds_.erase(node.id);
				else selectedNodeIds_.insert(node.id);
			}
			else
			{
				if (!isSelected) {
					selectedNodeIds_.clear();
					selectedNodeIds_.insert(node.id);
				}
			}
		}

		if (node.typeCategory == "Functions" &&
			hud_->WasLastItemDoubleClicked(ImGuiMouseButton_Left) &&
			!node.stringData.empty())
		{
			OpenMaterialFunctionFromNode(node.stringData);
			ImGui::PopID();
			break;
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

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
	{
		if (!ImGui::GetIO().KeyCtrl)
		{
			selectedNodeIds_.clear();
		}
		selectedLinkId_ = -1;

		isBackgroundClicked_ = true;
		selectionStart_ = ImVec2((ImGui::GetMousePos().x - offset.x) / scale_,
			(ImGui::GetMousePos().y - offset.y) / scale_);
		preDragSelection_ = selectedNodeIds_;
	}

	if (isBackgroundClicked_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 5.0f))
	{
		isDraggingSelection_ = true;
		isBackgroundClicked_ = false;
	}

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		isBackgroundClicked_ = false;
	}

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
			if (IsHiddenCanvasNodeCategory(node.typeCategory))
			{
				continue;
			}

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

	if (isDraggingLink_)
	{
		ImVec2 startLocal = GetPinPosition(draggingStartPinId_);
		ImVec2 startPos = ImVec2(offset.x + startLocal.x * scale_, offset.y + startLocal.y * scale_);
		ImVec2 endPos = ImGui::GetMousePos();
		draw_list->AddBezierCubic(startPos, ImVec2(startPos.x + 50.0f * scale_, startPos.y), ImVec2(endPos.x - 50.0f * scale_, endPos.y), endPos, IM_COL32(255, 255, 255, 200), 3.0f * scale_);

		if (ImGui::IsMouseReleased(0))
		{
			if (!ImGui::IsAnyItemHovered())
			{
				ImGui::OpenPopup("CanvasMenu");
				autoConnectStartPinId_ = draggingStartPinId_;
			}
			isDraggingLink_ = false;
		}
	}

	if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
	{
		scrolling_.x += ImGui::GetIO().MouseDelta.x;
		scrolling_.y += ImGui::GetIO().MouseDelta.y;
	}

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered())
	{
		ImGui::OpenPopup("CanvasMenu");
	}

	size_t nodeCountBefore = nodes_.size();

	if (ImGui::BeginPopup("CanvasMenu"))
	{
		ImVec2 openPos = ImGui::GetMousePosOnOpeningCurrentPopup();
		ImVec2 spawnPos = ImVec2((openPos.x - offset.x) / scale_, (openPos.y - offset.y) / scale_);

		if (ImGui::BeginMenu("Variables"))
		{
			if (ImGui::BeginMenu("Engine"))
			{
				if (ImGui::BeginMenu("Positioning"))
				{
					for (const char* var : { "boneTransformationMatrix", "worldTransformationMatrix", "relativeTransformationMatrix", "modelMatrix", "viewProjectionMatrix", "transformationMatrix", "viewPosition" })
						if (ImGui::MenuItem(var)) nodes_.push_back(SpawnNode("Variables", var, spawnPos));
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Vertex Inputs"))
				{
					for (const char* var : {
						SHADER_VARIABLE_NAMES::VERTEX::POSITION,
						SHADER_VARIABLE_NAMES::VERTEX::NORMAL,
						SHADER_VARIABLE_NAMES::VERTEX::COLOR,
						SHADER_VARIABLE_NAMES::VERTEX::UV,
						SHADER_VARIABLE_NAMES::VERTEX::BONE_IDS,
						SHADER_VARIABLE_NAMES::VERTEX::WEIGHTS,
						SHADER_VARIABLE_NAMES::VERTEX::MODIFIED_POSITION
						})
					{
						if (ImGui::MenuItem(var)) nodes_.push_back(SpawnNode("Variables", var, spawnPos));
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Vertex Shader Outs"))
				{
					for (const char* var : { "finalModelMatrix", "fragmentPositionWorldSpace", "fragmentPositionScreenSpace", "vertexNormal", "vertexColor", "outBoneIDs", "outWeights", "directionalLightSpaceFragmentPositions", "spotLightSpaceFragmentPositions" })
						if (ImGui::MenuItem(var)) nodes_.push_back(SpawnNode("Variables", var, spawnPos));
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Timing & UVs"))
				{
					for (const char* var : { "deltaTime", "elapsedTime", "textureUV" })
						if (ImGui::MenuItem(var)) nodes_.push_back(SpawnNode("Variables", var, spawnPos));
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}

			if (IsEditingMaterial() && ImGui::BeginMenu("Material"))
			{
				if (ImGui::BeginMenu("Variables"))
				{
					bool hasVariables = false;
					for (const ShaderNode& materialNode : nodes_)
					{
						if (materialNode.typeCategory != "MaterialVariable")
						{
							continue;
						}

						hasVariables = true;
						if (ImGui::BeginMenu(materialNode.name.c_str()))
						{
							if (ImGui::MenuItem("Getter"))
							{
								nodes_.push_back(CreateMaterialVariableAccessorNode(materialNode, false, spawnPos));
							}

							if (materialNode.isUniform)
							{
								ImGui::MenuItem("Setter", nullptr, false, false);
							}
							else if (ImGui::MenuItem("Setter"))
							{
								nodes_.push_back(CreateMaterialVariableAccessorNode(materialNode, true, spawnPos));
							}

							ImGui::EndMenu();
						}
					}

					if (!hasVariables)
					{
						ImGui::MenuItem("No Variables Defined", nullptr, false, false);
					}

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Arrays"))
				{
					bool hasArrays = false;
					for (const ShaderNode& materialNode : nodes_)
					{
						if (materialNode.typeCategory != "MaterialVariableArray")
						{
							continue;
						}

						hasArrays = true;
						if (ImGui::BeginMenu(materialNode.name.c_str()))
						{
							if (ImGui::MenuItem("Getter"))
							{
								nodes_.push_back(CreateMaterialVariableAccessorNode(materialNode, false, spawnPos));
							}

							if (materialNode.isUniform)
							{
								ImGui::MenuItem("Setter", nullptr, false, false);
							}
							else if (ImGui::MenuItem("Setter"))
							{
								nodes_.push_back(CreateMaterialVariableAccessorNode(materialNode, true, spawnPos));
							}

							ImGui::EndMenu();
						}
					}

					if (!hasArrays)
					{
						ImGui::MenuItem("No Arrays Defined", nullptr, false, false);
					}

					ImGui::EndMenu();
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Math"))
		{
			if (ImGui::BeginMenu("Basic Math")) {
				for (const char* func : { "Add", "Subtract", "Multiply", "Divide", "Modulo", "Abs", "Sign", "Min", "Max", "Fma" })
					if (ImGui::MenuItem(func)) nodes_.push_back(SpawnNode("Math", func, spawnPos));
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Rounding")) {
				for (const char* func : { "Floor", "Trunc", "Round", "RoundEven", "Ceil", "Fract", "Modf" })
					if (ImGui::MenuItem(func)) nodes_.push_back(SpawnNode("Math", func, spawnPos));
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Interpolation")) {
				for (const char* func : { "Clamp", "Mix", "Step", "Smoothstep" })
					if (ImGui::MenuItem(func)) nodes_.push_back(SpawnNode("Math", func, spawnPos));
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Utility")) {
				for (const char* func : { "Mask", "IsNan", "IsInf", "FloatBitsToInt", "FloatBitsToUint", "IntBitsToFloat", "UintBitsToFloat", "Frexp", "Ldexp" })
					if (ImGui::MenuItem(func)) nodes_.push_back(SpawnNode("Math", func, spawnPos));
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Trigonometry"))
		{
			for (const char* func : { "Radians", "Degrees", "Sine", "Cosine", "Tangent", "Asin", "Acos", "Atan", "Sinh", "Cosh", "Tanh", "Asinh", "Acosh", "Atanh" })
				if (ImGui::MenuItem(func)) nodes_.push_back(SpawnNode("Trigonometry", func, spawnPos));
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Exponential"))
		{
			for (const char* func : { "Pow", "Exp", "Log", "Exp2", "Log2", "Sqrt", "InverseSqrt" })
				if (ImGui::MenuItem(func)) nodes_.push_back(SpawnNode("Exponential", func, spawnPos));
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Geometric"))
		{
			for (const char* func : { "Length", "Distance", "Dot", "Cross", "Normalize", "Faceforward", "Reflect", "Refract" })
				if (ImGui::MenuItem(func)) nodes_.push_back(SpawnNode("Geometric", func, spawnPos));
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Matrix"))
		{
			for (const char* func : { "MatrixCompMult", "OuterProduct", "Transpose", "Determinant", "Inverse" })
				if (ImGui::MenuItem(func)) nodes_.push_back(SpawnNode("Matrix", func, spawnPos));
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Texture"))
		{
			if (ImGui::MenuItem("Texture Sample")) nodes_.push_back(SpawnNode("Texture", "Texture Sample", spawnPos));
			ImGui::EndMenu();
		}

		if (IsEditingMaterialFunction() && ImGui::BeginMenu("Function"))
		{
			if (ImGui::MenuItem("Function Input"))
			{
				nodes_.push_back(SpawnNode("FunctionInput", "Function Input", spawnPos));
			}
			if (ImGui::MenuItem("Function Output"))
			{
				if (ShaderNode* masterNode = FindNode(masterNodeId_))
				{
					masterNode->inputs.push_back({
						nextId_++,
						masterNode->id,
						"Result" + std::to_string(masterNode->inputs.size() + 1),
						ShaderPinType::Float,
						ShaderPinKind::Input,
						0.0f
						});
					masterNode->size.y = GetNodeBodyHeightFromPins(masterNode->inputs.size(), masterNode->outputs.size());
				}
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Material Functions"))
		{
			for (const std::string& materialFunctionPath : GetMaterialFunctionAssetPaths())
			{
				if (ImGui::MenuItem(materialFunctionPath.c_str()))
				{
					nodes_.push_back(CreateMaterialFunctionCallNode(materialFunctionPath, spawnPos));
				}
			}
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

		if (ImGui::BeginMenu("Custom"))
		{
			if (ImGui::MenuItem("Custom GLSL")) nodes_.push_back(SpawnNode("Custom", "Custom GLSL", spawnPos));
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Flow"))
		{
			if (ImGui::MenuItem("If"))
			{
				nodes_.push_back(SpawnNode("Flow", "If", spawnPos));
			}
			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}
	else
	{
		autoConnectStartPinId_ = -1;
	}

	if (nodes_.size() > nodeCountBefore && autoConnectStartPinId_ != -1)
	{
		ShaderNode& newNode = nodes_.back();
		if (!newNode.inputs.empty())
		{
			links_.push_back({ nextId_++, autoConnectStartPinId_, newNode.inputs[0].id });
		}
		autoConnectStartPinId_ = -1;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
	{
		bool ctrlPressed = ImGui::GetIO().KeyCtrl;

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

	const char* materialValueTypeLabels[] = { "Float", "Vector2", "Vector3", "Vector4" };
	auto GetMaterialValueTypeIndex = [](ShaderPinType type)
	{
		switch (type)
		{
		case ShaderPinType::Vector2: return 1;
		case ShaderPinType::Vector3: return 2;
		case ShaderPinType::Vector4: return 3;
		case ShaderPinType::Float:
		default:
			return 0;
		}
	};
	auto GetMaterialValueTypeFromIndex = [](int index)
	{
		switch (index)
		{
		case 1: return ShaderPinType::Vector2;
		case 2: return ShaderPinType::Vector3;
		case 3: return ShaderPinType::Vector4;
		case 0:
		default:
			return ShaderPinType::Float;
		}
	};

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

			if (node->typeCategory == "Custom")
			{
				ImGui::Separator();
				ImGui::Text("GLSL Code");
				ImGui::TextDisabled("(Use RETURN_RESULT: for output)");
				char buf[8192];
				strncpy(buf, node->stringData.c_str(), sizeof(buf));
				buf[sizeof(buf) - 1] = 0;
				if (ImGui::InputTextMultiline("##CustomGLSL", buf, sizeof(buf), ImVec2(-1.0f, 300.0f)))
				{
					node->stringData = buf;
				}
			}

			if (node->typeCategory == "MaterialVariable" && !node->outputs.empty())
			{
				ImGui::Separator();
				ImGui::Text("Material Variable");

				const std::string previousName = SanitizeIdentifier(node->name);
				char nameBuffer[256];
				strncpy(nameBuffer, node->name.c_str(), sizeof(nameBuffer));
				nameBuffer[sizeof(nameBuffer) - 1] = 0;
				if (ImGui::InputText("Variable Name", nameBuffer, sizeof(nameBuffer)))
				{
					node->name = SanitizeIdentifier(nameBuffer);
					RenameMaterialVariableAccessorReferences(previousName, node->name);
				}

				int typeIndex = GetMaterialValueTypeIndex(node->outputs[0].type);
				if (ImGui::Combo("Value Type", &typeIndex, materialValueTypeLabels, IM_ARRAYSIZE(materialValueTypeLabels)))
				{
					node->outputs[0].type = GetMaterialValueTypeFromIndex(typeIndex);
					node->outputs[0].defaultValue = GetDefaultValueForPinType(node->outputs[0].type);
					SynchronizeMaterialVariableAccessorNodes();
				}

				int storageIndex = node->isUniform ? 0 : 1;
				const char* storageLabels[] = { "Uniform", "Global" };
				if (ImGui::Combo("Storage", &storageIndex, storageLabels, IM_ARRAYSIZE(storageLabels)))
				{
					node->isUniform = storageIndex == 0;
					SynchronizeMaterialVariableAccessorNodes();
				}

				if (DrawShaderValueEditor("Default Value", node->outputs[0].type, node->outputs[0].defaultValue))
				{
					SynchronizeMaterialVariableAccessorNodes();
				}
			}

			if (node->typeCategory == "MaterialVariableArray" && !node->outputs.empty())
			{
				ImGui::Separator();
				ImGui::Text("Material Variable Array");

				const std::string previousName = SanitizeIdentifier(node->name);
				char nameBuffer[256];
				strncpy(nameBuffer, node->name.c_str(), sizeof(nameBuffer));
				nameBuffer[sizeof(nameBuffer) - 1] = 0;
				if (ImGui::InputText("Array Name", nameBuffer, sizeof(nameBuffer)))
				{
					node->name = SanitizeIdentifier(nameBuffer);
					RenameMaterialVariableAccessorReferences(previousName, node->name);
				}

				int typeIndex = GetMaterialValueTypeIndex(node->outputs[0].type);
				if (ImGui::Combo("Element Type", &typeIndex, materialValueTypeLabels, IM_ARRAYSIZE(materialValueTypeLabels)))
				{
					node->outputs[0].type = GetMaterialValueTypeFromIndex(typeIndex);
					node->outputs[0].defaultValue = GetDefaultValueForPinType(node->outputs[0].type);
					EnsureArrayDefaultsMatchNode(*node);
					SynchronizeMaterialVariableAccessorNodes();
				}

				int storageIndex = node->isUniform ? 0 : 1;
				const char* storageLabels[] = { "Uniform", "Global" };
				if (ImGui::Combo("Storage", &storageIndex, storageLabels, IM_ARRAYSIZE(storageLabels)))
				{
					node->isUniform = storageIndex == 0;
					SynchronizeMaterialVariableAccessorNodes();
				}

				int arraySize = (int)node->arrayDefaultValues.size();
				if (ImGui::InputInt("Array Size", &arraySize))
				{
					arraySize = std::clamp(arraySize, 1, kMaxMaterialArraySize);
					node->arrayDefaultValues.resize(arraySize, GetDefaultValueForPinType(node->outputs[0].type));
					EnsureArrayDefaultsMatchNode(*node);
					SynchronizeMaterialVariableAccessorNodes();
				}

				for (size_t valueIndex = 0; valueIndex < node->arrayDefaultValues.size(); ++valueIndex)
				{
					ImGui::PushID((int)valueIndex);
					if (DrawShaderValueEditor(("Default " + std::to_string(valueIndex)).c_str(), node->outputs[0].type, node->arrayDefaultValues[valueIndex]))
					{
						SynchronizeMaterialVariableAccessorNodes();
					}
					ImGui::PopID();
				}
			}

			if (IsMaterialVariableAccessorCategory(node->typeCategory))
			{
				ImGui::Separator();
				ImGui::Text("%s", IsMaterialVariableSetterCategory(node->typeCategory) ? "Variable Setter" : "Variable Getter");
				ImGui::TextWrapped("Reference: %s", GetMaterialVariableReferenceName(*node).c_str());
				if (FindMaterialVariableDeclarationNode(nodes_, GetMaterialVariableReferenceName(*node)) == nullptr)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "Referenced material variable definition is missing.");
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

			if (node->typeCategory == "Functions")
			{
				ImGui::Separator();
				ImGui::Text("Material Function");

				char pathBuffer[512];
				strncpy(pathBuffer, node->stringData.c_str(), sizeof(pathBuffer));
				pathBuffer[sizeof(pathBuffer) - 1] = 0;
				if (ImGui::InputText("Asset Path", pathBuffer, sizeof(pathBuffer)))
				{
					node->stringData = pathBuffer;
				}

				if (ImGui::Button("Reload Signature"))
				{
					RefreshMaterialFunctionCallNodeSignature(*node);
				}

				if (!node->stringData.empty())
				{
					ImGui::SameLine();
					if (ImGui::Button("Open Function"))
					{
						OpenMaterialFunctionFromNode(node->stringData);
					}
				}
			}

			if (node->typeCategory == "Flow" && node->name == "If Check")
			{
				ImGui::Separator();
				ImGui::Text("If Check");

				const char* operators[] = { ">", ">=", "<", "<=", "==", "!=" };
				int operatorIndex = 0;
				for (int index = 0; index < IM_ARRAYSIZE(operators); ++index)
				{
					if (node->stringData == operators[index])
					{
						operatorIndex = index;
						break;
					}
				}

				if (ImGui::Combo("Operator", &operatorIndex, operators, IM_ARRAYSIZE(operators)))
				{
					node->stringData = operators[operatorIndex];
				}
			}

			if (node->typeCategory == "Flow" && node->name == "If")
			{
				ImGui::Separator();
				ImGui::Text("If");
				if (node->inputs.size() >= 5 && node->outputs.size() == 1)
				{
					ImGui::TextWrapped("Compares A and B, then outputs the A<B, A=B, or A>B input value based on the result.");
				}
				else
				{
					ImGui::TextWrapped("Legacy If node: outputs 1.0 or 0.0 for the comparisons A > B, A == B, and A < B.");
				}
			}

			if (node->typeCategory == "Flow" && node->name == "For Iteration")
			{
				ImGui::Separator();
				ImGui::TextWrapped("Use the array node's Count output to clamp the iteration index before array access.");
			}

			if (IsEditingMaterialFunction() && node->typeCategory == "FunctionInput")
			{
				ImGui::Separator();
				ImGui::Text("Function Input");

				char nameBuffer[256];
				strncpy(nameBuffer, node->name.c_str(), sizeof(nameBuffer));
				nameBuffer[sizeof(nameBuffer) - 1] = 0;
				if (ImGui::InputText("Input Name", nameBuffer, sizeof(nameBuffer)))
				{
					node->name = nameBuffer;
					if (!node->outputs.empty())
					{
						node->outputs[0].name = nameBuffer;
					}
				}

				int pinType = static_cast<int>(node->outputs.empty() ? ShaderPinType::Float : node->outputs[0].type);
				const char* pinTypeLabels[] = { "None", "Float", "Vector2", "Vector3", "Vector4", "Vector4i", "Matrix4x4", "Texture", "Any" };
				if (ImGui::Combo("Input Type", &pinType, pinTypeLabels, IM_ARRAYSIZE(pinTypeLabels)) && !node->outputs.empty())
				{
					node->outputs[0].type = static_cast<ShaderPinType>(pinType);
					node->outputs[0].defaultValue = GetDefaultValueForPinType(node->outputs[0].type);
				}
			}

			if (IsEditingMaterialFunction() && node->typeCategory == "Master" && !node->inputs.empty())
			{
				ImGui::Separator();
				ImGui::Text("Function Outputs");

				if (ImGui::Button("Add Output"))
				{
					node->inputs.push_back({
						nextId_++,
						node->id,
						"Result" + std::to_string(node->inputs.size() + 1),
						ShaderPinType::Float,
						ShaderPinKind::Input,
						0.0f
						});
					node->size.y = GetNodeBodyHeightFromPins(node->inputs.size(), node->outputs.size());
				}

				const char* pinTypeLabels[] = { "None", "Float", "Vector2", "Vector3", "Vector4", "Vector4i", "Matrix4x4", "Texture", "Any" };
				for (size_t outputIndex = 0; outputIndex < node->inputs.size(); ++outputIndex)
				{
					ShaderPin& outputPin = node->inputs[outputIndex];
					ImGui::PushID(outputPin.id);

					char outputNameBuffer[256];
					strncpy(outputNameBuffer, outputPin.name.c_str(), sizeof(outputNameBuffer));
					outputNameBuffer[sizeof(outputNameBuffer) - 1] = 0;
					if (ImGui::InputText("Output Name", outputNameBuffer, sizeof(outputNameBuffer)))
					{
						outputPin.name = outputNameBuffer;
					}

					int pinType = static_cast<int>(outputPin.type);
					if (ImGui::Combo("Output Type", &pinType, pinTypeLabels, IM_ARRAYSIZE(pinTypeLabels)))
					{
						outputPin.type = static_cast<ShaderPinType>(pinType);
						outputPin.defaultValue = GetDefaultValueForPinType(outputPin.type);
					}

					if (node->inputs.size() > 1)
					{
						if (ImGui::Button("Remove Output"))
						{
							const int pinIdToRemove = outputPin.id;
							links_.erase(std::remove_if(links_.begin(), links_.end(), [pinIdToRemove](const ShaderLink& link)
								{
									return link.endPinId == pinIdToRemove;
								}), links_.end());
							node->inputs.erase(node->inputs.begin() + outputIndex);
							node->size.y = GetNodeBodyHeightFromPins(node->inputs.size(), node->outputs.size());
							ImGui::PopID();
							break;
						}
					}

					ImGui::Separator();
					ImGui::PopID();
				}
			}

			if (!node->inputs.empty())
			{
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

const char* ShaderEditorPanel::GetCurrentEditorReflectionFileType() const
{
	return IsEditingMaterial() ? kMaterialEditorReflectionFileType : kMaterialFunctionEditorReflectionFileType;
}

std::string ShaderEditorPanel::GetOpenButtonLabel() const
{
	return IsEditingMaterial() ? "Open Material" : "Open Material Function";
}

std::string ShaderEditorPanel::GetSaveButtonLabel() const
{
	return IsEditingMaterial() ? "Save Material" : "Save Material Function";
}

std::string ShaderEditorPanel::GetBackButtonLabel() const
{
	if (navigationStack_.empty())
	{
		return "Back";
	}

	return navigationStack_.back().assetMode == ShaderEditorAssetMode::Material ? "Back To Material" : "Back";
}

bool ShaderEditorPanel::LoadMaterialFunctionSignature(const std::string& assetPath, MaterialFunction& outMaterialFunction) const
{
	return MaterialFunctionSerializer::Deserialize(assetPath, outMaterialFunction);
}

void ShaderEditorPanel::ApplyMaterialFunctionSignatureToGraph(const MaterialFunction& materialFunction)
{
	std::vector<MaterialFunctionPinDefinition> inputDefinitions = materialFunction.GetInputs();

	ImVec2 spawnPosition(120.0f, 80.0f);
	for (const MaterialFunctionPinDefinition& inputDefinition : inputDefinitions)
	{
		ShaderNode inputNode = SpawnNode("FunctionInput", "Function Input", spawnPosition);
		inputNode.name = inputDefinition.name;
		if (!inputNode.outputs.empty())
		{
			inputNode.outputs[0].name = inputDefinition.name;
			inputNode.outputs[0].type = inputDefinition.type;
			inputNode.outputs[0].defaultValue = GetDefaultValueForPinType(inputDefinition.type);
		}
		nodes_.push_back(inputNode);
		spawnPosition.y += 120.0f;
	}

	if (ShaderNode* masterNode = FindNode(masterNodeId_))
	{
		masterNode->inputs.clear();
		const std::vector<MaterialFunctionPinDefinition>& outputDefinitions = materialFunction.GetOutputs();
		if (outputDefinitions.empty())
		{
			masterNode->inputs.push_back({ nextId_++, masterNode->id, "Result", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
		}
		else
		{
			for (const MaterialFunctionPinDefinition& outputDefinition : outputDefinitions)
			{
				masterNode->inputs.push_back({
					nextId_++,
					masterNode->id,
					outputDefinition.name,
					outputDefinition.type,
					ShaderPinKind::Input,
					GetDefaultValueForPinType(outputDefinition.type)
					});
			}
		}

		masterNode->size.y = GetNodeBodyHeightFromPins(masterNode->inputs.size(), masterNode->outputs.size());
	}
}

void ShaderEditorPanel::RefreshMaterialFunctionCallNodeSignature(ShaderNode& node)
{
	MaterialFunction materialFunction;
	if (!LoadMaterialFunctionSignature(node.stringData, materialFunction))
	{
		return;
	}

	const int originalNodeId = node.id;
	const ImVec2 originalPosition = node.pos;
	std::vector<int> existingPinIds;
	for (const ShaderPin& pin : node.inputs)
	{
		existingPinIds.push_back(pin.id);
	}
	for (const ShaderPin& pin : node.outputs)
	{
		existingPinIds.push_back(pin.id);
	}

	links_.erase(std::remove_if(links_.begin(), links_.end(), [&existingPinIds](const ShaderLink& link)
		{
			return std::find(existingPinIds.begin(), existingPinIds.end(), link.startPinId) != existingPinIds.end() ||
				std::find(existingPinIds.begin(), existingPinIds.end(), link.endPinId) != existingPinIds.end();
		}), links_.end());

	node.inputs.clear();
	node.outputs.clear();
	node.typeCategory = "Functions";
	node.name = materialFunction.GetName().empty() ? std::filesystem::path(node.stringData).filename().generic_string() : materialFunction.GetName();

	for (const MaterialFunctionPinDefinition& inputDefinition : materialFunction.GetInputs())
	{
		node.inputs.push_back({
			nextId_++,
			originalNodeId,
			inputDefinition.name,
			inputDefinition.type,
			ShaderPinKind::Input,
			GetDefaultValueForPinType(inputDefinition.type)
		});
	}

	const std::vector<MaterialFunctionPinDefinition>& outputDefinitions = materialFunction.GetOutputs();
	if (outputDefinitions.empty())
	{
		node.outputs.push_back({
			nextId_++,
			originalNodeId,
			"Result",
			ShaderPinType::Float,
			ShaderPinKind::Output,
			0.0f
			});
	}
	else
	{
		for (const MaterialFunctionPinDefinition& outputDefinition : outputDefinitions)
		{
			node.outputs.push_back({
				nextId_++,
				originalNodeId,
				outputDefinition.name,
				outputDefinition.type,
				ShaderPinKind::Output,
				GetDefaultValueForPinType(outputDefinition.type)
				});
		}
	}

	node.pos = originalPosition;
	node.size.y = GetNodeBodyHeightFromPins(node.inputs.size(), node.outputs.size());
}

ShaderNode ShaderEditorPanel::CreateMaterialFunctionCallNode(const std::string& assetPath, ImVec2 pos)
{
	ShaderNode node;
	node.id = nextId_++;
	node.name = std::filesystem::path(assetPath).filename().generic_string();
	node.typeCategory = "Functions";
	node.pos = pos;
	node.size = ImVec2(220, 95);
	node.stringData = assetPath;

	RefreshMaterialFunctionCallNodeSignature(node);
	return node;
}

std::vector<std::string> ShaderEditorPanel::GetMaterialFunctionAssetPaths() const
{
	std::vector<std::string> assetPaths;
	const std::filesystem::path contentRoot = EditorAssetPathUtils::GetContentRootPath();
	if (!std::filesystem::exists(contentRoot))
	{
		return assetPaths;
	}

	for (const auto& entry : std::filesystem::recursive_directory_iterator(contentRoot, std::filesystem::directory_options::skip_permission_denied))
	{
		if (!entry.is_regular_file())
		{
			continue;
		}

		tinyxml2::XMLDocument document;
		if (document.LoadFile(entry.path().generic_string().c_str()) != tinyxml2::XML_SUCCESS)
		{
			continue;
		}

		tinyxml2::XMLElement* root = document.FirstChildElement("GameAsset");
		const char* fileType = root ? root->Attribute("FileType") : nullptr;
		if (fileType && std::string(fileType) == "MaterialFunction")
		{
			assetPaths.push_back(EditorAssetPathUtils::ToContentRelativePath(entry.path().generic_string()));
		}
	}

	std::sort(assetPaths.begin(), assetPaths.end());
	return assetPaths;
}

void ShaderEditorPanel::OpenMaterialFunctionFromNode(const std::string& assetPath)
{
	if (assetPath.empty())
	{
		return;
	}

	CaptureSnapshotForNavigation();
	SetAssetMode(ShaderEditorAssetMode::MaterialFunction);
	currentAssetPath_ = assetPath;

	MaterialFunction materialFunction;
	if (!LoadMaterialFunctionSignature(assetPath, materialFunction))
	{
		RestorePreviousSnapshot();
		return;
	}

	ResetEditorGraph(ImVec2(700.0f, 300.0f));
	ApplyMaterialFunctionSignatureToGraph(materialFunction);

	const std::string absoluteAssetPath = EditorAssetPathUtils::GetContentRootPath() + assetPath;
	if (!LoadEditorReflection(absoluteAssetPath))
	{
		ApplyHierarchicalLayout();
		SaveEditorReflection(absoluteAssetPath);
	}
}

ShaderNode ShaderEditorPanel::SpawnNode(const std::string& category, const std::string& name, ImVec2 pos)
{
	ShaderNode node;
	node.id = nextId_++;
	node.name = name;
	node.typeCategory = category;
	node.pos = pos;
	node.size = ImVec2(160, 80);

	if (category == "Variables")
	{
		ShaderPinType type = ShaderPinType::Matrix4x4;
		if (name == "deltaTime" || name == "elapsedTime") type = ShaderPinType::Float;
		else if (name == "textureUV" || name == SHADER_VARIABLE_NAMES::VERTEX::UV) type = ShaderPinType::Vector2;
		else if (name == "viewPosition" || name == "vertexNormal" ||
			name == SHADER_VARIABLE_NAMES::VERTEX::POSITION ||
			name == SHADER_VARIABLE_NAMES::VERTEX::NORMAL ||
			name == SHADER_VARIABLE_NAMES::VERTEX::MODIFIED_POSITION) type = ShaderPinType::Vector3;
		else if (name == "fragmentPositionWorldSpace" || name == "fragmentPositionScreenSpace" ||
			name == "vertexColor" || name == "outWeights" ||
			name == SHADER_VARIABLE_NAMES::VERTEX::COLOR ||
			name == SHADER_VARIABLE_NAMES::VERTEX::WEIGHTS ||
			name == "directionalLightSpaceFragmentPositions" || name == "spotLightSpaceFragmentPositions") type = ShaderPinType::Vector4;
		else if (name == "outBoneIDs" || name == SHADER_VARIABLE_NAMES::VERTEX::BONE_IDS) type = ShaderPinType::Vector4i;

		node.outputs.push_back({ nextId_++, node.id, "Value", type, ShaderPinKind::Output });
		node.size = ImVec2(320, 60);
	}
	else if (category == "MaterialVariable")
	{
		node.name = SanitizeIdentifier(name);
		node.outputs.push_back({ nextId_++, node.id, "Value", ShaderPinType::Float, ShaderPinKind::Output, 0.0f });
		node.size = ImVec2(220, 60);
		node.isUniform = true;
	}
	else if (category == "MaterialVariableArray")
	{
		node.name = SanitizeIdentifier(name);
		node.inputs.push_back({ nextId_++, node.id, "Index", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
		node.outputs.push_back({ nextId_++, node.id, "Value", ShaderPinType::Float, ShaderPinKind::Output, 0.0f });
		node.outputs.push_back({ nextId_++, node.id, "Count", ShaderPinType::Float, ShaderPinKind::Output, 1.0f });
		node.arrayDefaultValues.push_back(0.0f);
		node.size = ImVec2(230, 95);
		node.isUniform = true;
	}
	else if (category == "FunctionInput")
	{
		node.name = "Input";
		node.outputs.push_back({ nextId_++, node.id, "Input", ShaderPinType::Float, ShaderPinKind::Output, 0.0f });
		node.size = ImVec2(200, 60);
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
	else if (category == "Texture")
	{
		if (name == "Texture Sample") {
			node.stringData = "diffuseTexture";
			node.inputs.push_back({ nextId_++, node.id, "UV", ShaderPinType::Vector2, ShaderPinKind::Input, Vector2(0.f) });
			node.outputs.push_back({ nextId_++, node.id, "Color", ShaderPinType::Vector4, ShaderPinKind::Output });
			node.size = ImVec2(200, 80);
		}
	}
	else if (category == "Custom")
	{
		if (name == "Custom GLSL") {
			node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Any, ShaderPinKind::Output });
			node.size = ImVec2(250, 80);
		}
	}
	else if (category == "Flow")
	{
		if (name == "If")
		{
			node.inputs.push_back({ nextId_++, node.id, "A", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
			node.inputs.push_back({ nextId_++, node.id, "B", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
			node.inputs.push_back({ nextId_++, node.id, "A<B", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.inputs.push_back({ nextId_++, node.id, "A=B", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.inputs.push_back({ nextId_++, node.id, "A>B", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Any, ShaderPinKind::Output, 0.0f });
			node.size = ImVec2(200, 195);
		}
		else if (name == "If Check")
		{
			node.stringData = ">";
			node.inputs.push_back({ nextId_++, node.id, "A", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
			node.inputs.push_back({ nextId_++, node.id, "B", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
			node.inputs.push_back({ nextId_++, node.id, "True", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.inputs.push_back({ nextId_++, node.id, "False", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Any, ShaderPinKind::Output, 0.0f });
			node.size = ImVec2(200, 145);
		}
		else if (name == "For Iteration")
		{
			node.inputs.push_back({ nextId_++, node.id, "Iteration", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
			node.inputs.push_back({ nextId_++, node.id, "Count", ShaderPinType::Float, ShaderPinKind::Input, 1.0f });
			node.outputs.push_back({ nextId_++, node.id, "Index", ShaderPinType::Float, ShaderPinKind::Output, 0.0f });
			node.size = ImVec2(190, 95);
		}
	}
	else
	{
		static const std::unordered_set<std::string> func1Arg = {
			"Radians", "Degrees", "Sine", "Cosine", "Tangent", "Asin", "Acos", "Sinh", "Cosh", "Tanh",
			"Asinh", "Acosh", "Atanh", "Exp", "Log", "Exp2", "Log2", "Sqrt", "InverseSqrt", "Abs", "Sign",
			"Floor", "Trunc", "Round", "RoundEven", "Ceil", "Fract", "Normalize", "Transpose", "Inverse",
			"FloatBitsToInt", "FloatBitsToUint", "IntBitsToFloat", "UintBitsToFloat"
		};
		static const std::unordered_set<std::string> func1ArgFloatOut = {
			"Length", "Determinant", "IsNan", "IsInf"
		};
		static const std::unordered_set<std::string> func2Arg = {
			"Add", "Subtract", "Multiply", "Divide", "Modulo",
			"Pow", "Min", "Max", "Step", "Reflect", "MatrixCompMult", "OuterProduct", "Atan", "Ldexp"
		};
		static const std::unordered_set<std::string> func2ArgFloatOut = {
			"Distance", "Dot"
		};
		static const std::unordered_set<std::string> func3Arg = {
			"Clamp", "Mix", "Smoothstep", "Fma", "Faceforward", "Refract"
		};

		if (func1Arg.count(name)) {
			node.inputs.push_back({ nextId_++, node.id, "X", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Any, ShaderPinKind::Output });
			node.size = ImVec2(160, 65);
		}
		else if (name == "Mask") {
			node.inputs.push_back({ nextId_++, node.id, "Value", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "X", ShaderPinType::Float, ShaderPinKind::Output, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "Y", ShaderPinType::Float, ShaderPinKind::Output, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "Z", ShaderPinType::Float, ShaderPinKind::Output, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "W", ShaderPinType::Float, ShaderPinKind::Output, 0.0f });
			node.size = ImVec2(180, 150);
		}
		else if (func1ArgFloatOut.count(name)) {
			node.inputs.push_back({ nextId_++, node.id, "X", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Float, ShaderPinKind::Output });
			node.size = ImVec2(160, 65);
		}
		else if (func2Arg.count(name)) {
			node.inputs.push_back({ nextId_++, node.id, "A", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.inputs.push_back({ nextId_++, node.id, "B", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Any, ShaderPinKind::Output });
			node.size = ImVec2(160, 95);
		}
		else if (func2ArgFloatOut.count(name)) {
			node.inputs.push_back({ nextId_++, node.id, "A", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.inputs.push_back({ nextId_++, node.id, "B", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Float, ShaderPinKind::Output });
			node.size = ImVec2(160, 95);
		}
		else if (func3Arg.count(name)) {
			node.inputs.push_back({ nextId_++, node.id, "A", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.inputs.push_back({ nextId_++, node.id, "B", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.inputs.push_back({ nextId_++, node.id, "C", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Any, ShaderPinKind::Output });
			node.size = ImVec2(160, 120);
		}
		else if (name == "Cross") {
			node.inputs.push_back({ nextId_++, node.id, "A", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f) });
			node.inputs.push_back({ nextId_++, node.id, "B", ShaderPinType::Vector3, ShaderPinKind::Input, Vector3(0.f) });
			node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Vector3, ShaderPinKind::Output });
			node.size = ImVec2(160, 95);
		}
		else if (name == "Modf" || name == "Frexp") {
			node.inputs.push_back({ nextId_++, node.id, "X", ShaderPinType::Any, ShaderPinKind::Input, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "Out", ShaderPinType::Any, ShaderPinKind::Output });
			node.outputs.push_back({ nextId_++, node.id, name == "Modf" ? "I" : "Exp", ShaderPinType::Any, ShaderPinKind::Output });
			node.size = ImVec2(160, 95);
		}
	}

	return node;
}

ShaderNode ShaderEditorPanel::CreateMaterialVariableAccessorNode(const ShaderNode& declarationNode, bool createSetter, ImVec2 pos)
{
	ShaderNode node;
	node.id = nextId_++;
	node.pos = pos;
	node.size = ImVec2(220, 60);
	node.isUniform = declarationNode.isUniform;
	node.stringData = SanitizeIdentifier(declarationNode.name);

	const bool isArray = declarationNode.typeCategory == "MaterialVariableArray";
	const ShaderPinType valueType = declarationNode.outputs.empty() ? ShaderPinType::Float : declarationNode.outputs[0].type;
	const ShaderValue defaultValue =
		isArray && !declarationNode.arrayDefaultValues.empty() ? declarationNode.arrayDefaultValues.front() :
		(declarationNode.outputs.empty() ? GetDefaultValueForPinType(valueType) : declarationNode.outputs[0].defaultValue);

	if (isArray)
	{
		node.typeCategory = createSetter ? "MaterialVariableArraySet" : "MaterialVariableArrayGet";
		node.name = std::string(createSetter ? "Set " : "Get ") + node.stringData;
		if (createSetter)
		{
			node.inputs.push_back({ nextId_++, node.id, "Value", valueType, ShaderPinKind::Input, defaultValue });
			node.inputs.push_back({ nextId_++, node.id, "Index", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "Value", valueType, ShaderPinKind::Output, defaultValue });
			node.size = ImVec2(230, 110);
		}
		else
		{
			node.inputs.push_back({ nextId_++, node.id, "Index", ShaderPinType::Float, ShaderPinKind::Input, 0.0f });
			node.outputs.push_back({ nextId_++, node.id, "Value", valueType, ShaderPinKind::Output, defaultValue });
			node.outputs.push_back({ nextId_++, node.id, "Count", ShaderPinType::Float, ShaderPinKind::Output, (float)std::max<size_t>(1, declarationNode.arrayDefaultValues.size()) });
			node.size = ImVec2(230, 95);
		}
	}
	else
	{
		node.typeCategory = createSetter ? "MaterialVariableSet" : "MaterialVariableGet";
		node.name = std::string(createSetter ? "Set " : "Get ") + node.stringData;
		if (createSetter)
		{
			node.inputs.push_back({ nextId_++, node.id, "Value", valueType, ShaderPinKind::Input, defaultValue });
			node.outputs.push_back({ nextId_++, node.id, "Value", valueType, ShaderPinKind::Output, defaultValue });
			node.size = ImVec2(220, 85);
		}
		else
		{
			node.outputs.push_back({ nextId_++, node.id, "Value", valueType, ShaderPinKind::Output, defaultValue });
			node.size = ImVec2(220, 60);
		}
	}

	return node;
}

void ShaderEditorPanel::SynchronizeMaterialVariableAccessorNodes()
{
	for (ShaderNode& node : nodes_)
	{
		if (!IsMaterialVariableAccessorCategory(node.typeCategory))
		{
			continue;
		}

		const ShaderNode* declarationNode = FindMaterialVariableDeclarationNode(nodes_, GetMaterialVariableReferenceName(node));
		if (!declarationNode)
		{
			continue;
		}

		const bool isArray = declarationNode->typeCategory == "MaterialVariableArray";
		const ShaderPinType valueType = declarationNode->outputs.empty() ? ShaderPinType::Float : declarationNode->outputs[0].type;
		const ShaderValue defaultValue =
			isArray && !declarationNode->arrayDefaultValues.empty() ? declarationNode->arrayDefaultValues.front() :
			(declarationNode->outputs.empty() ? GetDefaultValueForPinType(valueType) : declarationNode->outputs[0].defaultValue);

		node.isUniform = declarationNode->isUniform;
		node.stringData = SanitizeIdentifier(declarationNode->name);
		node.name = std::string(IsMaterialVariableSetterCategory(node.typeCategory) ? "Set " : "Get ") + node.stringData;

		if (node.typeCategory == "MaterialVariableGet" && !node.outputs.empty())
		{
			node.outputs[0].name = "Value";
			node.outputs[0].type = valueType;
			node.outputs[0].defaultValue = defaultValue;
			node.size = ImVec2(220, 60);
		}
		else if (node.typeCategory == "MaterialVariableSet" && !node.inputs.empty() && !node.outputs.empty())
		{
			node.inputs[0].name = "Value";
			node.inputs[0].type = valueType;
			node.inputs[0].defaultValue = defaultValue;
			node.outputs[0].name = "Value";
			node.outputs[0].type = valueType;
			node.outputs[0].defaultValue = defaultValue;
			node.size = ImVec2(220, 85);
		}
		else if (node.typeCategory == "MaterialVariableArrayGet" && !node.inputs.empty() && node.outputs.size() >= 2)
		{
			node.inputs[0].name = "Index";
			node.inputs[0].type = ShaderPinType::Float;
			node.outputs[0].name = "Value";
			node.outputs[0].type = valueType;
			node.outputs[0].defaultValue = defaultValue;
			node.outputs[1].name = "Count";
			node.outputs[1].type = ShaderPinType::Float;
			node.outputs[1].defaultValue = (float)std::max<size_t>(1, declarationNode->arrayDefaultValues.size());
			node.size = ImVec2(230, 95);
		}
		else if (node.typeCategory == "MaterialVariableArraySet" && node.inputs.size() >= 2 && !node.outputs.empty())
		{
			node.inputs[0].name = "Value";
			node.inputs[0].type = valueType;
			node.inputs[0].defaultValue = defaultValue;
			node.inputs[1].name = "Index";
			node.inputs[1].type = ShaderPinType::Float;
			node.outputs[0].name = "Value";
			node.outputs[0].type = valueType;
			node.outputs[0].defaultValue = defaultValue;
			node.size = ImVec2(230, 110);
		}
	}
}

void ShaderEditorPanel::RenameMaterialVariableAccessorReferences(const std::string& previousName, const std::string& newName)
{
	const std::string sanitizedPreviousName = SanitizeIdentifier(previousName);
	const std::string sanitizedNewName = SanitizeIdentifier(newName);
	if (sanitizedPreviousName == sanitizedNewName)
	{
		return;
	}

	for (ShaderNode& node : nodes_)
	{
		if (!IsMaterialVariableAccessorCategory(node.typeCategory))
		{
			continue;
		}

		if (GetMaterialVariableReferenceName(node) == sanitizedPreviousName)
		{
			node.stringData = sanitizedNewName;
		}
	}

	SynchronizeMaterialVariableAccessorNodes();
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
	if (!outMaterialData || !IsEditingMaterial())
	{
		return;
	}

	SynchronizeMaterialVariableAccessorNodes();

	outMaterialData->baseColor.calculation = "";
	outMaterialData->baseColor.result = "";
	outMaterialData->emmisiveColor.calculation = "";
	outMaterialData->emmisiveColor.result = "";
	outMaterialData->fragmentNormal.calculation = "";
	outMaterialData->fragmentNormal.result = "";
	outMaterialData->vertexPositionOffset.calculation = "";
	outMaterialData->vertexPositionOffset.result = "";
	outMaterialData->vertexShaderFunctions = "";
	outMaterialData->fragmentShaderFunctions = "";
	outMaterialData->vertexShaderUniforms = "";
	outMaterialData->fragmentShaderUniforms = "";

	ShaderNode* master = FindNode(masterNodeId_);
	if (!master)
	{
		return;
	}

	auto GetGLSLTypeString = [](ShaderPinType type) -> std::string
	{
		switch (type)
		{
		case ShaderPinType::Float: return "float";
		case ShaderPinType::Vector2: return "vec2";
		case ShaderPinType::Vector3: return "vec3";
		case ShaderPinType::Vector4: return "vec4";
		case ShaderPinType::Vector4i: return "ivec4";
		case ShaderPinType::Matrix4x4: return "mat4";
		case ShaderPinType::Texture: return "sampler2D";
		case ShaderPinType::Any:
		case ShaderPinType::None:
		default:
			return "float";
		}
	};

	auto PromoteTypes = [](ShaderPinType left, ShaderPinType right) -> ShaderPinType
	{
		if (left == right) return left;
		if (left == ShaderPinType::Any) return right;
		if (right == ShaderPinType::Any) return left;
		if (left == ShaderPinType::Float) return right;
		if (right == ShaderPinType::Float) return left;
		return std::max(left, right);
	};

	auto GetDefaultValueString = [&](const ShaderPin& pin) -> std::pair<std::string, ShaderPinType>
	{
		switch (pin.type)
		{
		case ShaderPinType::Vector2:
			if (std::holds_alternative<Vector2>(pin.defaultValue))
			{
				const Vector2 value = std::get<Vector2>(pin.defaultValue);
				return { "vec2(" + std::to_string(value.x) + ", " + std::to_string(value.y) + ")", ShaderPinType::Vector2 };
			}
			break;
		case ShaderPinType::Vector3:
			if (std::holds_alternative<Vector3>(pin.defaultValue))
			{
				const Vector3 value = std::get<Vector3>(pin.defaultValue);
				return { "vec3(" + std::to_string(value.x) + ", " + std::to_string(value.y) + ", " + std::to_string(value.z) + ")", ShaderPinType::Vector3 };
			}
			break;
		case ShaderPinType::Vector4:
			if (std::holds_alternative<Vector4>(pin.defaultValue))
			{
				const Vector4 value = std::get<Vector4>(pin.defaultValue);
				return { "vec4(" + std::to_string(value.x) + ", " + std::to_string(value.y) + ", " + std::to_string(value.z) + ", " + std::to_string(value.w) + ")", ShaderPinType::Vector4 };
			}
			break;
		case ShaderPinType::Any:
		case ShaderPinType::Float:
		case ShaderPinType::None:
		default:
			if (std::holds_alternative<float>(pin.defaultValue))
			{
				return { std::to_string(std::get<float>(pin.defaultValue)) + "f", ShaderPinType::Float };
			}
			break;
		}

		return { "0.0f", ShaderPinType::Float };
	};

	auto GetGLSLFuncName = [](const std::string& nodeName) -> std::string
	{
		if (nodeName == "Sine") return "sin";
		if (nodeName == "Cosine") return "cos";
		if (nodeName == "Tangent") return "tan";
		if (nodeName == "InverseSqrt") return "inversesqrt";
		if (nodeName == "FloatBitsToInt") return "floatBitsToInt";
		if (nodeName == "FloatBitsToUint") return "floatBitsToUint";
		if (nodeName == "IntBitsToFloat") return "intBitsToFloat";
		if (nodeName == "UintBitsToFloat") return "uintBitsToFloat";
		if (nodeName == "MatrixCompMult") return "matrixCompMult";
		if (nodeName == "OuterProduct") return "outerProduct";
		if (nodeName == "RoundEven") return "roundEven";
		if (nodeName == "IsNan") return "isnan";
		if (nodeName == "IsInf") return "isinf";

		std::string glslName = nodeName;
		glslName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(glslName[0])));
		return glslName;
	};

	auto GetMaskComponentExpression = [](const std::string& value, ShaderPinType inputType, size_t componentIndex) -> std::string
	{
		static const char* componentNames[] = { "x", "y", "z", "w" };
		if (componentIndex >= 4)
		{
			return "0.0f";
		}

		switch (inputType)
		{
		case ShaderPinType::Float:
			return componentIndex == 0 ? value : "0.0f";
		case ShaderPinType::Vector2:
			return componentIndex < 2 ? value + "." + componentNames[componentIndex] : "0.0f";
		case ShaderPinType::Vector3:
			return componentIndex < 3 ? value + "." + componentNames[componentIndex] : "0.0f";
		case ShaderPinType::Vector4:
			return value + "." + componentNames[componentIndex];
		case ShaderPinType::Vector4i:
			return "float(" + value + "." + componentNames[componentIndex] + ")";
		case ShaderPinType::Any:
		case ShaderPinType::Matrix4x4:
		case ShaderPinType::Texture:
		case ShaderPinType::None:
		default:
			return componentIndex == 0 ? value : "0.0f";
		}
	};

	auto FormatMasterOutput = [](const std::string& value, ShaderPinType actualType, ShaderPinType expectedType) -> std::string
	{
		if (actualType == expectedType || actualType == ShaderPinType::Any)
		{
			return value;
		}

		if (expectedType == ShaderPinType::Vector4 && actualType == ShaderPinType::Vector3) return "vec4(" + value + ", 1.0f)";
		if (expectedType == ShaderPinType::Vector4 && actualType == ShaderPinType::Float) return "vec4(" + value + ")";
		if (expectedType == ShaderPinType::Vector3 && actualType == ShaderPinType::Vector4) return value + ".xyz";
		if (expectedType == ShaderPinType::Vector3 && actualType == ShaderPinType::Float) return "vec3(" + value + ")";
		if (expectedType == ShaderPinType::Vector2 && actualType == ShaderPinType::Float) return "vec2(" + value + ")";
		return value;
	};

	std::string materialVariableDeclarations;
	for (const ShaderNode& node : nodes_)
	{
		if (node.typeCategory != "MaterialVariable" && node.typeCategory != "MaterialVariableArray")
		{
			continue;
		}

		ShaderNode declarationNode = node;
		declarationNode.name = SanitizeIdentifier(declarationNode.name);
		if (declarationNode.typeCategory == "MaterialVariableArray")
		{
			EnsureArrayDefaultsMatchNode(declarationNode);
		}
		else if (!declarationNode.outputs.empty())
		{
			EnsureValueMatchesType(declarationNode.outputs[0].defaultValue, declarationNode.outputs[0].type);
		}

		materialVariableDeclarations += BuildMaterialNodeDeclaration(declarationNode);
		if (!materialVariableDeclarations.empty() && materialVariableDeclarations.back() != '\n')
		{
			materialVariableDeclarations += "\n";
		}
	}

	outMaterialData->vertexShaderUniforms = materialVariableDeclarations;
	outMaterialData->fragmentShaderUniforms = materialVariableDeclarations;

	auto CompileSubGraph = [&](const std::vector<std::string>& targetMasterPins, std::string& outCalculation, std::string& outFunctionDefinitions, std::unordered_map<int, ShaderPinType>& resolvedPinTypes)
	{
		std::vector<ShaderNode*> executionOrder;
		std::unordered_set<int> visitedNodes;
		std::unordered_set<std::string> emittedFunctionNames;

		std::function<void(int)> backtrace = [&](int nodeId)
		{
			if (visitedNodes.find(nodeId) != visitedNodes.end())
			{
				return;
			}

			visitedNodes.insert(nodeId);
			ShaderNode* node = FindNode(nodeId);
			if (!node)
			{
				return;
			}

			for (const ShaderPin& inputPin : node->inputs)
			{
				for (const ShaderLink& link : links_)
				{
					if (link.endPinId == inputPin.id)
					{
						if (ShaderPin* connectedOutput = FindPin(link.startPinId))
						{
							backtrace(connectedOutput->nodeId);
						}
					}
				}
			}

			if (nodeId != masterNodeId_)
			{
				executionOrder.push_back(node);
			}
		};

		for (const ShaderPin& masterInput : master->inputs)
		{
			if (std::find(targetMasterPins.begin(), targetMasterPins.end(), masterInput.name) == targetMasterPins.end())
			{
				continue;
			}

			for (const ShaderLink& link : links_)
			{
				if (link.endPinId == masterInput.id)
				{
					if (ShaderPin* connectedOutput = FindPin(link.startPinId))
					{
						backtrace(connectedOutput->nodeId);
					}
				}
			}
		}

		auto GetPinValueAndType = [&](const ShaderPin& pin) -> std::pair<std::string, ShaderPinType>
		{
			for (const ShaderLink& link : links_)
			{
				if (link.endPinId != pin.id)
				{
					continue;
				}

				ShaderPin* outputPin = FindPin(link.startPinId);
				ShaderNode* sourceNode = outputPin ? FindNode(outputPin->nodeId) : nullptr;
				if (!outputPin || !sourceNode)
				{
					continue;
				}

				if (sourceNode->typeCategory == "Variables" ||
					sourceNode->typeCategory == "MaterialVariable" ||
					sourceNode->typeCategory == "MaterialVariableGet")
				{
					return { sourceNode->typeCategory == "MaterialVariableGet" ? GetMaterialVariableReferenceName(*sourceNode) : sourceNode->name, outputPin->type };
				}

				const ShaderPinType actualType = resolvedPinTypes.count(outputPin->id) ? resolvedPinTypes.at(outputPin->id) : outputPin->type;
				return { "node_" + std::to_string(sourceNode->id) + "_out_" + std::to_string(outputPin->id), actualType };
			}

			return GetDefaultValueString(pin);
		};

		if (executionOrder.empty())
		{
			outCalculation.clear();
			outFunctionDefinitions.clear();
			return;
		}

		std::string glslBody = "\n\t// --- Generated Node Graph Calculations ---\n";
		std::string functionDefinitions;

		for (ShaderNode* node : executionOrder)
		{
			if (!node || node->typeCategory == "Variables" || node->typeCategory == "MaterialVariable" || node->typeCategory == "MaterialVariableGet")
			{
				continue;
			}

			glslBody += "\t// Node: " + node->name + "\n";
			const std::string outVar = node->outputs.empty() ? "" : "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[0].id);

			if (node->typeCategory == "Math" || node->typeCategory == "Trigonometry" ||
				node->typeCategory == "Exponential" || node->typeCategory == "Geometric" ||
				node->typeCategory == "Matrix")
			{
				if (node->name == "Mask")
				{
					auto [value, type] = GetPinValueAndType(node->inputs[0]);
					for (size_t outputIndex = 0; outputIndex < node->outputs.size(); ++outputIndex)
					{
						const ShaderPin& outputPin = node->outputs[outputIndex];
						const std::string outputVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(outputPin.id);
						resolvedPinTypes[outputPin.id] = ShaderPinType::Float;
						glslBody += "\tfloat " + outputVar + " = " + GetMaskComponentExpression(value, type, outputIndex) + ";\n";
					}
				}
				else if (node->name == "Add" || node->name == "Subtract" || node->name == "Multiply" || node->name == "Divide" || node->name == "Modulo")
				{
					auto [valueA, typeA] = GetPinValueAndType(node->inputs[0]);
					auto [valueB, typeB] = GetPinValueAndType(node->inputs[1]);
					const ShaderPinType outType = PromoteTypes(typeA, typeB);
					resolvedPinTypes[node->outputs[0].id] = outType;

					if (node->name == "Modulo")
					{
						glslBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = mod(" + valueA + ", " + valueB + ");\n";
					}
					else
					{
						const std::string op = node->name == "Add" ? "+" : node->name == "Subtract" ? "-" : node->name == "Multiply" ? "*" : "/";
						glslBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = " + valueA + " " + op + " " + valueB + ";\n";
					}
				}
				else if (node->name == "Modf" || node->name == "Frexp")
				{
					auto [valueX, typeX] = GetPinValueAndType(node->inputs[0]);
					resolvedPinTypes[node->outputs[0].id] = typeX;
					resolvedPinTypes[node->outputs[1].id] = typeX;

					const std::string outVar2 = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[1].id);
					const std::string funcName = GetGLSLFuncName(node->name);
					glslBody += "\t" + GetGLSLTypeString(typeX) + " " + outVar2 + ";\n";
					glslBody += "\t" + GetGLSLTypeString(typeX) + " " + outVar + " = " + funcName + "(" + valueX + ", " + outVar2 + ");\n";
				}
				else
				{
					const std::string funcName = GetGLSLFuncName(node->name);
					if (node->inputs.size() == 1)
					{
						auto [value, type] = GetPinValueAndType(node->inputs[0]);
						ShaderPinType outType = type;
						if (node->name == "Length" || node->name == "Determinant" || node->name == "IsNan" || node->name == "IsInf")
						{
							outType = ShaderPinType::Float;
						}
						resolvedPinTypes[node->outputs[0].id] = outType;
						glslBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = " + funcName + "(" + value + ");\n";
					}
					else if (node->inputs.size() == 2)
					{
						auto [valueA, typeA] = GetPinValueAndType(node->inputs[0]);
						auto [valueB, typeB] = GetPinValueAndType(node->inputs[1]);
						ShaderPinType outType = PromoteTypes(typeA, typeB);
						if (node->name == "Distance" || node->name == "Dot") outType = ShaderPinType::Float;
						if (node->name == "Cross") outType = ShaderPinType::Vector3;
						if (node->name == "OuterProduct") outType = ShaderPinType::Matrix4x4;
						resolvedPinTypes[node->outputs[0].id] = outType;
						glslBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = " + funcName + "(" + valueA + ", " + valueB + ");\n";
					}
					else if (node->inputs.size() == 3)
					{
						auto [valueA, typeA] = GetPinValueAndType(node->inputs[0]);
						auto [valueB, typeB] = GetPinValueAndType(node->inputs[1]);
						auto [valueC, typeC] = GetPinValueAndType(node->inputs[2]);
						ShaderPinType outType = PromoteTypes(typeA, typeB);
						outType = PromoteTypes(outType, typeC);
						resolvedPinTypes[node->outputs[0].id] = outType;
						glslBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = " + funcName + "(" + valueA + ", " + valueB + ", " + valueC + ");\n";
					}
				}
			}
			else if (node->typeCategory == "Texture" && node->name == "Texture Sample")
			{
				auto [uvValue, uvType] = GetPinValueAndType(node->inputs[0]);
				(void)uvType;
				resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Vector4;
				const std::string samplerName = node->stringData.empty() ? "diffuseTexture" : node->stringData;
				glslBody += "\tvec4 " + outVar + " = texture(" + samplerName + ", " + uvValue + ");\n";
			}
			else if ((node->typeCategory == "MaterialVariableArray" || node->typeCategory == "MaterialVariableArrayGet") && node->outputs.size() >= 2 && !node->inputs.empty())
			{
				auto [indexValue, indexType] = GetPinValueAndType(node->inputs[0]);
				(void)indexType;
				const std::string countVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[1].id);
				const std::string indexVar = "node_" + std::to_string(node->id) + "_index";
				const ShaderNode* declarationNode =
					node->typeCategory == "MaterialVariableArrayGet" ?
					FindMaterialVariableDeclarationNode(nodes_, GetMaterialVariableReferenceName(*node)) :
					node;
				const std::string arrayName =
					node->typeCategory == "MaterialVariableArrayGet" ?
					GetMaterialVariableReferenceName(*node) :
					node->name;
				const float arraySize = (float)std::max<size_t>(1, declarationNode ? declarationNode->arrayDefaultValues.size() : 1);

				resolvedPinTypes[node->outputs[0].id] = node->outputs[0].type;
				resolvedPinTypes[node->outputs[1].id] = ShaderPinType::Float;
				glslBody += "\tfloat " + countVar + " = " + std::to_string(arraySize) + "f;\n";
				glslBody += "\tint " + indexVar + " = int(clamp(floor(" + indexValue + "), 0.0f, max(" + countVar + " - 1.0f, 0.0f)));\n";
				glslBody += "\t" + GetGLSLTypeString(node->outputs[0].type) + " " + outVar + " = " + arrayName + "[" + indexVar + "];\n";
			}
			else if (node->typeCategory == "MaterialVariableSet" && !node->inputs.empty() && !node->outputs.empty())
			{
				auto [value, valueType] = GetPinValueAndType(node->inputs[0]);
				const ShaderNode* declarationNode = FindMaterialVariableDeclarationNode(nodes_, GetMaterialVariableReferenceName(*node));
				const ShaderPinType outputType = declarationNode && !declarationNode->outputs.empty() ? declarationNode->outputs[0].type : valueType;
				const std::string variableName = GetMaterialVariableReferenceName(*node);

				resolvedPinTypes[node->outputs[0].id] = outputType;
				if (declarationNode && !declarationNode->isUniform)
				{
					glslBody += "\t" + variableName + " = " + FormatMasterOutput(value, valueType, outputType) + ";\n";
					glslBody += "\t" + GetGLSLTypeString(outputType) + " " + outVar + " = " + variableName + ";\n";
				}
				else
				{
					glslBody += "\t" + GetGLSLTypeString(outputType) + " " + outVar + " = " + FormatMasterOutput(value, valueType, outputType) + ";\n";
				}
			}
			else if (node->typeCategory == "MaterialVariableArraySet" && node->inputs.size() >= 2 && !node->outputs.empty())
			{
				auto [value, valueType] = GetPinValueAndType(node->inputs[0]);
				auto [indexValue, indexType] = GetPinValueAndType(node->inputs[1]);
				(void)indexType;

				const ShaderNode* declarationNode = FindMaterialVariableDeclarationNode(nodes_, GetMaterialVariableReferenceName(*node));
				const ShaderPinType outputType = declarationNode && !declarationNode->outputs.empty() ? declarationNode->outputs[0].type : valueType;
				const std::string variableName = GetMaterialVariableReferenceName(*node);
				const std::string indexVar = "node_" + std::to_string(node->id) + "_index";
				const float arraySize = (float)std::max<size_t>(1, declarationNode ? declarationNode->arrayDefaultValues.size() : 1);

				resolvedPinTypes[node->outputs[0].id] = outputType;
				glslBody += "\tint " + indexVar + " = int(clamp(floor(" + indexValue + "), 0.0f, max(" + std::to_string(arraySize) + "f - 1.0f, 0.0f)));\n";
				if (declarationNode && !declarationNode->isUniform)
				{
					glslBody += "\t" + variableName + "[" + indexVar + "] = " + FormatMasterOutput(value, valueType, outputType) + ";\n";
					glslBody += "\t" + GetGLSLTypeString(outputType) + " " + outVar + " = " + variableName + "[" + indexVar + "];\n";
				}
				else
				{
					glslBody += "\t" + GetGLSLTypeString(outputType) + " " + outVar + " = " + FormatMasterOutput(value, valueType, outputType) + ";\n";
				}
			}
			else if (node->typeCategory == "Constants")
			{
				if (node->name == "Float Constant")
				{
					resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Float;
					auto [value, valueType] = GetPinValueAndType(node->outputs[0]);
					(void)valueType;
					glslBody += "\tfloat " + outVar + " = " + value + ";\n";
				}
				else if (node->name == "Vector2 Constant")
				{
					auto [x, typeX] = GetPinValueAndType(node->inputs[0]);
					auto [y, typeY] = GetPinValueAndType(node->inputs[1]);
					(void)typeX;
					(void)typeY;
					resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Vector2;
					glslBody += "\tvec2 " + outVar + " = vec2(" + x + ", " + y + ");\n";
				}
				else if (node->name == "Vector3 Constant")
				{
					auto [x, typeX] = GetPinValueAndType(node->inputs[0]);
					auto [y, typeY] = GetPinValueAndType(node->inputs[1]);
					auto [z, typeZ] = GetPinValueAndType(node->inputs[2]);
					(void)typeX;
					(void)typeY;
					(void)typeZ;
					resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Vector3;
					glslBody += "\tvec3 " + outVar + " = vec3(" + x + ", " + y + ", " + z + ");\n";
				}
				else if (node->name == "Vector4 Constant")
				{
					auto [x, typeX] = GetPinValueAndType(node->inputs[0]);
					auto [y, typeY] = GetPinValueAndType(node->inputs[1]);
					auto [z, typeZ] = GetPinValueAndType(node->inputs[2]);
					auto [w, typeW] = GetPinValueAndType(node->inputs[3]);
					(void)typeX;
					(void)typeY;
					(void)typeZ;
					(void)typeW;
					resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Vector4;
					glslBody += "\tvec4 " + outVar + " = vec4(" + x + ", " + y + ", " + z + ", " + w + ");\n";
				}
			}
			else if (node->typeCategory == "Custom" && node->name == "Custom GLSL")
			{
				resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Any;
				std::string code = node->stringData;
				std::string calculation;
				std::string result = "0.0";
				const size_t pos = code.find("RETURN_RESULT:");
				if (pos != std::string::npos)
				{
					calculation = code.substr(0, pos);
					result = code.substr(pos + 14);
				}
				else
				{
					calculation = code;
				}

				result.erase(std::remove(result.begin(), result.end(), ';'), result.end());
				result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
				result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());
				if (result.empty())
				{
					result = "0.0";
				}

				glslBody += calculation + "\n";
				glslBody += "#define " + outVar + " (" + result + ")\n";
			}
			else if (node->typeCategory == "Flow" && node->name == "If" && node->inputs.size() >= 5 && !node->outputs.empty())
			{
				auto [valueA, typeA] = GetPinValueAndType(node->inputs[0]);
				auto [valueB, typeB] = GetPinValueAndType(node->inputs[1]);
				auto [valueLess, typeLess] = GetPinValueAndType(node->inputs[2]);
				auto [valueEqual, typeEqual] = GetPinValueAndType(node->inputs[3]);
				auto [valueGreater, typeGreater] = GetPinValueAndType(node->inputs[4]);
				(void)typeA;
				(void)typeB;

				ShaderPinType outType = PromoteTypes(typeLess, typeEqual);
				outType = PromoteTypes(outType, typeGreater);
				if (outType == ShaderPinType::Any)
				{
					outType = typeLess != ShaderPinType::Any ? typeLess :
						typeEqual != ShaderPinType::Any ? typeEqual :
						typeGreater;
				}
				if (outType == ShaderPinType::None)
				{
					outType = ShaderPinType::Float;
				}

				resolvedPinTypes[node->outputs[0].id] = outType;
				glslBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = ((" + valueA + ") < (" + valueB + ")) ? " +
					FormatMasterOutput(valueLess, typeLess, outType) + " : (((" + valueA + ") == (" + valueB + ")) ? " +
					FormatMasterOutput(valueEqual, typeEqual, outType) + " : " +
					FormatMasterOutput(valueGreater, typeGreater, outType) + ");\n";
			}
			else if (node->typeCategory == "Flow" && node->name == "If" && node->inputs.size() >= 2 && node->outputs.size() >= 3)
			{
				auto [valueA, typeA] = GetPinValueAndType(node->inputs[0]);
				auto [valueB, typeB] = GetPinValueAndType(node->inputs[1]);
				(void)typeA;
				(void)typeB;

				for (const ShaderPin& outputPin : node->outputs)
				{
					resolvedPinTypes[outputPin.id] = ShaderPinType::Float;
				}

				const std::string greaterVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[0].id);
				const std::string equalVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[1].id);
				const std::string lessVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[2].id);
				glslBody += "\tfloat " + greaterVar + " = ((" + valueA + ") > (" + valueB + ")) ? 1.0f : 0.0f;\n";
				glslBody += "\tfloat " + equalVar + " = ((" + valueA + ") == (" + valueB + ")) ? 1.0f : 0.0f;\n";
				glslBody += "\tfloat " + lessVar + " = ((" + valueA + ") < (" + valueB + ")) ? 1.0f : 0.0f;\n";
			}
			else if (node->typeCategory == "Flow" && node->name == "If Check" && node->inputs.size() >= 4 && !node->outputs.empty())
			{
				auto [valueA, typeA] = GetPinValueAndType(node->inputs[0]);
				auto [valueB, typeB] = GetPinValueAndType(node->inputs[1]);
				auto [valueTrue, typeTrue] = GetPinValueAndType(node->inputs[2]);
				auto [valueFalse, typeFalse] = GetPinValueAndType(node->inputs[3]);
				(void)typeA;
				(void)typeB;

				ShaderPinType outType = PromoteTypes(typeTrue, typeFalse);
				if (outType == ShaderPinType::Any)
				{
					outType = typeTrue != ShaderPinType::Any ? typeTrue : typeFalse;
				}
				if (outType == ShaderPinType::None)
				{
					outType = ShaderPinType::Float;
				}

				resolvedPinTypes[node->outputs[0].id] = outType;
				const std::string comparisonOperator = node->stringData.empty() ? ">" : node->stringData;
				glslBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = ((" + valueA + ") " + comparisonOperator + " (" + valueB + ")) ? " +
					FormatMasterOutput(valueTrue, typeTrue, outType) + " : " + FormatMasterOutput(valueFalse, typeFalse, outType) + ";\n";
			}
			else if (node->typeCategory == "Flow" && node->name == "For Iteration" && node->inputs.size() >= 2 && !node->outputs.empty())
			{
				auto [iterationValue, iterationType] = GetPinValueAndType(node->inputs[0]);
				auto [countValue, countType] = GetPinValueAndType(node->inputs[1]);
				(void)iterationType;
				(void)countType;

				resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Float;
				glslBody += "\tfloat " + outVar + " = clamp(floor(" + iterationValue + "), 0.0f, max(" + countValue + " - 1.0f, 0.0f));\n";
			}
			else if (node->typeCategory == "Functions")
			{
				MaterialFunction materialFunction;
				if (!LoadMaterialFunctionSignature(node->stringData, materialFunction))
				{
					continue;
				}

				const std::string functionName = materialFunction.GetGeneratedFunctionName();
				if (!functionName.empty() && emittedFunctionNames.insert(functionName).second)
				{
					functionDefinitions += materialFunction.GetGeneratedFunctionDefinitions();
					if (!functionDefinitions.empty() && functionDefinitions.back() != '\n')
					{
						functionDefinitions += "\n";
					}
				}

				std::vector<std::string> arguments;
				for (const ShaderPin& inputPin : node->inputs)
				{
					arguments.push_back(GetPinValueAndType(inputPin).first);
				}

				const std::string joinedArguments = [&arguments]()
				{
					std::string result;
					for (size_t index = 0; index < arguments.size(); ++index)
					{
						if (index > 0)
						{
							result += ", ";
						}
						result += arguments[index];
					}
					return result;
				}();

				for (size_t outputIndex = 0; outputIndex < node->outputs.size(); ++outputIndex)
				{
					const ShaderPin& outputPin = node->outputs[outputIndex];
					const MaterialFunctionPinDefinition outputDefinition{
						outputPin.name,
						outputPin.type
					};
					const ShaderPinType outType = outputPin.type;
					const std::string outputVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(outputPin.id);

					resolvedPinTypes[outputPin.id] = outType;
					glslBody += "\t" + GetGLSLTypeString(outType) + " " + outputVar + " = " +
						GetMaterialFunctionOutputFunctionName(functionName, outputDefinition, outputIndex) +
						"(" + joinedArguments + ");\n";
				}
			}
		}

		outCalculation = glslBody;
		outFunctionDefinitions = functionDefinitions;
	};

	std::string vertexCalculation;
	std::string vertexFunctionDefinitions;
	std::unordered_map<int, ShaderPinType> vertexResolvedPinTypes;
	CompileSubGraph({ "World Position Offset" }, vertexCalculation, vertexFunctionDefinitions, vertexResolvedPinTypes);
	outMaterialData->vertexPositionOffset.calculation = vertexCalculation;
	outMaterialData->vertexShaderFunctions = vertexFunctionDefinitions;

	std::string fragmentCalculation;
	std::string fragmentFunctionDefinitions;
	std::unordered_map<int, ShaderPinType> fragmentResolvedPinTypes;
	CompileSubGraph({ "Base Color", "Emissive", "Normal" }, fragmentCalculation, fragmentFunctionDefinitions, fragmentResolvedPinTypes);
	outMaterialData->baseColor.calculation = fragmentCalculation;
	outMaterialData->fragmentShaderFunctions = fragmentFunctionDefinitions;
	auto GetMasterPinValueAndType = [&](const ShaderPin& pin) -> std::pair<std::string, ShaderPinType>
	{
		for (const ShaderLink& link : links_)
		{
			if (link.endPinId != pin.id)
			{
				continue;
			}

			ShaderPin* outputPin = FindPin(link.startPinId);
			ShaderNode* sourceNode = outputPin ? FindNode(outputPin->nodeId) : nullptr;
			if (!outputPin || !sourceNode)
			{
				continue;
			}

			if (sourceNode->typeCategory == "Variables" ||
				sourceNode->typeCategory == "MaterialVariable" ||
				sourceNode->typeCategory == "MaterialVariableGet")
			{
				return { sourceNode->typeCategory == "MaterialVariableGet" ? GetMaterialVariableReferenceName(*sourceNode) : sourceNode->name, outputPin->type };
			}

			const std::unordered_map<int, ShaderPinType>& resolvedPinTypes =
				pin.name == "World Position Offset" ? vertexResolvedPinTypes : fragmentResolvedPinTypes;
			const ShaderPinType actualType = resolvedPinTypes.count(outputPin->id) ? resolvedPinTypes.at(outputPin->id) : outputPin->type;
			return { "node_" + std::to_string(sourceNode->id) + "_out_" + std::to_string(outputPin->id), actualType };
		}

		return GetDefaultValueString(pin);
	};

	for (const ShaderPin& masterInput : master->inputs)
	{
		auto [finalValue, finalType] = GetMasterPinValueAndType(masterInput);
		if (finalType == ShaderPinType::None)
		{
			continue;
		}

		const std::string formattedResult = FormatMasterOutput(finalValue, finalType, masterInput.type);
		if (masterInput.name == "Base Color") outMaterialData->baseColor.result = formattedResult + ";";
		else if (masterInput.name == "Emissive") outMaterialData->emmisiveColor.result = formattedResult + ";";
		else if (masterInput.name == "Normal") outMaterialData->fragmentNormal.result = formattedResult + ";";
		else if (masterInput.name == "World Position Offset") outMaterialData->vertexPositionOffset.result = formattedResult + ";";
	}
}

bool ShaderEditorPanel::BuildActiveMaterialFunction(MaterialFunction& outMaterialFunction)
{
	if (!IsEditingMaterialFunction())
	{
		return false;
	}

	ShaderNode* masterNode = FindNode(masterNodeId_);
	if (!masterNode || masterNode->inputs.empty())
	{
		return false;
	}

	outMaterialFunction.SetAssetPath(currentAssetPath_);
	outMaterialFunction.SetName(std::filesystem::path(currentAssetPath_).filename().generic_string());

	std::vector<ShaderNode*> inputNodes;
	for (ShaderNode& node : nodes_)
	{
		if (node.typeCategory == "FunctionInput")
		{
			inputNodes.push_back(&node);
		}
	}

	std::sort(inputNodes.begin(), inputNodes.end(), [](const ShaderNode* left, const ShaderNode* right)
		{
			return left->id < right->id;
		});

	std::vector<MaterialFunctionPinDefinition> inputDefinitions;
	for (const ShaderNode* inputNode : inputNodes)
	{
		if (!inputNode || inputNode->outputs.empty())
		{
			continue;
		}

		inputDefinitions.push_back({ inputNode->name, inputNode->outputs[0].type });
	}
	outMaterialFunction.SetInputs(inputDefinitions);

	std::vector<MaterialFunctionPinDefinition> outputDefinitions;
	for (const ShaderPin& outputPin : masterNode->inputs)
	{
		outputDefinitions.push_back({ outputPin.name, outputPin.type });
	}
	outMaterialFunction.SetOutputs(outputDefinitions);

	const std::string generatedFunctionName = "mf_" + SanitizeIdentifier(currentAssetPath_);
	outMaterialFunction.SetGeneratedFunctionName(generatedFunctionName);

	auto GetGLSLTypeString = [](ShaderPinType type) -> std::string
	{
		switch (type)
		{
		case ShaderPinType::Float: return "float";
		case ShaderPinType::Vector2: return "vec2";
		case ShaderPinType::Vector3: return "vec3";
		case ShaderPinType::Vector4: return "vec4";
		case ShaderPinType::Vector4i: return "ivec4";
		case ShaderPinType::Matrix4x4: return "mat4";
		case ShaderPinType::Texture: return "sampler2D";
		case ShaderPinType::Any:
		case ShaderPinType::None:
		default:
			return "float";
		}
	};

	auto PromoteTypes = [](ShaderPinType left, ShaderPinType right) -> ShaderPinType
	{
		if (left == right) return left;
		if (left == ShaderPinType::Any) return right;
		if (right == ShaderPinType::Any) return left;
		if (left == ShaderPinType::Float) return right;
		if (right == ShaderPinType::Float) return left;
		return std::max(left, right);
	};

	auto GetDefaultValueString = [&](const ShaderPin& pin) -> std::pair<std::string, ShaderPinType>
	{
		switch (pin.type)
		{
		case ShaderPinType::Vector2:
			if (std::holds_alternative<Vector2>(pin.defaultValue))
			{
				const Vector2 value = std::get<Vector2>(pin.defaultValue);
				return { "vec2(" + std::to_string(value.x) + ", " + std::to_string(value.y) + ")", ShaderPinType::Vector2 };
			}
			break;
		case ShaderPinType::Vector3:
			if (std::holds_alternative<Vector3>(pin.defaultValue))
			{
				const Vector3 value = std::get<Vector3>(pin.defaultValue);
				return { "vec3(" + std::to_string(value.x) + ", " + std::to_string(value.y) + ", " + std::to_string(value.z) + ")", ShaderPinType::Vector3 };
			}
			break;
		case ShaderPinType::Vector4:
			if (std::holds_alternative<Vector4>(pin.defaultValue))
			{
				const Vector4 value = std::get<Vector4>(pin.defaultValue);
				return { "vec4(" + std::to_string(value.x) + ", " + std::to_string(value.y) + ", " + std::to_string(value.z) + ", " + std::to_string(value.w) + ")", ShaderPinType::Vector4 };
			}
			break;
		case ShaderPinType::Any:
		case ShaderPinType::Float:
		case ShaderPinType::None:
		default:
			if (std::holds_alternative<float>(pin.defaultValue))
			{
				return { std::to_string(std::get<float>(pin.defaultValue)) + "f", ShaderPinType::Float };
			}
			break;
		}

		return { "0.0f", ShaderPinType::Float };
	};

	auto GetGLSLFuncName = [](const std::string& nodeName) -> std::string
	{
		if (nodeName == "Sine") return "sin";
		if (nodeName == "Cosine") return "cos";
		if (nodeName == "Tangent") return "tan";
		if (nodeName == "InverseSqrt") return "inversesqrt";
		if (nodeName == "FloatBitsToInt") return "floatBitsToInt";
		if (nodeName == "FloatBitsToUint") return "floatBitsToUint";
		if (nodeName == "IntBitsToFloat") return "intBitsToFloat";
		if (nodeName == "UintBitsToFloat") return "uintBitsToFloat";
		if (nodeName == "MatrixCompMult") return "matrixCompMult";
		if (nodeName == "OuterProduct") return "outerProduct";
		if (nodeName == "RoundEven") return "roundEven";
		if (nodeName == "IsNan") return "isnan";
		if (nodeName == "IsInf") return "isinf";

		std::string glslName = nodeName;
		glslName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(glslName[0])));
		return glslName;
	};

	auto GetMaskComponentExpression = [](const std::string& value, ShaderPinType inputType, size_t componentIndex) -> std::string
	{
		static const char* componentNames[] = { "x", "y", "z", "w" };
		if (componentIndex >= 4)
		{
			return "0.0f";
		}

		switch (inputType)
		{
		case ShaderPinType::Float:
			return componentIndex == 0 ? value : "0.0f";
		case ShaderPinType::Vector2:
			return componentIndex < 2 ? value + "." + componentNames[componentIndex] : "0.0f";
		case ShaderPinType::Vector3:
			return componentIndex < 3 ? value + "." + componentNames[componentIndex] : "0.0f";
		case ShaderPinType::Vector4:
			return value + "." + componentNames[componentIndex];
		case ShaderPinType::Vector4i:
			return "float(" + value + "." + componentNames[componentIndex] + ")";
		case ShaderPinType::Any:
		case ShaderPinType::Matrix4x4:
		case ShaderPinType::Texture:
		case ShaderPinType::None:
		default:
			return componentIndex == 0 ? value : "0.0f";
		}
	};

	auto FormatOutputValue = [](const std::string& value, ShaderPinType actualType, ShaderPinType expectedType) -> std::string
	{
		if (actualType == expectedType || actualType == ShaderPinType::Any)
		{
			return value;
		}

		if (expectedType == ShaderPinType::Vector4 && actualType == ShaderPinType::Vector3) return "vec4(" + value + ", 1.0f)";
		if (expectedType == ShaderPinType::Vector4 && actualType == ShaderPinType::Float) return "vec4(" + value + ")";
		if (expectedType == ShaderPinType::Vector3 && actualType == ShaderPinType::Vector4) return value + ".xyz";
		if (expectedType == ShaderPinType::Vector3 && actualType == ShaderPinType::Float) return "vec3(" + value + ")";
		if (expectedType == ShaderPinType::Vector2 && actualType == ShaderPinType::Float) return "vec2(" + value + ")";
		return value;
	};

	std::vector<ShaderNode*> executionOrder;
	std::unordered_set<int> visitedNodes;
	std::unordered_map<int, ShaderPinType> resolvedPinTypes;
	std::unordered_set<std::string> emittedFunctionNames;

	std::function<void(int)> backtrace = [&](int nodeId)
	{
		if (visitedNodes.find(nodeId) != visitedNodes.end())
		{
			return;
		}

		visitedNodes.insert(nodeId);
		ShaderNode* node = FindNode(nodeId);
		if (!node)
		{
			return;
		}

		for (const ShaderPin& inputPin : node->inputs)
		{
			for (const ShaderLink& link : links_)
			{
				if (link.endPinId == inputPin.id)
				{
					if (ShaderPin* connectedOutput = FindPin(link.startPinId))
					{
						backtrace(connectedOutput->nodeId);
					}
				}
			}
		}

		if (nodeId != masterNodeId_)
		{
			executionOrder.push_back(node);
		}
	};

	for (const ShaderPin& masterOutputPin : masterNode->inputs)
	{
		for (const ShaderLink& link : links_)
		{
			if (link.endPinId == masterOutputPin.id)
			{
				if (ShaderPin* connectedOutput = FindPin(link.startPinId))
				{
					backtrace(connectedOutput->nodeId);
				}
			}
		}
	}

	auto GetPinValueAndType = [&](const ShaderPin& pin) -> std::pair<std::string, ShaderPinType>
	{
		for (const ShaderLink& link : links_)
		{
			if (link.endPinId != pin.id)
			{
				continue;
			}

			ShaderPin* outputPin = FindPin(link.startPinId);
			ShaderNode* sourceNode = outputPin ? FindNode(outputPin->nodeId) : nullptr;
			if (!outputPin || !sourceNode)
			{
				continue;
			}

			if (sourceNode->typeCategory == "FunctionInput")
			{
				return { SanitizeIdentifier(sourceNode->name), outputPin->type };
			}

			if (sourceNode->typeCategory == "Variables")
			{
				return { sourceNode->name, outputPin->type };
			}

			const ShaderPinType actualType = resolvedPinTypes.count(outputPin->id) ? resolvedPinTypes.at(outputPin->id) : outputPin->type;
			return { "node_" + std::to_string(sourceNode->id) + "_out_" + std::to_string(outputPin->id), actualType };
		}

		return GetDefaultValueString(pin);
	};

	std::string nestedFunctionDefinitions;
	std::string functionBody = "\t// --- Generated Material Function Calculations ---\n";

	for (ShaderNode* node : executionOrder)
	{
		if (!node || node->typeCategory == "FunctionInput" || node->typeCategory == "Variables")
		{
			continue;
		}

		functionBody += "\t// Node: " + node->name + "\n";
		const std::string outVar = node->outputs.empty() ? "" : "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[0].id);

		if (node->typeCategory == "Math" || node->typeCategory == "Trigonometry" ||
			node->typeCategory == "Exponential" || node->typeCategory == "Geometric" ||
			node->typeCategory == "Matrix")
		{
			if (node->name == "Mask")
			{
				auto [value, type] = GetPinValueAndType(node->inputs[0]);
				for (size_t outputIndex = 0; outputIndex < node->outputs.size(); ++outputIndex)
				{
					const ShaderPin& outputPin = node->outputs[outputIndex];
					const std::string outputVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(outputPin.id);
					resolvedPinTypes[outputPin.id] = ShaderPinType::Float;
					functionBody += "\tfloat " + outputVar + " = " + GetMaskComponentExpression(value, type, outputIndex) + ";\n";
				}
			}
			else if (node->name == "Add" || node->name == "Subtract" || node->name == "Multiply" || node->name == "Divide" || node->name == "Modulo")
			{
				auto [valueA, typeA] = GetPinValueAndType(node->inputs[0]);
				auto [valueB, typeB] = GetPinValueAndType(node->inputs[1]);
				const ShaderPinType outType = PromoteTypes(typeA, typeB);
				resolvedPinTypes[node->outputs[0].id] = outType;
				if (node->name == "Modulo")
				{
					functionBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = mod(" + valueA + ", " + valueB + ");\n";
				}
				else
				{
					const std::string op = node->name == "Add" ? "+" : node->name == "Subtract" ? "-" : node->name == "Multiply" ? "*" : "/";
					functionBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = " + valueA + " " + op + " " + valueB + ";\n";
				}
			}
			else if (node->name == "Modf" || node->name == "Frexp")
			{
				auto [valueX, typeX] = GetPinValueAndType(node->inputs[0]);
				resolvedPinTypes[node->outputs[0].id] = typeX;
				resolvedPinTypes[node->outputs[1].id] = typeX;
				const std::string outVar2 = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[1].id);
				const std::string funcName = GetGLSLFuncName(node->name);
				functionBody += "\t" + GetGLSLTypeString(typeX) + " " + outVar2 + ";\n";
				functionBody += "\t" + GetGLSLTypeString(typeX) + " " + outVar + " = " + funcName + "(" + valueX + ", " + outVar2 + ");\n";
			}
			else
			{
				const std::string funcName = GetGLSLFuncName(node->name);
				if (node->inputs.size() == 1)
				{
					auto [value, type] = GetPinValueAndType(node->inputs[0]);
					ShaderPinType outType = type;
					if (node->name == "Length" || node->name == "Determinant" || node->name == "IsNan" || node->name == "IsInf")
					{
						outType = ShaderPinType::Float;
					}
					resolvedPinTypes[node->outputs[0].id] = outType;
					functionBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = " + funcName + "(" + value + ");\n";
				}
				else if (node->inputs.size() == 2)
				{
					auto [valueA, typeA] = GetPinValueAndType(node->inputs[0]);
					auto [valueB, typeB] = GetPinValueAndType(node->inputs[1]);
					ShaderPinType outType = PromoteTypes(typeA, typeB);
					if (node->name == "Distance" || node->name == "Dot") outType = ShaderPinType::Float;
					if (node->name == "Cross") outType = ShaderPinType::Vector3;
					if (node->name == "OuterProduct") outType = ShaderPinType::Matrix4x4;
					resolvedPinTypes[node->outputs[0].id] = outType;
					functionBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = " + funcName + "(" + valueA + ", " + valueB + ");\n";
				}
				else if (node->inputs.size() == 3)
				{
					auto [valueA, typeA] = GetPinValueAndType(node->inputs[0]);
					auto [valueB, typeB] = GetPinValueAndType(node->inputs[1]);
					auto [valueC, typeC] = GetPinValueAndType(node->inputs[2]);
					ShaderPinType outType = PromoteTypes(typeA, typeB);
					outType = PromoteTypes(outType, typeC);
					resolvedPinTypes[node->outputs[0].id] = outType;
					functionBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = " + funcName + "(" + valueA + ", " + valueB + ", " + valueC + ");\n";
				}
			}
		}
		else if (node->typeCategory == "Texture" && node->name == "Texture Sample")
		{
			auto [uvValue, uvType] = GetPinValueAndType(node->inputs[0]);
			(void)uvType;
			resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Vector4;
			const std::string samplerName = node->stringData.empty() ? "diffuseTexture" : node->stringData;
			functionBody += "\tvec4 " + outVar + " = texture(" + samplerName + ", " + uvValue + ");\n";
		}
		else if (node->typeCategory == "Constants")
		{
			if (node->name == "Float Constant")
			{
				resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Float;
				functionBody += "\tfloat " + outVar + " = " + GetPinValueAndType(node->outputs[0]).first + ";\n";
			}
			else if (node->name == "Vector2 Constant")
			{
				resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Vector2;
				functionBody += "\tvec2 " + outVar + " = vec2(" + GetPinValueAndType(node->inputs[0]).first + ", " + GetPinValueAndType(node->inputs[1]).first + ");\n";
			}
			else if (node->name == "Vector3 Constant")
			{
				resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Vector3;
				functionBody += "\tvec3 " + outVar + " = vec3(" + GetPinValueAndType(node->inputs[0]).first + ", " + GetPinValueAndType(node->inputs[1]).first + ", " + GetPinValueAndType(node->inputs[2]).first + ");\n";
			}
			else if (node->name == "Vector4 Constant")
			{
				resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Vector4;
				functionBody += "\tvec4 " + outVar + " = vec4(" + GetPinValueAndType(node->inputs[0]).first + ", " + GetPinValueAndType(node->inputs[1]).first + ", " + GetPinValueAndType(node->inputs[2]).first + ", " + GetPinValueAndType(node->inputs[3]).first + ");\n";
			}
		}
		else if (node->typeCategory == "Custom" && node->name == "Custom GLSL")
		{
			resolvedPinTypes[node->outputs[0].id] = ShaderPinType::Any;
			std::string code = node->stringData;
			std::string calculation;
			std::string result = "0.0";
			const size_t pos = code.find("RETURN_RESULT:");
			if (pos != std::string::npos)
			{
				calculation = code.substr(0, pos);
				result = code.substr(pos + 14);
			}
			else
			{
				calculation = code;
			}

			result.erase(std::remove(result.begin(), result.end(), ';'), result.end());
			result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
			result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());
			if (result.empty())
			{
				result = "0.0";
			}

			functionBody += calculation + "\n";
			functionBody += "#define " + outVar + " (" + result + ")\n";
		}
		else if (node->typeCategory == "Flow" && node->name == "If" && node->inputs.size() >= 5 && !node->outputs.empty())
		{
			auto [valueA, typeA] = GetPinValueAndType(node->inputs[0]);
			auto [valueB, typeB] = GetPinValueAndType(node->inputs[1]);
			auto [valueLess, typeLess] = GetPinValueAndType(node->inputs[2]);
			auto [valueEqual, typeEqual] = GetPinValueAndType(node->inputs[3]);
			auto [valueGreater, typeGreater] = GetPinValueAndType(node->inputs[4]);
			(void)typeA;
			(void)typeB;

			ShaderPinType outType = PromoteTypes(typeLess, typeEqual);
			outType = PromoteTypes(outType, typeGreater);
			if (outType == ShaderPinType::Any)
			{
				outType = typeLess != ShaderPinType::Any ? typeLess :
					typeEqual != ShaderPinType::Any ? typeEqual :
					typeGreater;
			}
			if (outType == ShaderPinType::None)
			{
				outType = ShaderPinType::Float;
			}

			resolvedPinTypes[node->outputs[0].id] = outType;
			functionBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = ((" + valueA + ") < (" + valueB + ")) ? " +
				FormatOutputValue(valueLess, typeLess, outType) + " : (((" + valueA + ") == (" + valueB + ")) ? " +
				FormatOutputValue(valueEqual, typeEqual, outType) + " : " +
				FormatOutputValue(valueGreater, typeGreater, outType) + ");\n";
		}
		else if (node->typeCategory == "Flow" && node->name == "If" && node->inputs.size() >= 2 && node->outputs.size() >= 3)
		{
			auto [valueA, typeA] = GetPinValueAndType(node->inputs[0]);
			auto [valueB, typeB] = GetPinValueAndType(node->inputs[1]);
			(void)typeA;
			(void)typeB;

			for (const ShaderPin& outputPin : node->outputs)
			{
				resolvedPinTypes[outputPin.id] = ShaderPinType::Float;
			}

			const std::string greaterVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[0].id);
			const std::string equalVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[1].id);
			const std::string lessVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(node->outputs[2].id);
			functionBody += "\tfloat " + greaterVar + " = ((" + valueA + ") > (" + valueB + ")) ? 1.0f : 0.0f;\n";
			functionBody += "\tfloat " + equalVar + " = ((" + valueA + ") == (" + valueB + ")) ? 1.0f : 0.0f;\n";
			functionBody += "\tfloat " + lessVar + " = ((" + valueA + ") < (" + valueB + ")) ? 1.0f : 0.0f;\n";
		}
		else if (node->typeCategory == "Flow" && node->name == "If Check" && node->inputs.size() >= 4 && !node->outputs.empty())
		{
			auto [valueA, typeA] = GetPinValueAndType(node->inputs[0]);
			auto [valueB, typeB] = GetPinValueAndType(node->inputs[1]);
			auto [valueTrue, typeTrue] = GetPinValueAndType(node->inputs[2]);
			auto [valueFalse, typeFalse] = GetPinValueAndType(node->inputs[3]);
			(void)typeA;
			(void)typeB;

			ShaderPinType outType = PromoteTypes(typeTrue, typeFalse);
			if (outType == ShaderPinType::Any)
			{
				outType = typeTrue != ShaderPinType::Any ? typeTrue : typeFalse;
			}
			if (outType == ShaderPinType::None)
			{
				outType = ShaderPinType::Float;
			}

			resolvedPinTypes[node->outputs[0].id] = outType;
			const std::string comparisonOperator = node->stringData.empty() ? ">" : node->stringData;
			functionBody += "\t" + GetGLSLTypeString(outType) + " " + outVar + " = ((" + valueA + ") " + comparisonOperator + " (" + valueB + ")) ? " +
				FormatOutputValue(valueTrue, typeTrue, outType) + " : " + FormatOutputValue(valueFalse, typeFalse, outType) + ";\n";
		}
		else if (node->typeCategory == "Functions")
		{
			MaterialFunction materialFunction;
			if (!LoadMaterialFunctionSignature(node->stringData, materialFunction))
			{
				continue;
			}

			const std::string functionName = materialFunction.GetGeneratedFunctionName();
			if (!functionName.empty() && emittedFunctionNames.insert(functionName).second)
			{
				nestedFunctionDefinitions += materialFunction.GetGeneratedFunctionDefinitions();
				if (!nestedFunctionDefinitions.empty() && nestedFunctionDefinitions.back() != '\n')
				{
					nestedFunctionDefinitions += "\n";
				}
			}

			std::string joinedArguments;
			for (size_t inputIndex = 0; inputIndex < node->inputs.size(); ++inputIndex)
			{
				if (inputIndex > 0)
				{
					joinedArguments += ", ";
				}
				joinedArguments += GetPinValueAndType(node->inputs[inputIndex]).first;
			}

			for (size_t outputIndex = 0; outputIndex < node->outputs.size(); ++outputIndex)
			{
				const ShaderPin& outputPin = node->outputs[outputIndex];
				const ShaderPinType outType = outputPin.type;
				const std::string outputVar = "node_" + std::to_string(node->id) + "_out_" + std::to_string(outputPin.id);
				const MaterialFunctionPinDefinition outputDefinition{
					outputPin.name,
					outputPin.type
				};

				resolvedPinTypes[outputPin.id] = outType;
				functionBody += "\t" + GetGLSLTypeString(outType) + " " + outputVar + " = " +
					GetMaterialFunctionOutputFunctionName(functionName, outputDefinition, outputIndex) +
					"(" + joinedArguments + ");\n";
			}
		}
	}

	std::string functionDefinition = nestedFunctionDefinitions;
	for (size_t outputIndex = 0; outputIndex < masterNode->inputs.size(); ++outputIndex)
	{
		const ShaderPin& masterOutputPin = masterNode->inputs[outputIndex];
		const auto [finalValue, finalType] = GetPinValueAndType(masterOutputPin);
		const std::string formattedResult = FormatOutputValue(finalValue, finalType, masterOutputPin.type);
		const MaterialFunctionPinDefinition outputDefinition{
			masterOutputPin.name,
			masterOutputPin.type
		};

		functionDefinition += GetGLSLTypeString(masterOutputPin.type) + " " +
			GetMaterialFunctionOutputFunctionName(generatedFunctionName, outputDefinition, outputIndex) + "(";
		for (size_t inputIndex = 0; inputIndex < inputDefinitions.size(); ++inputIndex)
		{
			if (inputIndex > 0)
			{
				functionDefinition += ", ";
			}

			functionDefinition += GetGLSLTypeString(inputDefinitions[inputIndex].type) + " " + SanitizeIdentifier(inputDefinitions[inputIndex].name);
		}
		functionDefinition += ")\n{\n";
		functionDefinition += functionBody;
		functionDefinition += "\treturn " + formattedResult + ";\n";
		functionDefinition += "}\n";
	}

	outMaterialFunction.SetGeneratedFunctionDefinitions(functionDefinition);
	return true;
}
