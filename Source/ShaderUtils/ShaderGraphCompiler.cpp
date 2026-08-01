#include "ShaderGraphCompiler.h"

#include "ShaderUtils/ShaderGraphTypeUtils.h"

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
#include <utility>

namespace
{
	constexpr const char* MATERIAL_NODE_METADATA_PREFIX = "// GOKNAR_MATERIAL_NODE|";
	constexpr const char* kWorldTransformGetterCategory = "WorldTransformGetters";
	constexpr const char* kWorldPositionGetterNodeName = "World Position";
	constexpr const char* kWorldRotationGetterNodeName = "World Rotation";
	constexpr const char* kWorldScalingGetterNodeName = "World Scaling";

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

	bool IsWorldTransformGetterNode(const ShaderNode& node)
	{
		return node.typeCategory == kWorldTransformGetterCategory;
	}

	std::string GetWorldTransformMatrixExpression()
	{
		return SHADER_VARIABLE_NAMES::VERTEX_SHADER_OUTS::FINAL_MODEL_MATRIX;
	}

	std::string BuildWorldPositionExpression(const std::string& matrixExpression)
	{
		return "vec3(" + matrixExpression + "[0][3], " + matrixExpression + "[1][3], " + matrixExpression + "[2][3])";
	}

	std::string BuildWorldScalingExpression(const std::string& matrixExpression)
	{
		return "vec3("
			"length(vec3(" + matrixExpression + "[0][0], " + matrixExpression + "[1][0], " + matrixExpression + "[2][0])), "
			"length(vec3(" + matrixExpression + "[0][1], " + matrixExpression + "[1][1], " + matrixExpression + "[2][1])), "
			"length(vec3(" + matrixExpression + "[0][2], " + matrixExpression + "[1][2], " + matrixExpression + "[2][2])))";
	}

	std::string BuildWorldRotationGetterFunctionDefinition(const std::string& functionName)
	{
		return
			"vec3 " + functionName + "(mat4 worldTransform)\n"
			"{\n"
			"\tvec3 worldScale = " + BuildWorldScalingExpression("worldTransform") + ";\n"
			"\tvec3 safeScale = max(worldScale, vec3(0.000001));\n"
			"\tfloat r00 = worldTransform[0][0] / safeScale.x;\n"
			"\tfloat r01 = worldTransform[0][1] / safeScale.y;\n"
			"\tfloat r02 = worldTransform[0][2] / safeScale.z;\n"
			"\tfloat r12 = worldTransform[1][2] / safeScale.z;\n"
			"\tfloat r22 = worldTransform[2][2] / safeScale.z;\n"
			"\treturn vec3(atan(r12, r22), asin(clamp(-r02, -1.0, 1.0)), atan(r01, r00));\n"
			"}\n";
	}

	std::string GetWorldTransformGetterExpression(const ShaderNode& node, const std::string& matrixExpression, const std::string& rotationFunctionName)
	{
		if (node.name == kWorldPositionGetterNodeName)
		{
			return BuildWorldPositionExpression(matrixExpression);
		}

		if (node.name == kWorldScalingGetterNodeName)
		{
			return BuildWorldScalingExpression(matrixExpression);
		}

		if (node.name == kWorldRotationGetterNodeName)
		{
			return rotationFunctionName + "(" + matrixExpression + ")";
		}

		return "vec3(0.0)";
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

	std::string BuildMaterialVariableDeclarations(const std::vector<ShaderNode>& nodes)
	{
		std::string materialVariableDeclarations;
		for (const ShaderNode& node : nodes)
		{
			if (!IsMaterialVariableDeclarationCategory(node.typeCategory))
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

		return materialVariableDeclarations;
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

	bool IsIdentifierCharacter(char character)
	{
		return std::isalnum(static_cast<unsigned char>(character)) || character == '_';
	}

	std::string GetTrailingIdentifier(const std::string& value)
	{
		if (value.empty())
		{
			return "";
		}

		size_t end = value.size();
		while (end > 0 && std::isspace(static_cast<unsigned char>(value[end - 1])))
		{
			--end;
		}

		size_t begin = end;
		while (begin > 0 && IsIdentifierCharacter(value[begin - 1]))
		{
			--begin;
		}

		return begin < end ? value.substr(begin, end - begin) : "";
	}

	bool IsSimpleIdentifierExpression(const std::string& value)
	{
		if (value.empty())
		{
			return false;
		}

		const unsigned char firstCharacter = static_cast<unsigned char>(value.front());
		if (!std::isalpha(firstCharacter) && value.front() != '_')
		{
			return false;
		}

		return std::all_of(value.begin() + 1, value.end(), [](char character)
			{
				return IsIdentifierCharacter(character);
			});
	}

	std::unordered_set<std::string> GetFunctionParameterNamesFromSignatureLine(const std::string& line)
	{
		std::unordered_set<std::string> parameterNames;
		const size_t openParenthesis = line.find('(');
		const size_t closeParenthesis = line.rfind(')');
		if (openParenthesis == std::string::npos || closeParenthesis == std::string::npos || closeParenthesis <= openParenthesis)
		{
			return parameterNames;
		}

		const std::string prefix = Trim(line.substr(0, openParenthesis));
		if (prefix.empty() || prefix.find('=') != std::string::npos || prefix.find(';') != std::string::npos)
		{
			return parameterNames;
		}

		std::string parameters = line.substr(openParenthesis + 1, closeParenthesis - openParenthesis - 1);
		if (Trim(parameters).empty() || Trim(parameters) == "void")
		{
			return parameterNames;
		}

		size_t parameterStart = 0;
		while (parameterStart <= parameters.size())
		{
			const size_t comma = parameters.find(',', parameterStart);
			const size_t parameterEnd = comma == std::string::npos ? parameters.size() : comma;
			const std::string parameter = Trim(parameters.substr(parameterStart, parameterEnd - parameterStart));
			const std::string parameterName = GetTrailingIdentifier(parameter);
			if (!parameterName.empty())
			{
				parameterNames.insert(parameterName);
			}

			if (comma == std::string::npos)
			{
				break;
			}
			parameterStart = comma + 1;
		}

		return parameterNames;
	}

	bool IsParameterSelfDeclarationLine(const std::string& line, const std::unordered_set<std::string>& parameterNames)
	{
		if (parameterNames.empty())
		{
			return false;
		}

		std::string trimmedLine = Trim(line);
		if (trimmedLine.empty() || trimmedLine.back() != ';')
		{
			return false;
		}
		trimmedLine.pop_back();

		const size_t assignment = trimmedLine.find('=');
		if (assignment == std::string::npos || trimmedLine.find('=', assignment + 1) != std::string::npos)
		{
			return false;
		}

		const std::string left = Trim(trimmedLine.substr(0, assignment));
		const std::string right = Trim(trimmedLine.substr(assignment + 1));
		const std::string declaredName = GetTrailingIdentifier(left);
		return !declaredName.empty() && declaredName == right && parameterNames.find(declaredName) != parameterNames.end();
	}

	// Material functions saved by the old graph compiler may contain declarations such as
	// `vec3 position = position;`. Keep them loadable, but do not use this as part of new code generation.
	std::string RemoveLegacyMaterialFunctionParameterSelfDeclarations(const std::string& definitions)
	{
		std::istringstream input(definitions);
		std::ostringstream output;
		std::string line;
		std::unordered_set<std::string> activeParameterNames;
		int braceDepth = 0;
		bool wroteAnyLine = false;

		while (std::getline(input, line))
		{
			if (braceDepth == 0)
			{
				activeParameterNames = GetFunctionParameterNamesFromSignatureLine(line);
			}

			const bool skipLine = braceDepth > 0 && IsParameterSelfDeclarationLine(line, activeParameterNames);
			if (!skipLine)
			{
				if (wroteAnyLine)
				{
					output << '\n';
				}
				output << line;
				wroteAnyLine = true;
			}

			for (char character : line)
			{
				if (character == '{')
				{
					++braceDepth;
				}
				else if (character == '}')
				{
					braceDepth = std::max(0, braceDepth - 1);
				}
			}

			if (braceDepth == 0 && line.find('}') != std::string::npos)
			{
				activeParameterNames.clear();
			}
		}

		return output.str();
	}

	bool LoadMaterialFunctionSignatureFromAsset(const std::string& assetPath, MaterialFunction& outMaterialFunction)
	{
		if (!MaterialFunctionSerializer::Deserialize(assetPath, outMaterialFunction))
		{
			return false;
		}

		outMaterialFunction.SetGeneratedFunctionDefinitions(
			RemoveLegacyMaterialFunctionParameterSelfDeclarations(outMaterialFunction.GetGeneratedFunctionDefinitions()));
		return true;
	}

	std::string GetMaterialFunctionOutputFunctionName(const std::string& baseFunctionName, const MaterialFunctionPinDefinition& outputDefinition, size_t outputIndex)
	{
		return baseFunctionName + "_out_" + std::to_string(outputIndex) + "_" + SanitizeIdentifier(outputDefinition.name);
	}

	std::string GetGLSLTypeString(ShaderPinType type)
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
	}

	ShaderPinType PromoteTypes(ShaderPinType left, ShaderPinType right)
	{
		return ShaderGraphTypeUtils::GetPromotedType(left, right);
	}

	std::string FormatFloat(float value)
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
	}

	std::pair<std::string, ShaderPinType> GetDefaultValueString(const ShaderPin& pin)
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
	}

	std::string GetGLSLFuncName(const std::string& nodeName)
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
	}

	std::string GetMaskComponentExpression(const std::string& value, ShaderPinType inputType, size_t componentIndex)
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
	}

	std::string FormatOutputValue(const std::string& value, ShaderPinType actualType, ShaderPinType expectedType)
	{
		return ShaderGraphTypeUtils::ConvertExpressionToType(value, actualType, expectedType);
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
		std::vector<std::pair<std::string, std::string>> functionDefinitionBlocks;
		std::unordered_map<int, GeneratedValue> masterValuesByPinId;
	};

	StageResult CompileGraphStage(
		const ShaderGraphCompileInput& input,
		const std::vector<int>& targetMasterPinIds,
		const std::string& worldRotationGetterFunctionName,
		bool includeUnconnectedTargetPins)
	{
		StageResult stage;
		if (!input.nodes || !input.links)
		{
			return stage;
		}

		const std::vector<ShaderNode>& nodes = *input.nodes;
		const std::vector<ShaderLink>& links = *input.links;
		const ShaderNode* master = FindNode(nodes, input.masterNodeId);
		if (!master)
		{
			return stage;
		}

		std::unordered_map<int, const ShaderPin*> pinsById;
		std::unordered_map<int, const ShaderNode*> nodeByPinId;
		std::unordered_map<int, std::vector<int>> inputPinToOutputPinCandidates;
		std::unordered_map<int, int> inputPinToOutputPin;
		std::unordered_map<int, int> outputPinUseCount;

		for (const ShaderNode& node : nodes)
		{
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

		for (const ShaderNode& node : nodes)
		{
			std::string reservedName;
			if (node.typeCategory == "FunctionInput" || node.typeCategory == "Variables")
			{
				reservedName = node.name;
			}
			else if (node.typeCategory == "MaterialVariable")
			{
				reservedName = SanitizeIdentifier(node.name);
			}
			else if (node.typeCategory == "MaterialVariableGet")
			{
				reservedName = GetMaterialVariableReferenceName(node);
			}

			if (!reservedName.empty())
			{
				symbolUseCounts[ToLowerCamel(reservedName)] = std::max(symbolUseCounts[ToLowerCamel(reservedName)], 1);
			}
		}

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
				if (IsWorldTransformGetterNode(node))
				{
					if (node.name == kWorldPositionGetterNodeName) return "worldPosition";
					if (node.name == kWorldRotationGetterNodeName) return "worldRotation";
					if (node.name == kWorldScalingGetterNodeName) return "worldScaling";
				}
				if (node.typeCategory == "Functions") return node.name + outputPin.name;
				if (node.typeCategory == "FunctionInput") return node.name;
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

		auto AppendFunctionDefinition = [&](const std::string& key, std::string definition)
			{
				if (definition.empty() || !emittedFunctionNames.insert(key).second)
				{
					return;
				}
				if (!definition.empty() && definition.back() != '\n')
				{
					definition += "\n";
				}
				stage.functionDefinitions += definition;
				stage.functionDefinitionBlocks.push_back({ key, definition });
			};

		auto EmitTemporary = [&](const ShaderNode& node, const ShaderPin& outputPin, const GeneratedValue& value) -> GeneratedValue
			{
				if (value.type == ShaderPinType::Any)
				{
					return value;
				}

				GeneratedValue emittedValue = value;
				emittedValue.expression = MakeUniqueSymbol(GetPreferredSymbolName(node, outputPin));
				if (emittedValue.expression == value.expression)
				{
					return value;
				}
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
				if (value.pure && !value.sideEffect && IsSimpleIdentifierExpression(value.expression)) return true;
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

				GeneratedValue value = CompileOutputPin(linkedOutputIterator->second);
				if (inputPin.type != ShaderPinType::Any && inputPin.type != ShaderPinType::None)
				{
					const std::string convertedExpression = FormatOutputValue(value.expression, value.type, inputPin.type);
					if (convertedExpression != value.expression)
					{
						value.expression = convertedExpression;
						value.type = inputPin.type;
						++value.cost;
					}
				}
				return value;
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

				auto CacheDirectAndReturn = [&](GeneratedValue value) -> GeneratedValue
					{
						compiledOutputPins[outputPinId] = value;
						compilingOutputPins.erase(outputPinId);
						return value;
					};

				if (node.typeCategory == "FunctionInput")
				{
					// The node's value is the function parameter itself. Bypass temporary emission even
					// when the parameter fans out to multiple consumers.
					return CacheDirectAndReturn({ SanitizeIdentifier(node.name), outputPin.type, 1, true, false, false, false });
				}

				if (node.typeCategory == "Variables")
				{
					return CacheDirectAndReturn({ node.name, outputPin.type, 1, true, false, false, false });
				}

				if (node.typeCategory == "MaterialVariable" || node.typeCategory == "MaterialVariableGet")
				{
					return CacheDirectAndReturn({ node.typeCategory == "MaterialVariableGet" ? GetMaterialVariableReferenceName(node) : SanitizeIdentifier(node.name), outputPin.type, 1, true, false, false, false });
				}

				if (ShaderGraphTypeUtils::IsFloatVectorConverterNode(node) && !node.inputs.empty())
				{
					GeneratedValue inputValue = CompileInputPin(node.inputs[0]);
					const ShaderPinType outType = ShaderGraphTypeUtils::GetFloatVectorConverterTargetType(node.name);
					const std::string expression = FormatOutputValue(inputValue.expression, inputValue.type, outType);
					return CacheAndReturn({ expression, outType, inputValue.cost + 1, inputValue.pure, inputValue.constant, false, false });
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
						ShaderPinType outType = ShaderGraphTypeUtils::GetVectorAwareOperationType(a.type, b.type);
						const std::string formattedA = FormatOutputValue(a.expression, a.type, outType);
						const std::string formattedB = FormatOutputValue(b.expression, b.type, outType);
						bool pure = a.pure && b.pure;
						bool constant = a.constant && b.constant;
						std::string expression;

						if (node.name == "Add" && b.constant && b.expression == "0.0") expression = formattedA;
						else if (node.name == "Add" && a.constant && a.expression == "0.0") expression = formattedB;
						else if (node.name == "Subtract" && b.constant && b.expression == "0.0") expression = formattedA;
						else if (node.name == "Multiply" && b.constant && b.expression == "1.0") expression = formattedA;
						else if (node.name == "Multiply" && a.constant && a.expression == "1.0") expression = formattedB;
						else if (node.name == "Multiply" && ((a.constant && a.expression == "0.0") || (b.constant && b.expression == "0.0"))) expression = FormatOutputValue("0.0", ShaderPinType::Float, outType);
						else if (node.name == "Divide" && b.constant && b.expression == "1.0") expression = formattedA;
						else if (node.name == "Modulo") expression = "mod(" + formattedA + ", " + formattedB + ")";
						else
						{
							const std::string op = node.name == "Add" ? "+" : node.name == "Subtract" ? "-" : node.name == "Multiply" ? "*" : "/";
							expression = "(" + formattedA + " " + op + " " + formattedB + ")";
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
					ShaderPinType argumentType = inputs.empty() ? ShaderPinType::Float : inputs.front().type;
					for (size_t index = 1; index < inputs.size(); ++index)
					{
						if (node.name == "Refract" && index == 2)
						{
							continue;
						}
						argumentType = ShaderGraphTypeUtils::GetVectorAwareOperationType(argumentType, inputs[index].type);
					}
					if (outType == ShaderPinType::Any || outType == ShaderPinType::None)
					{
						outType = argumentType;
					}
					if (node.name == "Length" || node.name == "Distance" || node.name == "Dot" || node.name == "Determinant" || node.name == "IsNan" || node.name == "IsInf") outType = ShaderPinType::Float;
					if (node.name == "Cross")
					{
						argumentType = ShaderPinType::Vector3;
						outType = ShaderPinType::Vector3;
					}
					if (node.name == "OuterProduct")
					{
						argumentType = ShaderPinType::Any;
						outType = ShaderPinType::Matrix4x4;
					}

					std::string expression = funcName + "(";
					bool pure = true;
					bool constant = true;
					int cost = 1;
					for (size_t index = 0; index < inputs.size(); ++index)
					{
						if (index > 0) expression += ", ";
						const ShaderPinType expectedArgumentType = node.name == "Refract" && index == 2 ? ShaderPinType::Float : argumentType;
						expression += FormatOutputValue(inputs[index].expression, inputs[index].type, expectedArgumentType);
						pure = pure && inputs[index].pure;
						constant = constant && inputs[index].constant;
						cost += inputs[index].cost;
					}
					expression += ")";
					return CacheAndReturn({ expression, outType, cost, pure, constant, false, false });
				}

				if (IsWorldTransformGetterNode(node))
				{
					if (node.name == kWorldRotationGetterNodeName)
					{
						AppendFunctionDefinition(worldRotationGetterFunctionName, BuildWorldRotationGetterFunctionDefinition(worldRotationGetterFunctionName));
					}

					const int cost = node.name == kWorldPositionGetterNodeName ? 2 :
						node.name == kWorldScalingGetterNodeName ? 6 : 8;
					return CacheAndReturn({
						GetWorldTransformGetterExpression(node, GetWorldTransformMatrixExpression(), worldRotationGetterFunctionName),
						ShaderPinType::Vector3,
						cost,
						true,
						false,
						false,
						false
						});
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
					if (!functionName.empty())
					{
						AppendFunctionDefinition(functionName, materialFunction.GetGeneratedFunctionDefinitions());
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
					MaterialFunctionPinDefinition outputDefinition{ outputPin.name, outputPin.type };
					const std::vector<MaterialFunctionPinDefinition>& functionOutputs = materialFunction.GetOutputs();
					if (outputIndex < functionOutputs.size())
					{
						outputDefinition = functionOutputs[outputIndex];
					}
					const std::string expression = GetMaterialFunctionOutputFunctionName(functionName, outputDefinition, outputIndex) + "(" + joinedArguments + ")";
					return CacheAndReturn({ expression, outputDefinition.type, cost, true, false, true, false });
				}

				return CacheAndReturn({ "0.0", ShaderPinType::Float, 1, true, true, false, false });
			};

		std::unordered_set<int> targetMasterPinSet(targetMasterPinIds.begin(), targetMasterPinIds.end());
		for (const ShaderPin& masterInput : master->inputs)
		{
			if (targetMasterPinSet.find(masterInput.id) == targetMasterPinSet.end())
			{
				continue;
			}

			if (!includeUnconnectedTargetPins && inputPinToOutputPin.find(masterInput.id) == inputPinToOutputPin.end())
			{
				continue;
			}

			stage.masterValuesByPinId[masterInput.id] = CompileInputPin(masterInput);
		}

		if (!stage.calculation.empty())
		{
			stage.calculation = "\n\t// Optimized shader graph calculations\n" + stage.calculation;
		}
		return stage;
	}

	std::vector<int> GetMasterPinIdsByName(const ShaderNode& master, const std::vector<std::string>& targetMasterPins)
	{
		std::vector<int> pinIds;
		for (const ShaderPin& masterInput : master.inputs)
		{
			if (std::find(targetMasterPins.begin(), targetMasterPins.end(), masterInput.name) != targetMasterPins.end())
			{
				pinIds.push_back(masterInput.id);
			}
		}
		return pinIds;
	}
}

void ShaderGraphCompiler::CompileMaterial(const ShaderGraphCompileInput& input, MaterialInitializationData* outMaterialData) const
{
	if (!outMaterialData || !input.nodes || !input.links || !input.textures)
	{
		return;
	}

	const std::vector<ShaderNode>& nodes = *input.nodes;

	outMaterialData->baseColor.calculation = "";
	outMaterialData->baseColor.result = "";
	outMaterialData->emissiveColor.calculation = "";
	outMaterialData->emissiveColor.result = "";
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

	const std::string materialVariableDeclarations = BuildMaterialVariableDeclarations(nodes);
	outMaterialData->vertexShaderUniforms = materialVariableDeclarations;
	outMaterialData->fragmentShaderUniforms = materialVariableDeclarations;

	StageResult vertexStage = CompileGraphStage(
		input,
		GetMasterPinIdsByName(*master, { "Vertex Normal", "World Position Offset" }),
		"GoknarGetWorldRotationRadians",
		false);
	outMaterialData->vertexShaderFunctions = vertexStage.functionDefinitions;

	StageResult fragmentStage = CompileGraphStage(
		input,
		GetMasterPinIdsByName(*master, { "Base Color", "Emissive", "Fragment Normal", "Normal", "Ambient Occlusion", "Metallic", "Roughness" }),
		"GoknarGetWorldRotationRadians",
		false);
	outMaterialData->baseColor.calculation = fragmentStage.calculation;
	outMaterialData->fragmentShaderFunctions = fragmentStage.functionDefinitions;

	auto AssignMaterialResult = [&](const std::string& masterPinName, ShaderFunctionAndResult& targetResult) -> bool
		{
			const auto masterPinIterator = std::find_if(master->inputs.begin(), master->inputs.end(), [&masterPinName](const ShaderPin& pin)
				{
					return pin.name == masterPinName;
				});
			if (masterPinIterator == master->inputs.end())
			{
				return false;
			}

			const auto valueIterator = fragmentStage.masterValuesByPinId.find(masterPinIterator->id);
			if (valueIterator == fragmentStage.masterValuesByPinId.end())
			{
				return false;
			}

			targetResult.result = FormatOutputValue(valueIterator->second.expression, valueIterator->second.type, masterPinIterator->type) + ";";
			return true;
		};

	AssignMaterialResult("Base Color", outMaterialData->baseColor);
	AssignMaterialResult("Emissive", outMaterialData->emissiveColor);
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
			const auto masterPinIterator = std::find_if(master->inputs.begin(), master->inputs.end(), [&masterPinName](const ShaderPin& pin)
				{
					return pin.name == masterPinName;
				});
			if (masterPinIterator == master->inputs.end())
			{
				return;
			}

			const auto valueIterator = vertexStage.masterValuesByPinId.find(masterPinIterator->id);
			if (valueIterator == vertexStage.masterValuesByPinId.end())
			{
				return;
			}

			targetResult.result = FormatOutputValue(valueIterator->second.expression, valueIterator->second.type, masterPinIterator->type) + ";";
		};

	AssignVertexMaterialResult("Vertex Normal", outMaterialData->vertexNormal);
	AssignVertexMaterialResult("World Position Offset", outMaterialData->vertexPositionOffset);

	if (!outMaterialData->vertexPositionOffset.result.empty())
	{
		outMaterialData->vertexPositionOffset.calculation = vertexStage.calculation;
	}
	else if (!outMaterialData->vertexNormal.result.empty())
	{
		outMaterialData->vertexNormal.calculation = vertexStage.calculation;
	}
}

bool ShaderGraphCompiler::CompileMaterialFunction(const ShaderGraphCompileInput& input, const std::string& assetPath, MaterialFunction& outMaterialFunction) const
{
	if (!input.nodes || !input.links)
	{
		return false;
	}

	const std::vector<ShaderNode>& nodes = *input.nodes;
	const ShaderNode* masterNode = FindNode(nodes, input.masterNodeId);
	if (!masterNode || masterNode->inputs.empty())
	{
		return false;
	}

	outMaterialFunction.SetAssetPath(assetPath);
	outMaterialFunction.SetName(std::filesystem::path(assetPath).filename().generic_string());

	std::vector<const ShaderNode*> inputNodes;
	for (const ShaderNode& node : nodes)
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

	const std::string generatedFunctionName = "mf_" + SanitizeIdentifier(assetPath);
	outMaterialFunction.SetGeneratedFunctionName(generatedFunctionName);

	std::string functionDefinitions;
	std::unordered_set<std::string> emittedFunctionDefinitionKeys;
	const std::string worldRotationGetterFunctionName = generatedFunctionName + "_getWorldRotationRadians";

	for (size_t outputIndex = 0; outputIndex < masterNode->inputs.size(); ++outputIndex)
	{
		const ShaderPin& masterOutputPin = masterNode->inputs[outputIndex];
		StageResult stage = CompileGraphStage(input, { masterOutputPin.id }, worldRotationGetterFunctionName, true);
		for (const auto& block : stage.functionDefinitionBlocks)
		{
			if (emittedFunctionDefinitionKeys.insert(block.first).second)
			{
				functionDefinitions += block.second;
			}
		}

		const auto valueIterator = stage.masterValuesByPinId.find(masterOutputPin.id);
		GeneratedValue finalValue;
		if (valueIterator != stage.masterValuesByPinId.end())
		{
			finalValue = valueIterator->second;
		}
		else
		{
			auto [defaultValue, defaultType] = GetDefaultValueString(masterOutputPin);
			finalValue = { defaultValue, defaultType, 1, true, true, false, false };
		}

		const MaterialFunctionPinDefinition outputDefinition{
			masterOutputPin.name,
			masterOutputPin.type
		};

		functionDefinitions += GetGLSLTypeString(masterOutputPin.type) + " " +
			GetMaterialFunctionOutputFunctionName(generatedFunctionName, outputDefinition, outputIndex) + "(";
		for (size_t inputIndex = 0; inputIndex < inputDefinitions.size(); ++inputIndex)
		{
			if (inputIndex > 0)
			{
				functionDefinitions += ", ";
			}

			functionDefinitions += GetGLSLTypeString(inputDefinitions[inputIndex].type) + " " + SanitizeIdentifier(inputDefinitions[inputIndex].name);
		}
		functionDefinitions += ")\n{\n";
		functionDefinitions += stage.calculation;
		functionDefinitions += "\treturn " + FormatOutputValue(finalValue.expression, finalValue.type, masterOutputPin.type) + ";\n";
		functionDefinitions += "}\n";
	}

	outMaterialFunction.SetGeneratedFunctionDefinitions(functionDefinitions);
	return true;
}
