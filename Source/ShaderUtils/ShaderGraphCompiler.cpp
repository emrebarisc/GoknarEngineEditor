#include "ShaderGraphCompiler.h"

#include "Goknar/Materials/MaterialFunctionSerializer.h"
#include "Goknar/Renderer/ShaderTypes.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace
{
	constexpr const char* MATERIAL_NODE_METADATA_PREFIX = "// GOKNAR_MATERIAL_NODE|";

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

	std::string BuildTextureSamplerBaseName(const std::string& texturePath)
	{
		std::string baseName = std::filesystem::path(texturePath).stem().generic_string();
		if (baseName.empty())
		{
			baseName = "textureSample";
		}

		return SanitizeIdentifier(baseName);
	}

	bool IsMaterialValueType(ShaderPinType type)
	{
		return type == ShaderPinType::Float || type == ShaderPinType::Vector2 || type == ShaderPinType::Vector3 || type == ShaderPinType::Vector4;
	}

	bool IsMaterialVariableDeclarationCategory(const std::string& category)
	{
		return category == "MaterialVariable" || category == "MaterialVariableArray";
	}

	bool IsMaterialVariableAccessorCategory(const std::string& category)
	{
		return category == "MaterialVariableGet" || category == "MaterialVariableSet" || category == "MaterialVariableArrayGet" || category == "MaterialVariableArraySet";
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

	void EnsureValueMatchesType(ShaderValue& value, ShaderPinType type)
	{
		switch (type)
		{
		case ShaderPinType::Vector2:
			if (!std::holds_alternative<Vector2>(value)) value = Vector2(0.f);
			return;
		case ShaderPinType::Vector3:
			if (!std::holds_alternative<Vector3>(value)) value = Vector3(0.f);
			return;
		case ShaderPinType::Vector4:
			if (!std::holds_alternative<Vector4>(value)) value = Vector4(0.f);
			return;
		case ShaderPinType::Float:
		default:
			if (!std::holds_alternative<float>(value)) value = 0.0f;
			return;
		}
	}

	void EnsureArrayDefaultsMatchNode(ShaderNode& node)
	{
		if (node.typeCategory != "MaterialVariableArray")
		{
			return;
		}

		ShaderPinType elementType = node.outputs.empty() ? ShaderPinType::Float : node.outputs[0].type;
		if (!IsMaterialValueType(elementType))
		{
			elementType = ShaderPinType::Float;
			if (!node.outputs.empty()) node.outputs[0].type = elementType;
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

	std::string BuildMaterialNodeMetadataLine(const ShaderNode& node)
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

		std::string valuesText;
		if (isArray)
		{
			for (size_t index = 0; index < node.arrayDefaultValues.size(); ++index)
			{
				if (index > 0) valuesText += ";";
				valuesText += ShaderValueToMetadataString(node.arrayDefaultValues[index], valueType);
			}
		}
		else
		{
			const ShaderValue defaultValue = node.outputs.empty() ? GetDefaultValueForPinType(valueType) : node.outputs[0].defaultValue;
			valuesText = ShaderValueToMetadataString(defaultValue, valueType);
		}

		std::ostringstream stream;
		stream << MATERIAL_NODE_METADATA_PREFIX
			<< "kind=" << (isArray ? "Array" : "Variable")
			<< "|name=" << node.name
			<< "|type=" << ShaderPinTypeToString(valueType)
			<< "|storage=" << (node.isUniform ? "Uniform" : "Global")
			<< "|count=" << (isArray ? node.arrayDefaultValues.size() : 1)
			<< "|values=" << valuesText;
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

		const char* glslType = valueType == ShaderPinType::Float ? "float" : valueType == ShaderPinType::Vector2 ? "vec2" : valueType == ShaderPinType::Vector3 ? "vec3" : "vec4";

		std::string declaration = BuildMaterialNodeMetadataLine(node) + "\n";
		if (node.isUniform)
		{
			declaration += "uniform ";
			declaration += glslType;
			declaration += " ";
			declaration += node.name;
			if (isArray) declaration += "[" + std::to_string(node.arrayDefaultValues.size()) + "]";
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
				if (index > 0) declaration += ", ";
				declaration += valueType == ShaderPinType::Float ? ShaderValueToMetadataString(node.arrayDefaultValues[index], valueType) + "f" : std::string(glslType) + "(" + ShaderValueToMetadataString(node.arrayDefaultValues[index], valueType) + ")";
			}
			declaration += ");\n";
		}
		else
		{
			const ShaderValue defaultValue = node.outputs.empty() ? GetDefaultValueForPinType(valueType) : node.outputs[0].defaultValue;
			declaration += " = ";
			declaration += valueType == ShaderPinType::Float ? ShaderValueToMetadataString(defaultValue, valueType) + "f" : std::string(glslType) + "(" + ShaderValueToMetadataString(defaultValue, valueType) + ")";
			declaration += ";\n";
		}

		return declaration;
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
			if (!IsMaterialVariableDeclarationCategory(node.typeCategory)) continue;
			if (SanitizeIdentifier(node.name) == referenceName) return &node;
		}
		return nullptr;
	}

	const ShaderNode* FindNode(const std::vector<ShaderNode>& nodes, int nodeId)
	{
		for (const ShaderNode& node : nodes)
		{
			if (node.id == nodeId) return &node;
		}
		return nullptr;
	}

	const ShaderEditorTextureInfo* GetTextureInfoForNode(const ShaderGraphCompileInput& input, const ShaderNode& textureNode)
	{
		if (!input.textures)
		{
			return nullptr;
		}

		for (const ShaderEditorTextureInfo& textureInfo : *input.textures)
		{
			if (textureInfo.path == textureNode.stringData && textureInfo.useTextureAtlas == textureNode.useTextureAtlas)
			{
				return &textureInfo;
			}
		}
		return nullptr;
	}

	std::string GetTextureSamplerNameForNode(const ShaderGraphCompileInput& input, const ShaderNode& textureNode)
	{
		const ShaderEditorTextureInfo* textureInfo = GetTextureInfoForNode(input, textureNode);
		return textureInfo ? textureInfo->name : "";
	}

	std::string GetTextureSampleUVExpression(const ShaderGraphCompileInput& input, const ShaderNode& textureNode, const std::string& uvExpression, ShaderPinType uvType)
	{
		std::string normalizedUVExpression = uvExpression;
		if (uvType == ShaderPinType::Float)
		{
			normalizedUVExpression = "vec2(" + uvExpression + ")";
		}
		else if (uvType == ShaderPinType::Vector3 || uvType == ShaderPinType::Vector4)
		{
			normalizedUVExpression = "(" + uvExpression + ").xy";
		}

		const ShaderEditorTextureInfo* textureInfo = GetTextureInfoForNode(input, textureNode);
		if (!textureInfo || !textureInfo->useTextureAtlas)
		{
			return normalizedUVExpression;
		}

		const std::string transformUniformName = textureInfo->name + "_UVTransform";
		const std::string wrappedUV = "vec2(fract((" + normalizedUVExpression + ").x), fract((" + normalizedUVExpression + ").y))";
		return "(" + wrappedUV + " * " + transformUniformName + ".xy + " + transformUniformName + ".zw)";
	}

	bool LoadMaterialFunctionSignatureFromAsset(const std::string& assetPath, MaterialFunction& outMaterialFunction)
	{
		return MaterialFunctionSerializer::Deserialize(assetPath, outMaterialFunction);
	}

	std::string GetMaterialFunctionOutputFunctionName(const std::string& baseFunctionName, const MaterialFunctionPinDefinition& outputDefinition, size_t outputIndex)
	{
		return baseFunctionName + "_out_" + std::to_string(outputIndex) + "_" + SanitizeIdentifier(outputDefinition.name);
	}
}

void ShaderGraphCompiler::CompileMaterial(const ShaderGraphCompileInput& input, MaterialInitializationData* outMaterialData) const
{

	if (!outMaterialData || !input.nodes || !input.links || !input.textures)
	{
		return;
	}

	const std::vector<ShaderNode>& nodes = *input.nodes;
	const std::vector<ShaderLink>& links = *input.links;

	outMaterialData->baseColor.calculation = "";
	outMaterialData->baseColor.result = "";
	outMaterialData->emisiveColor.calculation = "";
	outMaterialData->emisiveColor.result = "";
	outMaterialData->ambientOcclusion.calculation = "";
	outMaterialData->ambientOcclusion.result = "";
	outMaterialData->metallic.calculation = "";
	outMaterialData->metallic.result = "";
	outMaterialData->roughness.calculation = "";
	outMaterialData->roughness.result = "";
	outMaterialData->fragmentNormal.calculation = "";
	outMaterialData->fragmentNormal.result = "";
	outMaterialData->fragmentNormalIsTangentSpace = false;
	outMaterialData->vertexNormal.calculation = "";
	outMaterialData->vertexNormal.result = "";
	outMaterialData->vertexPositionOffset.calculation = "";
	outMaterialData->vertexPositionOffset.result = "";
	outMaterialData->vertexShaderFunctions = "";
	outMaterialData->fragmentShaderFunctions = "";
	outMaterialData->vertexShaderUniforms = "";
	outMaterialData->fragmentShaderUniforms = "";

	const ShaderNode* master = FindNode(nodes, input.masterNodeId);
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
			if (left == ShaderPinType::None) return right;
			if (right == ShaderPinType::None) return left;
			if (left == ShaderPinType::Any) return right;
			if (right == ShaderPinType::Any) return left;
			if (left == ShaderPinType::Float) return right;
			if (right == ShaderPinType::Float) return left;
			return std::max(left, right);
		};

	auto FormatFloat = [](float value) -> std::string
		{
			if (!std::isfinite(value))
			{
				value = 0.0f;
			}

			std::ostringstream stream;
			stream << std::fixed << std::setprecision(6) << value;
			std::string text = stream.str();
			while (!text.empty() && text.back() == '0')
			{
				text.pop_back();
			}
			if (!text.empty() && text.back() == '.')
			{
				text += '0';
			}
			if (text.empty() || text == "-0.0")
			{
				text = "0.0";
			}
			if (text.find('.') == std::string::npos)
			{
				text += ".0";
			}
			return text;
		};

	auto GetDefaultValueString = [&](const ShaderPin& pin) -> std::pair<std::string, ShaderPinType>
		{
			switch (pin.type)
			{
			case ShaderPinType::Vector2:
				if (std::holds_alternative<Vector2>(pin.defaultValue))
				{
					const Vector2 value = std::get<Vector2>(pin.defaultValue);
					return { "vec2(" + FormatFloat(value.x) + ", " + FormatFloat(value.y) + ")", ShaderPinType::Vector2 };
				}
				break;
			case ShaderPinType::Vector3:
				if (std::holds_alternative<Vector3>(pin.defaultValue))
				{
					const Vector3 value = std::get<Vector3>(pin.defaultValue);
					return { "vec3(" + FormatFloat(value.x) + ", " + FormatFloat(value.y) + ", " + FormatFloat(value.z) + ")", ShaderPinType::Vector3 };
				}
				break;
			case ShaderPinType::Vector4:
				if (std::holds_alternative<Vector4>(pin.defaultValue))
				{
					const Vector4 value = std::get<Vector4>(pin.defaultValue);
					return { "vec4(" + FormatFloat(value.x) + ", " + FormatFloat(value.y) + ", " + FormatFloat(value.z) + ", " + FormatFloat(value.w) + ")", ShaderPinType::Vector4 };
				}
				break;
			case ShaderPinType::Any:
			case ShaderPinType::Float:
			case ShaderPinType::None:
			default:
				if (std::holds_alternative<float>(pin.defaultValue))
				{
					return { FormatFloat(std::get<float>(pin.defaultValue)), ShaderPinType::Float };
				}
				break;
			}

			return { "0.0", ShaderPinType::Float };
		};

	auto GetGLSLFuncName = [](const std::string& nodeName) -> std::string
		{
			if (nodeName == "Sine") return "sin";
			if (nodeName == "Cosine") return "cos";
			if (nodeName == "Tangent") return "tan";
			if (nodeName == "Asin") return "asin";
			if (nodeName == "Acos") return "acos";
			if (nodeName == "Atan") return "atan";
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
			if (!glslName.empty())
			{
				glslName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(glslName[0])));
			}
			return glslName;
		};

	auto GetMaskComponentExpression = [](const std::string& value, ShaderPinType inputType, size_t componentIndex) -> std::string
		{
			static const char* componentNames[] = { "x", "y", "z", "w" };
			if (componentIndex >= 4)
			{
				return "0.0";
			}

			switch (inputType)
			{
			case ShaderPinType::Float:
				return componentIndex == 0 ? value : "0.0";
			case ShaderPinType::Vector2:
				return componentIndex < 2 ? "(" + value + ")." + componentNames[componentIndex] : "0.0";
			case ShaderPinType::Vector3:
				return componentIndex < 3 ? "(" + value + ")." + componentNames[componentIndex] : "0.0";
			case ShaderPinType::Vector4:
				return "(" + value + ")." + componentNames[componentIndex];
			case ShaderPinType::Vector4i:
				return "float((" + value + ")." + componentNames[componentIndex] + ")";
			case ShaderPinType::Any:
			case ShaderPinType::Matrix4x4:
			case ShaderPinType::Texture:
			case ShaderPinType::None:
			default:
				return componentIndex == 0 ? value : "0.0";
			}
		};

	auto FormatOutputValue = [](const std::string& value, ShaderPinType actualType, ShaderPinType expectedType) -> std::string
		{
			if (actualType == expectedType || actualType == ShaderPinType::Any)
			{
				return value;
			}

			if (expectedType == ShaderPinType::Vector4 && actualType == ShaderPinType::Vector3) return "vec4(" + value + ", 1.0)";
			if (expectedType == ShaderPinType::Vector4 && actualType == ShaderPinType::Float) return "vec4(" + value + ")";
			if (expectedType == ShaderPinType::Vector3 && actualType == ShaderPinType::Vector4) return "(" + value + ").xyz";
			if (expectedType == ShaderPinType::Vector3 && actualType == ShaderPinType::Float) return "vec3(" + value + ")";
			if (expectedType == ShaderPinType::Vector2 && actualType == ShaderPinType::Float) return "vec2(" + value + ")";
			return value;
		};

	std::string materialVariableDeclarations;
	for (const ShaderNode& node : nodes)
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

	std::unordered_map<int, const ShaderNode*> nodesById;
	std::unordered_map<int, const ShaderPin*> pinsById;
	std::unordered_map<int, const ShaderNode*> nodeByPinId;
	std::unordered_map<int, std::vector<int>> inputPinToOutputPinCandidates;
	std::unordered_map<int, int> inputPinToOutputPin;
	std::unordered_map<int, int> outputPinUseCount;

	for (const ShaderNode& node : nodes)
	{
		nodesById[node.id] = &node;
		for (const ShaderPin& pin : node.inputs)
		{
			pinsById[pin.id] = &pin;
			nodeByPinId[pin.id] = &node;
		}
		for (const ShaderPin& pin : node.outputs)
		{
			pinsById[pin.id] = &pin;
			nodeByPinId[pin.id] = &node;
		}
	}

	for (const ShaderLink& link : links)
	{
		const auto startPinIterator = pinsById.find(link.startPinId);
		const auto endPinIterator = pinsById.find(link.endPinId);
		if (startPinIterator == pinsById.end() || endPinIterator == pinsById.end())
		{
			continue;
		}
		if (startPinIterator->second->kind != ShaderPinKind::Output || endPinIterator->second->kind != ShaderPinKind::Input)
		{
			continue;
		}

		inputPinToOutputPinCandidates[link.endPinId].push_back(link.startPinId);
	}

	std::function<int(int, std::unordered_set<int>&)> GetOutputDependencyDepth;
	std::function<int(int, std::unordered_set<int>&)> GetInputDependencyDepth;

	GetInputDependencyDepth = [&](int inputPinId, std::unordered_set<int>& visitingOutputPins) -> int
		{
			const auto candidatesIterator = inputPinToOutputPinCandidates.find(inputPinId);
			if (candidatesIterator == inputPinToOutputPinCandidates.end())
			{
				return 0;
			}

			int depth = 0;
			for (int outputPinId : candidatesIterator->second)
			{
				depth = std::max(depth, GetOutputDependencyDepth(outputPinId, visitingOutputPins));
			}
			return depth;
		};

	GetOutputDependencyDepth = [&](int outputPinId, std::unordered_set<int>& visitingOutputPins) -> int
		{
			if (visitingOutputPins.find(outputPinId) != visitingOutputPins.end())
			{
				return 0;
			}

			const auto nodeIterator = nodeByPinId.find(outputPinId);
			if (nodeIterator == nodeByPinId.end())
			{
				return 0;
			}

			visitingOutputPins.insert(outputPinId);
			int depth = 1;
			for (const ShaderPin& inputPin : nodeIterator->second->inputs)
			{
				depth = std::max(depth, 1 + GetInputDependencyDepth(inputPin.id, visitingOutputPins));
			}
			visitingOutputPins.erase(outputPinId);
			return depth;
		};

	for (const auto& candidatePair : inputPinToOutputPinCandidates)
	{
		const std::vector<int>& candidates = candidatePair.second;
		if (candidates.empty())
		{
			continue;
		}

		int selectedOutputPinId = candidates.back();
		int selectedDepth = -1;
		std::unordered_set<int> visitingOutputPins;
		for (int outputPinId : candidates)
		{
			const int depth = GetOutputDependencyDepth(outputPinId, visitingOutputPins);
			if (depth >= selectedDepth)
			{
				selectedDepth = depth;
				selectedOutputPinId = outputPinId;
			}
		}

		inputPinToOutputPin[candidatePair.first] = selectedOutputPinId;
		++outputPinUseCount[selectedOutputPinId];
	}

	struct GeneratedValue
	{
		std::string expression;
		ShaderPinType type{ ShaderPinType::None };
		int cost{ 1 };
		bool pure{ true };
		bool constant{ false };
		bool expensive{ false };
		bool sideEffect{ false };
	};

	struct StageResult
	{
		std::string calculation;
		std::string functionDefinitions;
		std::unordered_map<std::string, GeneratedValue> masterValues;
	};

	auto CompileStage = [&](const std::vector<std::string>& targetMasterPins) -> StageResult
		{
			StageResult stage;
			std::unordered_map<int, GeneratedValue> compiledOutputPins;
			std::unordered_set<int> compilingOutputPins;
			std::unordered_set<std::string> emittedFunctionNames;
			std::unordered_map<std::string, int> symbolUseCounts;

			auto ToLowerCamel = [](std::string value) -> std::string
				{
					value = SanitizeIdentifier(value);
					if (!value.empty())
					{
						value[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[0])));
					}
					return value.empty() ? "value" : value;
				};

			auto MakeUniqueSymbol = [&](std::string preferredName) -> std::string
				{
					preferredName = ToLowerCamel(preferredName);
					int& count = symbolUseCounts[preferredName];
					if (count++ == 0)
					{
						return preferredName;
					}
					return preferredName + std::to_string(count);
				};

			auto GetPreferredSymbolName = [&](const ShaderNode& node, const ShaderPin& outputPin) -> std::string
				{
					if (node.typeCategory == "Texture" && node.name == "Texture Sample")
					{
						std::string samplerName = GetTextureSamplerNameForNode(input, node);
						if (!samplerName.empty())
						{
							return samplerName + "Sample";
						}
						if (!node.stringData.empty())
						{
							return BuildTextureSamplerBaseName(node.stringData) + "Sample";
						}
						return "textureSample";
					}

					if (node.name == "Add") return "sum";
					if (node.name == "Subtract") return "difference";
					if (node.name == "Multiply") return "product";
					if (node.name == "Divide") return "quotient";
					if (node.name == "Modulo") return "remainder";
					if (node.name == "Normalize") return "normalizedValue";
					if (node.name == "Dot") return "dotValue";
					if (node.name == "Cross") return "crossValue";
					if (node.name == "Length") return "lengthValue";
					if (node.name == "Mix") return "mixedValue";
					if (node.name == "Clamp") return "clampedValue";
					if (node.name == "Mask") return "masked" + outputPin.name;
					if (node.typeCategory == "MaterialVariableSet" || node.typeCategory == "MaterialVariableArraySet") return GetMaterialVariableReferenceName(node) + "Value";
					if (node.typeCategory == "MaterialVariableArray" || node.typeCategory == "MaterialVariableArrayGet") return GetMaterialVariableReferenceName(node) + outputPin.name;
					if (node.typeCategory == "Functions") return node.name + outputPin.name;
					if (node.typeCategory == "Custom") return "customResult";
					if (node.typeCategory == "Flow") return "selectedValue";
					return node.name.empty() ? outputPin.name : node.name + outputPin.name;
				};

			auto AppendStatement = [&](const std::string& statement)
				{
					if (statement.empty())
					{
						return;
					}
					stage.calculation += statement;
					if (stage.calculation.back() != '\n')
					{
						stage.calculation += "\n";
					}
				};

			auto EmitTemporary = [&](const ShaderNode& node, const ShaderPin& outputPin, const GeneratedValue& value) -> GeneratedValue
				{
					if (value.type == ShaderPinType::Any)
					{
						return value;
					}

					GeneratedValue emittedValue = value;
					emittedValue.expression = MakeUniqueSymbol(GetPreferredSymbolName(node, outputPin));
					emittedValue.cost = 1;
					emittedValue.constant = false;
					emittedValue.expensive = false;
					emittedValue.sideEffect = false;
					AppendStatement("\t" + GetGLSLTypeString(value.type) + " " + emittedValue.expression + " = " + value.expression + ";");
					return emittedValue;
				};

			auto ShouldInline = [&](int outputPinId, const GeneratedValue& value) -> bool
				{
					if (value.expression.empty()) return true;
					if (value.constant) return true;
					if (!value.pure || value.sideEffect || value.expensive) return false;
					const auto useCountIterator = outputPinUseCount.find(outputPinId);
					const int useCount = useCountIterator == outputPinUseCount.end() ? 0 : useCountIterator->second;
					if (useCount > 1) return false;
					return value.cost <= 6;
				};

			std::function<GeneratedValue(const ShaderPin&)> CompileInputPin;
			std::function<GeneratedValue(int)> CompileOutputPin;

			CompileInputPin = [&](const ShaderPin& inputPin) -> GeneratedValue
				{
					const auto linkedOutputIterator = inputPinToOutputPin.find(inputPin.id);
					if (linkedOutputIterator == inputPinToOutputPin.end())
					{
						auto [defaultExpression, defaultType] = GetDefaultValueString(inputPin);
						return { defaultExpression, defaultType, 1, true, true, false, false };
					}
					return CompileOutputPin(linkedOutputIterator->second);
				};

			CompileOutputPin = [&](int outputPinId) -> GeneratedValue
				{
					const auto compiledIterator = compiledOutputPins.find(outputPinId);
					if (compiledIterator != compiledOutputPins.end())
					{
						return compiledIterator->second;
					}

					if (compilingOutputPins.find(outputPinId) != compilingOutputPins.end())
					{
						return { "0.0", ShaderPinType::Float, 1, true, true, false, false };
					}

					const auto pinIterator = pinsById.find(outputPinId);
					const auto nodeIterator = nodeByPinId.find(outputPinId);
					if (pinIterator == pinsById.end() || nodeIterator == nodeByPinId.end())
					{
						return { "0.0", ShaderPinType::Float, 1, true, true, false, false };
					}

					const ShaderPin& outputPin = *pinIterator->second;
					const ShaderNode& node = *nodeIterator->second;
					compilingOutputPins.insert(outputPinId);

					auto CacheAndReturn = [&](GeneratedValue value) -> GeneratedValue
						{
							if (!ShouldInline(outputPinId, value))
							{
								value = EmitTemporary(node, outputPin, value);
							}
							compiledOutputPins[outputPinId] = value;
							compilingOutputPins.erase(outputPinId);
							return value;
						};

					if (node.typeCategory == "Variables")
					{
						return CacheAndReturn({ node.name, outputPin.type, 1, true, false, false, false });
					}

					if (node.typeCategory == "MaterialVariable" || node.typeCategory == "MaterialVariableGet")
					{
						return CacheAndReturn({ node.typeCategory == "MaterialVariableGet" ? GetMaterialVariableReferenceName(node) : SanitizeIdentifier(node.name), outputPin.type, 1, true, false, false, false });
					}

					if (node.typeCategory == "Constants")
					{
						if (node.name == "Float Constant")
						{
							auto [value, type] = GetDefaultValueString(outputPin);
							return CacheAndReturn({ value, type, 1, true, true, false, false });
						}
						if ((node.name == "Vector2 Constant" || node.name == "Vector3 Constant" || node.name == "Vector4 Constant") && !node.outputs.empty())
						{
							std::vector<GeneratedValue> components;
							for (const ShaderPin& inputPin : node.inputs)
							{
								components.push_back(CompileInputPin(inputPin));
							}

							const ShaderPinType outType = node.name == "Vector2 Constant" ? ShaderPinType::Vector2 : node.name == "Vector3 Constant" ? ShaderPinType::Vector3 : ShaderPinType::Vector4;
							std::string expression = GetGLSLTypeString(outType) + "(";
							bool allConstant = true;
							int cost = 1;
							for (size_t index = 0; index < components.size(); ++index)
							{
								if (index > 0) expression += ", ";
								expression += components[index].expression;
								allConstant = allConstant && components[index].constant;
								cost += components[index].cost;
							}
							expression += ")";
							return CacheAndReturn({ expression, outType, cost, true, allConstant, false, false });
						}
					}

					if (node.typeCategory == "Math" || node.typeCategory == "Trigonometry" ||
						node.typeCategory == "Exponential" || node.typeCategory == "Geometric" ||
						node.typeCategory == "Matrix")
					{
						if (node.name == "Mask" && !node.inputs.empty())
						{
							GeneratedValue value = CompileInputPin(node.inputs[0]);
							const auto outputIterator = std::find_if(node.outputs.begin(), node.outputs.end(), [outputPinId](const ShaderPin& pin) { return pin.id == outputPinId; });
							const size_t outputIndex = outputIterator == node.outputs.end() ? 0 : static_cast<size_t>(std::distance(node.outputs.begin(), outputIterator));
							return CacheAndReturn({ GetMaskComponentExpression(value.expression, value.type, outputIndex), ShaderPinType::Float, value.cost + 1, value.pure, value.constant, false, false });
						}

						if ((node.name == "Add" || node.name == "Subtract" || node.name == "Multiply" || node.name == "Divide" || node.name == "Modulo") && node.inputs.size() >= 2)
						{
							GeneratedValue a = CompileInputPin(node.inputs[0]);
							GeneratedValue b = CompileInputPin(node.inputs[1]);
							ShaderPinType outType = PromoteTypes(a.type, b.type);
							bool pure = a.pure && b.pure;
							bool constant = a.constant && b.constant;
							std::string expression;

							if (node.name == "Add" && b.constant && b.expression == "0.0") expression = a.expression;
							else if (node.name == "Add" && a.constant && a.expression == "0.0") expression = b.expression;
							else if (node.name == "Subtract" && b.constant && b.expression == "0.0") expression = a.expression;
							else if (node.name == "Multiply" && b.constant && b.expression == "1.0") expression = a.expression;
							else if (node.name == "Multiply" && a.constant && a.expression == "1.0") expression = b.expression;
							else if (node.name == "Multiply" && ((a.constant && a.expression == "0.0") || (b.constant && b.expression == "0.0"))) expression = "0.0";
							else if (node.name == "Divide" && b.constant && b.expression == "1.0") expression = a.expression;
							else if (node.name == "Modulo") expression = "mod(" + a.expression + ", " + b.expression + ")";
							else
							{
								const std::string op = node.name == "Add" ? "+" : node.name == "Subtract" ? "-" : node.name == "Multiply" ? "*" : "/";
								expression = "(" + a.expression + " " + op + " " + b.expression + ")";
							}

							return CacheAndReturn({ expression, outType, a.cost + b.cost + 1, pure, constant, false, false });
						}

						if ((node.name == "Modf" || node.name == "Frexp") && node.inputs.size() >= 1 && node.outputs.size() >= 2)
						{
							GeneratedValue value = CompileInputPin(node.inputs[0]);
							const std::string secondarySymbol = MakeUniqueSymbol(GetPreferredSymbolName(node, node.outputs[1]));
							const std::string primarySymbol = MakeUniqueSymbol(GetPreferredSymbolName(node, node.outputs[0]));
							const std::string funcName = GetGLSLFuncName(node.name);
							AppendStatement("\t" + GetGLSLTypeString(value.type) + " " + secondarySymbol + ";");
							AppendStatement("\t" + GetGLSLTypeString(value.type) + " " + primarySymbol + " = " + funcName + "(" + value.expression + ", " + secondarySymbol + ");");

							GeneratedValue primaryValue{ primarySymbol, value.type, 1, true, false, false, false };
							GeneratedValue secondaryValue{ secondarySymbol, value.type, 1, true, false, false, false };
							compiledOutputPins[node.outputs[0].id] = primaryValue;
							compiledOutputPins[node.outputs[1].id] = secondaryValue;
							compilingOutputPins.erase(outputPinId);
							return outputPinId == node.outputs[1].id ? secondaryValue : primaryValue;
						}

						const std::string funcName = GetGLSLFuncName(node.name);
						std::vector<GeneratedValue> inputs;
						for (const ShaderPin& inputPin : node.inputs)
						{
							inputs.push_back(CompileInputPin(inputPin));
						}

						ShaderPinType outType = outputPin.type;
						if (outType == ShaderPinType::Any || outType == ShaderPinType::None)
						{
							outType = inputs.empty() ? ShaderPinType::Float : inputs.front().type;
							for (size_t index = 1; index < inputs.size(); ++index)
							{
								outType = PromoteTypes(outType, inputs[index].type);
							}
						}
						if (node.name == "Length" || node.name == "Distance" || node.name == "Dot" || node.name == "Determinant" || node.name == "IsNan" || node.name == "IsInf") outType = ShaderPinType::Float;
						if (node.name == "Cross") outType = ShaderPinType::Vector3;
						if (node.name == "OuterProduct") outType = ShaderPinType::Matrix4x4;

						std::string expression = funcName + "(";
						bool pure = true;
						bool constant = true;
						int cost = 1;
						for (size_t index = 0; index < inputs.size(); ++index)
						{
							if (index > 0) expression += ", ";
							expression += inputs[index].expression;
							pure = pure && inputs[index].pure;
							constant = constant && inputs[index].constant;
							cost += inputs[index].cost;
						}
						expression += ")";
						return CacheAndReturn({ expression, outType, cost, pure, constant, false, false });
					}

					if (node.typeCategory == "Texture" && node.name == "Texture Sample" && !node.inputs.empty())
					{
						GeneratedValue uvValue;
						if (inputPinToOutputPin.find(node.inputs[0].id) == inputPinToOutputPin.end())
						{
							uvValue = { SHADER_VARIABLE_NAMES::TEXTURE::UV, ShaderPinType::Vector2, 1, true, false, false, false };
						}
						else
						{
							uvValue = CompileInputPin(node.inputs[0]);
						}

						const std::string samplerName = GetTextureSamplerNameForNode(input, node);
						const std::string expression = samplerName.empty()
							? "vec4(0.0, 0.0, 0.0, 1.0)"
							: "texture(" + samplerName + ", " + GetTextureSampleUVExpression(input, node, uvValue.expression, uvValue.type) + ")";
						return CacheAndReturn({ expression, ShaderPinType::Vector4, uvValue.cost + 4, true, samplerName.empty(), true, false });
					}

					if ((node.typeCategory == "MaterialVariableArray" || node.typeCategory == "MaterialVariableArrayGet") && node.outputs.size() >= 2)
					{
						const ShaderNode* declarationNode = node.typeCategory == "MaterialVariableArrayGet" ? FindMaterialVariableDeclarationNode(nodes, GetMaterialVariableReferenceName(node)) : &node;
						const std::string arrayName = node.typeCategory == "MaterialVariableArrayGet" ? GetMaterialVariableReferenceName(node) : SanitizeIdentifier(node.name);
						const int arraySize = static_cast<int>(std::max<size_t>(1, declarationNode ? declarationNode->arrayDefaultValues.size() : 1));

						if (outputPin.id == node.outputs[1].id)
						{
							return CacheAndReturn({ FormatFloat(static_cast<float>(arraySize)), ShaderPinType::Float, 1, true, true, false, false });
						}

						GeneratedValue indexValue = !node.inputs.empty() ? CompileInputPin(node.inputs[0]) : GeneratedValue{ "0.0", ShaderPinType::Float, 1, true, true, false, false };
						const std::string indexSymbol = MakeUniqueSymbol(arrayName + "Index");
						const std::string valueSymbol = MakeUniqueSymbol(GetPreferredSymbolName(node, outputPin));
						const ShaderPinType valueType = outputPin.type == ShaderPinType::Any || outputPin.type == ShaderPinType::None ? ShaderPinType::Float : outputPin.type;
						AppendStatement("\tint " + indexSymbol + " = int(clamp(floor(" + indexValue.expression + "), 0.0, " + FormatFloat(static_cast<float>(arraySize - 1)) + "));");
						AppendStatement("\t" + GetGLSLTypeString(valueType) + " " + valueSymbol + " = " + arrayName + "[" + indexSymbol + "];");
						GeneratedValue arrayValue{ valueSymbol, valueType, 1, true, false, false, false };
						compiledOutputPins[outputPinId] = arrayValue;
						compilingOutputPins.erase(outputPinId);
						return arrayValue;
					}

					if (node.typeCategory == "MaterialVariableSet" && !node.inputs.empty())
					{
						GeneratedValue inputValue = CompileInputPin(node.inputs[0]);
						const ShaderNode* declarationNode = FindMaterialVariableDeclarationNode(nodes, GetMaterialVariableReferenceName(node));
						const ShaderPinType outputType = declarationNode && !declarationNode->outputs.empty() ? declarationNode->outputs[0].type : inputValue.type;
						const std::string variableName = GetMaterialVariableReferenceName(node);
						const std::string formattedValue = FormatOutputValue(inputValue.expression, inputValue.type, outputType);
						if (declarationNode && !declarationNode->isUniform)
						{
							AppendStatement("\t" + variableName + " = " + formattedValue + ";");
							GeneratedValue setValue{ variableName, outputType, 1, false, false, false, true };
							compiledOutputPins[outputPinId] = setValue;
							compilingOutputPins.erase(outputPinId);
							return setValue;
						}
						return CacheAndReturn({ formattedValue, outputType, inputValue.cost, inputValue.pure, inputValue.constant, false, false });
					}

					if (node.typeCategory == "MaterialVariableArraySet" && node.inputs.size() >= 2)
					{
						GeneratedValue inputValue = CompileInputPin(node.inputs[0]);
						GeneratedValue indexValue = CompileInputPin(node.inputs[1]);
						const ShaderNode* declarationNode = FindMaterialVariableDeclarationNode(nodes, GetMaterialVariableReferenceName(node));
						const ShaderPinType outputType = declarationNode && !declarationNode->outputs.empty() ? declarationNode->outputs[0].type : inputValue.type;
						const std::string variableName = GetMaterialVariableReferenceName(node);
						const int arraySize = static_cast<int>(std::max<size_t>(1, declarationNode ? declarationNode->arrayDefaultValues.size() : 1));
						const std::string indexSymbol = MakeUniqueSymbol(variableName + "Index");
						const std::string formattedValue = FormatOutputValue(inputValue.expression, inputValue.type, outputType);
						AppendStatement("\tint " + indexSymbol + " = int(clamp(floor(" + indexValue.expression + "), 0.0, " + FormatFloat(static_cast<float>(arraySize - 1)) + "));");
						if (declarationNode && !declarationNode->isUniform)
						{
							AppendStatement("\t" + variableName + "[" + indexSymbol + "] = " + formattedValue + ";");
							GeneratedValue setValue{ variableName + "[" + indexSymbol + "]", outputType, 1, false, false, false, true };
							compiledOutputPins[outputPinId] = setValue;
							compilingOutputPins.erase(outputPinId);
							return setValue;
						}
						return CacheAndReturn({ formattedValue, outputType, inputValue.cost, inputValue.pure, inputValue.constant, false, false });
					}

					if (node.typeCategory == "Custom" && node.name == "Custom GLSL")
					{
						std::string code = node.stringData;
						std::string calculation;
						std::string result = "0.0";
						const size_t resultMarker = code.find("RETURN_RESULT:");
						if (resultMarker != std::string::npos)
						{
							calculation = code.substr(0, resultMarker);
							result = code.substr(resultMarker + 14);
						}
						else
						{
							calculation = code;
						}

						result.erase(std::remove(result.begin(), result.end(), ';'), result.end());
						result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
						result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());
						result = Trim(result);
						if (result.empty())
						{
							result = "0.0";
						}

						if (!Trim(calculation).empty())
						{
							AppendStatement(calculation);
						}

						const std::string symbol = MakeUniqueSymbol(GetPreferredSymbolName(node, outputPin));
						AppendStatement("#define " + symbol + " (" + result + ")");
						return CacheAndReturn({ symbol, ShaderPinType::Any, 1, false, false, false, true });
					}

					if (node.typeCategory == "Flow" && node.name == "If" && node.inputs.size() >= 5)
					{
						GeneratedValue a = CompileInputPin(node.inputs[0]);
						GeneratedValue b = CompileInputPin(node.inputs[1]);
						GeneratedValue less = CompileInputPin(node.inputs[2]);
						GeneratedValue equal = CompileInputPin(node.inputs[3]);
						GeneratedValue greater = CompileInputPin(node.inputs[4]);
						ShaderPinType outType = PromoteTypes(PromoteTypes(less.type, equal.type), greater.type);
						if (outType == ShaderPinType::Any || outType == ShaderPinType::None)
						{
							outType = less.type != ShaderPinType::Any && less.type != ShaderPinType::None ? less.type : equal.type != ShaderPinType::Any && equal.type != ShaderPinType::None ? equal.type : greater.type;
						}
						if (outType == ShaderPinType::Any || outType == ShaderPinType::None) outType = ShaderPinType::Float;
						const std::string expression = "((" + a.expression + ") < (" + b.expression + ")) ? " +
							FormatOutputValue(less.expression, less.type, outType) + " : (((" + a.expression + ") == (" + b.expression + ")) ? " +
							FormatOutputValue(equal.expression, equal.type, outType) + " : " + FormatOutputValue(greater.expression, greater.type, outType) + ")";
						return CacheAndReturn({ expression, outType, a.cost + b.cost + less.cost + equal.cost + greater.cost + 2, a.pure && b.pure && less.pure && equal.pure && greater.pure, a.constant && b.constant && less.constant && equal.constant && greater.constant, false, false });
					}

					if (node.typeCategory == "Flow" && node.name == "If Check" && node.inputs.size() >= 4)
					{
						GeneratedValue a = CompileInputPin(node.inputs[0]);
						GeneratedValue b = CompileInputPin(node.inputs[1]);
						GeneratedValue trueValue = CompileInputPin(node.inputs[2]);
						GeneratedValue falseValue = CompileInputPin(node.inputs[3]);
						ShaderPinType outType = PromoteTypes(trueValue.type, falseValue.type);
						if (outType == ShaderPinType::Any || outType == ShaderPinType::None) outType = trueValue.type != ShaderPinType::Any && trueValue.type != ShaderPinType::None ? trueValue.type : falseValue.type;
						if (outType == ShaderPinType::Any || outType == ShaderPinType::None) outType = ShaderPinType::Float;
						const std::string comparisonOperator = node.stringData.empty() ? ">" : node.stringData;
						const std::string expression = "((" + a.expression + ") " + comparisonOperator + " (" + b.expression + ")) ? " + FormatOutputValue(trueValue.expression, trueValue.type, outType) + " : " + FormatOutputValue(falseValue.expression, falseValue.type, outType);
						return CacheAndReturn({ expression, outType, a.cost + b.cost + trueValue.cost + falseValue.cost + 2, a.pure && b.pure && trueValue.pure && falseValue.pure, a.constant && b.constant && trueValue.constant && falseValue.constant, false, false });
					}

					if (node.typeCategory == "Flow" && node.name == "For Iteration" && node.inputs.size() >= 2)
					{
						GeneratedValue iterationValue = CompileInputPin(node.inputs[0]);
						GeneratedValue countValue = CompileInputPin(node.inputs[1]);
						return CacheAndReturn({ "clamp(floor(" + iterationValue.expression + "), 0.0, max(" + countValue.expression + " - 1.0, 0.0))", ShaderPinType::Float, iterationValue.cost + countValue.cost + 3, iterationValue.pure && countValue.pure, iterationValue.constant && countValue.constant, false, false });
					}

					if (node.typeCategory == "Functions")
					{
						MaterialFunction materialFunction;
						if (!LoadMaterialFunctionSignatureFromAsset(node.stringData, materialFunction))
						{
							return CacheAndReturn({ "0.0", ShaderPinType::Float, 1, true, true, false, false });
						}

						const std::string functionName = materialFunction.GetGeneratedFunctionName();
						if (!functionName.empty() && emittedFunctionNames.insert(functionName).second)
						{
							stage.functionDefinitions += materialFunction.GetGeneratedFunctionDefinitions();
							if (!stage.functionDefinitions.empty() && stage.functionDefinitions.back() != '\n')
							{
								stage.functionDefinitions += "\n";
							}
						}

						std::string joinedArguments;
						int cost = 2;
						for (size_t inputIndex = 0; inputIndex < node.inputs.size(); ++inputIndex)
						{
							GeneratedValue argument = CompileInputPin(node.inputs[inputIndex]);
							if (inputIndex > 0)
							{
								joinedArguments += ", ";
							}
							joinedArguments += argument.expression;
							cost += argument.cost;
						}

						const auto outputIterator = std::find_if(node.outputs.begin(), node.outputs.end(), [outputPinId](const ShaderPin& pin) { return pin.id == outputPinId; });
						const size_t outputIndex = outputIterator == node.outputs.end() ? 0 : static_cast<size_t>(std::distance(node.outputs.begin(), outputIterator));
						const MaterialFunctionPinDefinition outputDefinition{ outputPin.name, outputPin.type };
						const std::string expression = GetMaterialFunctionOutputFunctionName(functionName, outputDefinition, outputIndex) + "(" + joinedArguments + ")";
						return CacheAndReturn({ expression, outputPin.type, cost, true, false, true, false });
					}

					return CacheAndReturn({ "0.0", ShaderPinType::Float, 1, true, true, false, false });
				};

			for (const ShaderPin& masterInput : master->inputs)
			{
				if (std::find(targetMasterPins.begin(), targetMasterPins.end(), masterInput.name) == targetMasterPins.end())
				{
					continue;
				}

				if (inputPinToOutputPin.find(masterInput.id) == inputPinToOutputPin.end())
				{
					continue;
				}

				stage.masterValues[masterInput.name] = CompileInputPin(masterInput);
			}

			if (!stage.calculation.empty())
			{
				stage.calculation = "\n\t// Optimized shader graph calculations\n" + stage.calculation;
			}
			return stage;
		};

	StageResult vertexStage = CompileStage({ "Vertex Normal", "World Position Offset" });
	outMaterialData->vertexShaderFunctions = vertexStage.functionDefinitions;

	StageResult fragmentStage = CompileStage({ "Base Color", "Emissive", "Fragment Normal", "Normal", "Ambient Occlusion", "Metallic", "Roughness" });
	outMaterialData->baseColor.calculation = fragmentStage.calculation;
	outMaterialData->fragmentShaderFunctions = fragmentStage.functionDefinitions;

	auto AssignMaterialResult = [&](const std::string& masterPinName, ShaderFunctionAndResult& targetResult) -> bool
		{
			const auto valueIterator = fragmentStage.masterValues.find(masterPinName);
			if (valueIterator == fragmentStage.masterValues.end())
			{
				return false;
			}

			const auto masterPinIterator = std::find_if(master->inputs.begin(), master->inputs.end(), [&masterPinName](const ShaderPin& pin)
				{
					return pin.name == masterPinName;
				});
			if (masterPinIterator == master->inputs.end())
			{
				return false;
			}

			targetResult.result = FormatOutputValue(valueIterator->second.expression, valueIterator->second.type, masterPinIterator->type) + ";";
			return true;
		};

	AssignMaterialResult("Base Color", outMaterialData->baseColor);
	AssignMaterialResult("Emissive", outMaterialData->emisiveColor);
	const bool hasFragmentNormal = AssignMaterialResult("Fragment Normal", outMaterialData->fragmentNormal);
	if (hasFragmentNormal)
	{
		outMaterialData->fragmentNormalIsTangentSpace = true;
	}
	else if (AssignMaterialResult("Normal", outMaterialData->fragmentNormal))
	{
		outMaterialData->fragmentNormalIsTangentSpace = true;
	}
	AssignMaterialResult("Ambient Occlusion", outMaterialData->ambientOcclusion);
	AssignMaterialResult("Metallic", outMaterialData->metallic);
	AssignMaterialResult("Roughness", outMaterialData->roughness);

	auto AssignVertexMaterialResult = [&](const std::string& masterPinName, ShaderFunctionAndResult& targetResult)
		{
			const auto valueIterator = vertexStage.masterValues.find(masterPinName);
			if (valueIterator == vertexStage.masterValues.end())
			{
				return;
			}

			const auto masterPinIterator = std::find_if(master->inputs.begin(), master->inputs.end(), [&masterPinName](const ShaderPin& pin)
				{
					return pin.name == masterPinName;
				});
			if (masterPinIterator == master->inputs.end())
			{
				return;
			}

			targetResult.result = FormatOutputValue(valueIterator->second.expression, valueIterator->second.type, masterPinIterator->type) + ";";
		};

	AssignVertexMaterialResult("Vertex Normal", outMaterialData->vertexNormal);
	AssignVertexMaterialResult("World Position Offset", outMaterialData->vertexPositionOffset);

	// Emit shared vertex-stage graph calculations exactly once, before the first
	// generated vertex expression that can reference them. World-position offset
	// is evaluated before vertex-normal assignment in ShaderBuilder, so it owns
	// the shared calculations whenever it is connected; otherwise the vertex
	// normal path owns them.
	if (!outMaterialData->vertexPositionOffset.result.empty())
	{
		outMaterialData->vertexPositionOffset.calculation = vertexStage.calculation;
	}
	else if (!outMaterialData->vertexNormal.result.empty())
	{
		outMaterialData->vertexNormal.calculation = vertexStage.calculation;
	}

}
