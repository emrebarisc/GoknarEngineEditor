#pragma once

#include "UI/Panels/ShaderEditor/ShaderEditorTypes.h"

#include <algorithm>
#include <string>

namespace ShaderGraphTypeUtils
{
	inline constexpr const char* kVectorConverterCategory = "Converters";

	inline bool IsFloatVectorType(ShaderPinType type)
	{
		return type == ShaderPinType::Vector2 || type == ShaderPinType::Vector3 || type == ShaderPinType::Vector4;
	}

	inline int GetFloatVectorDimension(ShaderPinType type)
	{
		switch (type)
		{
		case ShaderPinType::Float: return 1;
		case ShaderPinType::Vector2: return 2;
		case ShaderPinType::Vector3: return 3;
		case ShaderPinType::Vector4: return 4;
		default: return 0;
		}
	}

	inline ShaderPinType GetFloatVectorTypeForDimension(int dimension)
	{
		switch (dimension)
		{
		case 2: return ShaderPinType::Vector2;
		case 3: return ShaderPinType::Vector3;
		case 4: return ShaderPinType::Vector4;
		default: return ShaderPinType::Float;
		}
	}

	inline const char* GetGLSLFloatVectorTypeName(ShaderPinType type)
	{
		switch (type)
		{
		case ShaderPinType::Vector2: return "vec2";
		case ShaderPinType::Vector3: return "vec3";
		case ShaderPinType::Vector4: return "vec4";
		default: return "float";
		}
	}

	inline const char* GetSwizzleForDimension(int dimension)
	{
		switch (dimension)
		{
		case 2: return "xy";
		case 3: return "xyz";
		case 4: return "xyzw";
		default: return "x";
		}
	}

	inline ShaderPinType GetPromotedType(ShaderPinType left, ShaderPinType right)
	{
		if (left == right) return left;
		if (left == ShaderPinType::None) return right;
		if (right == ShaderPinType::None) return left;
		if (left == ShaderPinType::Any) return right;
		if (right == ShaderPinType::Any) return left;
		if (left == ShaderPinType::Float) return right;
		if (right == ShaderPinType::Float) return left;
		return static_cast<int>(left) > static_cast<int>(right) ? left : right;
	}

	inline ShaderPinType GetVectorAwareOperationType(ShaderPinType left, ShaderPinType right)
	{
		const int leftDimension = GetFloatVectorDimension(left);
		const int rightDimension = GetFloatVectorDimension(right);
		if (leftDimension > 1 && rightDimension > 1 && leftDimension != rightDimension)
		{
			return GetFloatVectorTypeForDimension(std::min(leftDimension, rightDimension));
		}

		return GetPromotedType(left, right);
	}

	inline ShaderPinType GetFloatVectorConverterTargetType(const std::string& nodeName)
	{
		if (nodeName == "Float2" || nodeName == "float2" || nodeName == "Float2 Converter") return ShaderPinType::Vector2;
		if (nodeName == "Float3" || nodeName == "float3" || nodeName == "Float3 Converter") return ShaderPinType::Vector3;
		if (nodeName == "Float4" || nodeName == "float4" || nodeName == "Float4 Converter") return ShaderPinType::Vector4;
		return ShaderPinType::None;
	}

	inline bool IsFloatVectorConverterNode(const ShaderNode& node)
	{
		return node.typeCategory == kVectorConverterCategory && GetFloatVectorConverterTargetType(node.name) != ShaderPinType::None;
	}

	inline std::string ConvertExpressionToType(
		const std::string& expression,
		ShaderPinType actualType,
		ShaderPinType expectedType,
		const char* zeroLiteral = "0.0",
		const char* oneLiteral = "1.0")
	{
		if (actualType == expectedType ||
			actualType == ShaderPinType::Any ||
			expectedType == ShaderPinType::Any ||
			expectedType == ShaderPinType::None)
		{
			return expression;
		}

		if (expectedType == ShaderPinType::Float)
		{
			if (IsFloatVectorType(actualType))
			{
				return "(" + expression + ").x";
			}
			if (actualType == ShaderPinType::Vector4i)
			{
				return "float((" + expression + ").x)";
			}
			return expression;
		}

		if (actualType == ShaderPinType::Float && IsFloatVectorType(expectedType))
		{
			return std::string(GetGLSLFloatVectorTypeName(expectedType)) + "(" + expression + ")";
		}

		const int actualDimension = GetFloatVectorDimension(actualType);
		const int expectedDimension = GetFloatVectorDimension(expectedType);
		if (actualDimension == 0 || expectedDimension == 0)
		{
			return expression;
		}

		if (expectedDimension < actualDimension)
		{
			return "(" + expression + ")." + GetSwizzleForDimension(expectedDimension);
		}

		if (expectedType == ShaderPinType::Vector3 && actualType == ShaderPinType::Vector2)
		{
			return "vec3(" + expression + ", " + zeroLiteral + ")";
		}
		if (expectedType == ShaderPinType::Vector4 && actualType == ShaderPinType::Vector2)
		{
			return "vec4(" + expression + ", " + zeroLiteral + ", " + oneLiteral + ")";
		}
		if (expectedType == ShaderPinType::Vector4 && actualType == ShaderPinType::Vector3)
		{
			return "vec4(" + expression + ", " + oneLiteral + ")";
		}

		return expression;
	}
}
