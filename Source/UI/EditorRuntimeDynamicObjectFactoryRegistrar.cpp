#include "EditorRuntimeDynamicObjectFactoryRegistrar.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Goknar/Engine.h"
#include "Goknar/Components/CameraComponent.h"
#include "Goknar/Components/Component.h"
#include "Goknar/Components/DynamicMeshComponent.h"
#include "Goknar/Components/InstancedStaticMeshComponent.h"
#include "Goknar/Components/LightComponents/PointLightComponent.h"
#include "Goknar/Components/ParticleSystemComponent.h"
#include "Goknar/Components/SkeletalMeshComponent.h"
#include "Goknar/Components/SocketComponent.h"
#include "Goknar/Components/StaticMeshComponent.h"
#include "Goknar/Debug/DebugDrawer.h"
#include "Goknar/Factories/DynamicObjectFactory.h"
#include "Goknar/Managers/ResourceManager.h"
#include "Goknar/Model/SkeletalMesh.h"
#include "Goknar/Model/StaticMesh.h"
#include "Goknar/Navigation/NavigationTreeComponent.h"
#include "Goknar/ObjectBase.h"
#include "Goknar/Objects/PlayerStart.h"
#include "Goknar/Objects/ReflectionProbeObject.h"
#include "Goknar/Physics/Character.h"
#include "Goknar/Physics/Components/BoxCollisionComponent.h"
#include "Goknar/Physics/Components/CapsuleCollisionComponent.h"
#include "Goknar/Physics/Components/CollisionComponent.h"
#include "Goknar/Physics/Components/MovingTriangleMeshCollisionComponent.h"
#include "Goknar/Physics/Components/MultipleCollisionComponent.h"
#include "Goknar/Physics/Components/NonMovingTriangleMeshCollisionComponent.h"
#include "Goknar/Physics/Components/PhysicsMovementComponent.h"
#include "Goknar/Physics/Components/SphereCollisionComponent.h"
#include "Goknar/Physics/OverlappingPhysicsObject.h"
#include "Goknar/Physics/PhysicsObject.h"
#include "Goknar/Physics/RigidBody.h"
#include "Goknar/UI/HUD.h"

namespace
{
	struct ClassInfo
	{
		std::string name;
		std::vector<std::string> bases;
	};

	enum class ReflectionActionType
	{
		AddComponent,
		SetRootComponent,
		SetStaticMesh,
		SetSkeletalMesh,
		SetRadius,
		SetHeight,
		SetHalfSize,
		SetRelativePosition,
		SetRelativeRotation,
		SetRelativeScaling,
		SetMass
	};

	struct ReflectionAction
	{
		ReflectionActionType type{ ReflectionActionType::AddComponent };
		std::string variableName;
		std::string componentClassName;
		std::string resourcePath;
		Vector3 vectorValue{ Vector3::ZeroVector };
		float floatValue{ 0.f };
	};

	struct ClassReflection
	{
		std::vector<std::string> bases;
		std::vector<ReflectionAction> actions;
	};

	bool applyReflectionsOnCreate = true;
	std::map<std::string, ClassInfo> projectClassInfos;
	std::unordered_map<std::string, ClassReflection> projectObjectReflections;
	std::unordered_set<const Component*> reflectedComponents;

	const std::set<std::string> kKnownObjectBases =
	{
		"ObjectBase",
		"DebugObject",
		"HUD",
		"Controller",
		"PhysicsObject",
		"OverlappingPhysicsObject",
		"RigidBody",
		"Character",
		"ReflectionProbeObject",
		"PlayerStart"
	};

	const std::set<std::string> kAbstractObjectPlaceholderBases =
	{
		"Controller"
	};

	const std::set<std::string> kKnownComponentBases =
	{
		"Component",
		"RenderComponent",
		"SocketComponent",
		"CameraComponent",
		"NavigationTreeComponent",
		"PointLightComponent",
		"MeshComponent",
		"DynamicMeshComponent",
		"InstancedStaticMeshComponent",
		"StaticMeshComponent",
		"SkeletalMeshComponent",
		"ParticleSystemComponent",
		"BillboardParticleSystemComponent",
		"StaticMeshParticleSystemComponent",
		"CollisionComponent",
		"BoxCollisionComponent",
		"CapsuleCollisionComponent",
		"SphereCollisionComponent",
		"MovingTriangleMeshCollisionComponent",
		"NonMovingTriangleMeshCollisionComponent",
		"MultipleCollisionComponent",
		"PhysicsMovementComponent"
	};

	const std::set<std::string> kAbstractComponentPlaceholderBases =
	{
		"RenderComponent",
		"MeshComponent",
		"ParticleSystemComponent"
	};

	const std::set<std::string> kPhysicsComponentBases =
	{
		"CollisionComponent",
		"BoxCollisionComponent",
		"CapsuleCollisionComponent",
		"SphereCollisionComponent",
		"MovingTriangleMeshCollisionComponent",
		"NonMovingTriangleMeshCollisionComponent",
		"MultipleCollisionComponent"
	};

	const std::set<std::string> kOverlappingComponentBases =
	{
		"PhysicsMovementComponent"
	};

	const std::set<std::string> kIgnoredClassDeclarationTokens =
	{
		"GOKNAR_API",
		"final"
	};

	const std::set<std::string> kIgnoredBaseDeclarationTokens =
	{
		"public",
		"protected",
		"private",
		"virtual"
	};

	template <typename BaseType>
	class RuntimeObjectPlaceholder : public BaseType
	{
	public:
		explicit RuntimeObjectPlaceholder(std::string registeredClassName) :
			BaseType(),
			registeredClassName_(std::move(registeredClassName))
		{
			this->SetName(registeredClassName_);
		}

		const std::string& GetRegisteredClassName() const
		{
			return registeredClassName_;
		}

	private:
		std::string registeredClassName_;
	};

	template <typename BaseType>
	class RuntimeComponentPlaceholder : public BaseType
	{
	public:
		RuntimeComponentPlaceholder(Component* parentComponent, std::string registeredClassName) :
			BaseType(parentComponent),
			registeredClassName_(std::move(registeredClassName))
		{
		}

		const std::string& GetRegisteredClassName() const
		{
			return registeredClassName_;
		}

	private:
		std::string registeredClassName_;
	};

	std::string NormalizePath(const std::string& path)
	{
		if (path.empty())
		{
			return "";
		}

		return std::filesystem::path(path).lexically_normal().generic_string();
	}

	std::string StripNamespace(std::string value)
	{
		const size_t namespaceSeparatorIndex = value.rfind("::");
		if (namespaceSeparatorIndex != std::string::npos)
		{
			value = value.substr(namespaceSeparatorIndex + 2);
		}

		return value;
	}

	std::string RemoveComments(const std::string& content)
	{
		std::string result;
		result.reserve(content.size());

		for (size_t characterIndex = 0; characterIndex < content.size();)
		{
			if (characterIndex + 1 < content.size() && content[characterIndex] == '/' && content[characterIndex + 1] == '/')
			{
				while (characterIndex < content.size() && content[characterIndex] != '\n')
				{
					result += ' ';
					++characterIndex;
				}
				continue;
			}

			if (characterIndex + 1 < content.size() && content[characterIndex] == '/' && content[characterIndex + 1] == '*')
			{
				result += ' ';
				result += ' ';
				characterIndex += 2;
				while (characterIndex + 1 < content.size() && !(content[characterIndex] == '*' && content[characterIndex + 1] == '/'))
				{
					result += content[characterIndex] == '\n' ? '\n' : ' ';
					++characterIndex;
				}
				if (characterIndex + 1 < content.size())
				{
					result += ' ';
					result += ' ';
					characterIndex += 2;
				}
				continue;
			}

			result += content[characterIndex];
			++characterIndex;
		}

		return result;
	}

	std::vector<std::string> ExtractIdentifierTokens(const std::string& value)
	{
		std::vector<std::string> tokens;
		const std::regex identifierRegex("[A-Za-z_][A-Za-z0-9_:]*");
		for (std::sregex_iterator tokenIterator(value.begin(), value.end(), identifierRegex), endIterator; tokenIterator != endIterator; ++tokenIterator)
		{
			tokens.push_back(tokenIterator->str());
		}

		return tokens;
	}

	std::string NormalizeStatement(std::string value)
	{
		std::replace(value.begin(), value.end(), '\r', ' ');
		std::replace(value.begin(), value.end(), '\n', ' ');
		std::replace(value.begin(), value.end(), '\t', ' ');
		return value;
	}

	bool TryParseFloatLiteral(const std::string& value, float& outValue)
	{
		const std::regex numberRegex("[-+]?(?:[0-9]*\\.[0-9]+|[0-9]+)(?:[eE][-+]?[0-9]+)?");
		std::smatch match;
		if (!std::regex_search(value, match, numberRegex))
		{
			return false;
		}

		try
		{
			outValue = std::stof(match.str());
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool TryParseVector3Literal(const std::string& value, Vector3& outValue)
	{
		std::string vectorBody = value;
		std::smatch vectorMatch;
		const std::regex vectorRegex("Vector3\\s*(?:\\{|\\()\\s*([^\\}\\)]*)[\\}\\)]");
		if (std::regex_search(value, vectorMatch, vectorRegex))
		{
			vectorBody = vectorMatch[1].str();
		}

		std::vector<float> values;
		const std::regex numberRegex("[-+]?(?:[0-9]*\\.[0-9]+|[0-9]+)(?:[eE][-+]?[0-9]+)?");
		for (std::sregex_iterator numberIterator(vectorBody.begin(), vectorBody.end(), numberRegex), endIterator; numberIterator != endIterator; ++numberIterator)
		{
			try
			{
				values.push_back(std::stof(numberIterator->str()));
			}
			catch (...)
			{
				return false;
			}
		}

		if (values.size() == 1)
		{
			outValue = Vector3(values[0]);
			return true;
		}

		if (3 <= values.size())
		{
			outValue = Vector3(values[0], values[1], values[2]);
			return true;
		}

		return false;
	}

	std::vector<std::string> SplitStatements(const std::string& constructorBody)
	{
		std::vector<std::string> statements;
		std::string currentStatement;
		int parenthesisDepth = 0;
		int braceDepth = 0;

		for (char character : constructorBody)
		{
			if (character == '(')
			{
				++parenthesisDepth;
			}
			else if (character == ')' && 0 < parenthesisDepth)
			{
				--parenthesisDepth;
			}
			else if (character == '{')
			{
				++braceDepth;
			}
			else if (character == '}' && 0 < braceDepth)
			{
				--braceDepth;
			}

			if (character == ';' && parenthesisDepth == 0 && braceDepth == 0)
			{
				statements.push_back(NormalizeStatement(currentStatement));
				currentStatement.clear();
				continue;
			}

			currentStatement += character;
		}

		if (!currentStatement.empty())
		{
			statements.push_back(NormalizeStatement(currentStatement));
		}

		return statements;
	}

	std::vector<std::string> ExtractConstructorBodies(const std::string& sourceContent, const std::string& className)
	{
		std::vector<std::string> constructorBodies;
		const std::regex constructorRegex("\\b" + className + "\\s*::\\s*" + className + "\\s*\\([^\\)]*\\)\\s*(?::[^\\{]*)?\\{");

		for (std::sregex_iterator constructorIterator(sourceContent.begin(), sourceContent.end(), constructorRegex), endIterator;
			constructorIterator != endIterator;
			++constructorIterator)
		{
			const size_t openingBraceIndex = sourceContent.find('{', static_cast<size_t>(constructorIterator->position()));
			if (openingBraceIndex == std::string::npos)
			{
				continue;
			}

			size_t characterIndex = openingBraceIndex + 1;
			int braceDepth = 1;
			while (characterIndex < sourceContent.size() && 0 < braceDepth)
			{
				if (sourceContent[characterIndex] == '{')
				{
					++braceDepth;
				}
				else if (sourceContent[characterIndex] == '}')
				{
					--braceDepth;
				}
				++characterIndex;
			}

			if (braceDepth == 0 && openingBraceIndex + 1 < characterIndex - 1)
			{
				constructorBodies.push_back(sourceContent.substr(openingBraceIndex + 1, characterIndex - openingBraceIndex - 2));
			}
		}

		return constructorBodies;
	}

	bool IsSourceFile(const std::filesystem::path& filePath)
	{
		std::string extension = filePath.extension().generic_string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});

		return extension == ".h" ||
			extension == ".hh" ||
			extension == ".hpp" ||
			extension == ".hxx" ||
			extension == ".cpp" ||
			extension == ".cc" ||
			extension == ".cxx";
	}

	void ParseClassDeclarations(const std::filesystem::path& filePath, std::map<std::string, ClassInfo>& classInfos)
	{
		std::ifstream sourceFile(filePath);
		if (!sourceFile.is_open())
		{
			return;
		}

		const std::string sourceContent = RemoveComments(std::string(std::istreambuf_iterator<char>(sourceFile), std::istreambuf_iterator<char>()));
		const std::regex classRegex("\\b(class|struct)[ \\t\\r\\n]+([^:;{}]+)[ \\t\\r\\n]*:[ \\t\\r\\n]*([^{};]+)\\{");

		for (std::sregex_iterator classIterator(sourceContent.begin(), sourceContent.end(), classRegex), endIterator; classIterator != endIterator; ++classIterator)
		{
			const std::string classDeclaration = (*classIterator)[2].str();
			const std::string baseDeclaration = (*classIterator)[3].str();

			std::string className;
			for (const std::string& classToken : ExtractIdentifierTokens(classDeclaration))
			{
				const std::string token = StripNamespace(classToken);
				if (kIgnoredClassDeclarationTokens.find(token) == kIgnoredClassDeclarationTokens.end())
				{
					className = token;
				}
			}

			if (className.empty())
			{
				continue;
			}

			ClassInfo classInfo;
			classInfo.name = className;

			for (const std::string& baseToken : ExtractIdentifierTokens(baseDeclaration))
			{
				const std::string baseName = StripNamespace(baseToken);
				if (kIgnoredBaseDeclarationTokens.find(baseName) == kIgnoredBaseDeclarationTokens.end())
				{
					classInfo.bases.push_back(baseName);
				}
			}

			classInfos[classInfo.name] = classInfo;
		}
	}

	bool DerivesFrom(
		const std::string& className,
		const std::set<std::string>& baseClassNames,
		const std::map<std::string, ClassInfo>& classInfos,
		std::set<std::string>& visitedClassNames)
	{
		if (baseClassNames.find(className) != baseClassNames.end())
		{
			return true;
		}

		if (visitedClassNames.find(className) != visitedClassNames.end())
		{
			return false;
		}
		visitedClassNames.insert(className);

		const auto classInfoIterator = classInfos.find(className);
		if (classInfoIterator == classInfos.end())
		{
			return false;
		}

		for (const std::string& baseClassName : classInfoIterator->second.bases)
		{
			if (baseClassNames.find(baseClassName) != baseClassNames.end())
			{
				return true;
			}

			if (DerivesFrom(baseClassName, baseClassNames, classInfos, visitedClassNames))
			{
				return true;
			}
		}

		return false;
	}

	bool DerivesFrom(
		const std::string& className,
		const std::set<std::string>& baseClassNames,
		const std::map<std::string, ClassInfo>& classInfos)
	{
		std::set<std::string> visitedClassNames;
		return DerivesFrom(className, baseClassNames, classInfos, visitedClassNames);
	}

	std::string GetPlaceholderBase(
		const std::string& className,
		const std::set<std::string>& knownBaseClassNames,
		const std::set<std::string>& abstractPlaceholderBaseClassNames,
		const std::string& fallbackBaseName,
		const std::map<std::string, ClassInfo>& classInfos)
	{
		const auto classInfoIterator = classInfos.find(className);
		if (classInfoIterator != classInfos.end())
		{
			for (const std::string& baseClassName : classInfoIterator->second.bases)
			{
				if (knownBaseClassNames.find(baseClassName) != knownBaseClassNames.end())
				{
					if (abstractPlaceholderBaseClassNames.find(baseClassName) != abstractPlaceholderBaseClassNames.end())
					{
						return fallbackBaseName;
					}

					return baseClassName;
				}
			}

			for (const std::string& baseClassName : classInfoIterator->second.bases)
			{
				if (classInfos.find(baseClassName) != classInfos.end())
				{
					const std::string placeholderBase = GetPlaceholderBase(baseClassName, knownBaseClassNames, abstractPlaceholderBaseClassNames, fallbackBaseName, classInfos);
					if (placeholderBase != fallbackBaseName)
					{
						return placeholderBase;
					}
				}
			}
		}

		return fallbackBaseName;
	}

	int GetClassDepth(const std::string& className, const std::map<std::string, ClassInfo>& classInfos, std::set<std::string>& visitedClassNames)
	{
		if (visitedClassNames.find(className) != visitedClassNames.end())
		{
			return 0;
		}
		visitedClassNames.insert(className);

		const auto classInfoIterator = classInfos.find(className);
		if (classInfoIterator == classInfos.end())
		{
			return 0;
		}

		int maxDepth = 0;
		for (const std::string& baseClassName : classInfoIterator->second.bases)
		{
			int candidateDepth = 0;
			if (classInfos.find(baseClassName) != classInfos.end())
			{
				candidateDepth = GetClassDepth(baseClassName, classInfos, visitedClassNames) + 1;
			}
			else if (kKnownObjectBases.find(baseClassName) != kKnownObjectBases.end() || kKnownComponentBases.find(baseClassName) != kKnownComponentBases.end())
			{
				candidateDepth = 1;
			}

			maxDepth = (std::max)(maxDepth, candidateDepth);
		}

		return maxDepth;
	}

	int GetClassDepth(const std::string& className, const std::map<std::string, ClassInfo>& classInfos)
	{
		std::set<std::string> visitedClassNames;
		return GetClassDepth(className, classInfos, visitedClassNames);
	}

	void AddAction(ClassReflection& classReflection, const ReflectionAction& action)
	{
		classReflection.actions.push_back(action);
	}

	void ParseConstructorReflection(const std::string& constructorBody, ClassReflection& classReflection)
	{
		std::unordered_map<std::string, std::pair<std::string, std::string>> resourceVariables;
		const std::vector<std::string> statements = SplitStatements(constructorBody);

		for (const std::string& statement : statements)
		{
			std::smatch match;

			const std::regex resourceRegex("(?:StaticMesh|SkeletalMesh)\\s*\\*?\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*engine\\s*->\\s*GetResourceManager\\s*\\(\\s*\\)\\s*->\\s*GetContent\\s*<\\s*(StaticMesh|SkeletalMesh)\\s*>\\s*\\(\\s*\"([^\"]+)\"");
			if (std::regex_search(statement, match, resourceRegex))
			{
				resourceVariables[match[1].str()] = { match[2].str(), match[3].str() };
				continue;
			}

			const std::regex resourceAssignmentRegex("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*engine\\s*->\\s*GetResourceManager\\s*\\(\\s*\\)\\s*->\\s*GetContent\\s*<\\s*(StaticMesh|SkeletalMesh)\\s*>\\s*\\(\\s*\"([^\"]+)\"");
			if (std::regex_search(statement, match, resourceAssignmentRegex))
			{
				resourceVariables[match[1].str()] = { match[2].str(), match[3].str() };
				continue;
			}

			const std::regex addComponentRegex("(?:(?:[A-Za-z_][A-Za-z0-9_:<>]*\\s*\\*?\\s*)?([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*)?AddSubComponent\\s*<\\s*([A-Za-z_][A-Za-z0-9_:]*)\\s*>\\s*\\(");
			if (std::regex_search(statement, match, addComponentRegex))
			{
				const size_t addComponentCallIndex = statement.find("AddSubComponent", static_cast<size_t>(match.position()));
				if (2 <= addComponentCallIndex && statement.substr(addComponentCallIndex - 2, 2) == "->")
				{
					size_t ownerTokenEnd = addComponentCallIndex - 2;
					while (0 < ownerTokenEnd && std::isspace(static_cast<unsigned char>(statement[ownerTokenEnd - 1])))
					{
						--ownerTokenEnd;
					}

					size_t ownerTokenStart = ownerTokenEnd;
					while (0 < ownerTokenStart)
					{
						const char character = statement[ownerTokenStart - 1];
						if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
						{
							break;
						}
						--ownerTokenStart;
					}

					if (statement.substr(ownerTokenStart, ownerTokenEnd - ownerTokenStart) != "this")
					{
						continue;
					}
				}
				else if (1 <= addComponentCallIndex && statement[addComponentCallIndex - 1] == '.')
				{
					continue;
				}

				ReflectionAction action;
				action.type = ReflectionActionType::AddComponent;
				action.variableName = match[1].matched ? match[1].str() : "";
				action.componentClassName = StripNamespace(match[2].str());
				AddAction(classReflection, action);
				continue;
			}

			const std::regex setRootComponentRegex("\\bSetRootComponent\\s*\\(\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*\\)");
			if (std::regex_search(statement, match, setRootComponentRegex))
			{
				ReflectionAction action;
				action.type = ReflectionActionType::SetRootComponent;
				action.variableName = match[1].str();
				AddAction(classReflection, action);
				continue;
			}

			const std::regex setMeshRegex("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*->\\s*SetMesh\\s*\\((.*)\\)\\s*$");
			if (std::regex_search(statement, match, setMeshRegex))
			{
				const std::string variableName = match[1].str();
				const std::string argument = match[2].str();
				std::string resourceType;
				std::string resourcePath;

				const std::regex directResourceRegex("GetContent\\s*<\\s*(StaticMesh|SkeletalMesh)\\s*>\\s*\\(\\s*\"([^\"]+)\"");
				std::smatch directResourceMatch;
				if (std::regex_search(argument, directResourceMatch, directResourceRegex))
				{
					resourceType = directResourceMatch[1].str();
					resourcePath = directResourceMatch[2].str();
				}
				else
				{
					std::smatch resourceVariableMatch;
					const std::regex resourceVariableRegex("\\b([A-Za-z_][A-Za-z0-9_]*)\\b");
					if (std::regex_search(argument, resourceVariableMatch, resourceVariableRegex))
					{
						const auto resourceVariableIterator = resourceVariables.find(resourceVariableMatch[1].str());
						if (resourceVariableIterator != resourceVariables.end())
						{
							resourceType = resourceVariableIterator->second.first;
							resourcePath = resourceVariableIterator->second.second;
						}
					}
				}

				if (!resourcePath.empty())
				{
					ReflectionAction action;
					action.type = resourceType == "SkeletalMesh" ? ReflectionActionType::SetSkeletalMesh : ReflectionActionType::SetStaticMesh;
					action.variableName = variableName;
					action.resourcePath = resourcePath;
					AddAction(classReflection, action);
				}
				continue;
			}

			const std::regex componentFloatSetterRegex("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*->\\s*(SetRadius|SetHeight)\\s*\\((.*)\\)\\s*$");
			if (std::regex_search(statement, match, componentFloatSetterRegex))
			{
				float value = 0.f;
				if (TryParseFloatLiteral(match[3].str(), value))
				{
					ReflectionAction action;
					action.type = match[2].str() == "SetRadius" ? ReflectionActionType::SetRadius : ReflectionActionType::SetHeight;
					action.variableName = match[1].str();
					action.floatValue = value;
					AddAction(classReflection, action);
				}
				continue;
			}

			const std::regex componentVectorSetterRegex("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*->\\s*(SetHalfSize|SetRelativePosition|SetRelativeScaling|SetRelativeRotation)\\s*\\((.*)\\)\\s*$");
			if (std::regex_search(statement, match, componentVectorSetterRegex))
			{
				Vector3 value = Vector3::ZeroVector;
				if (TryParseVector3Literal(match[3].str(), value))
				{
					ReflectionAction action;
					const std::string setterName = match[2].str();
					if (setterName == "SetHalfSize")
					{
						action.type = ReflectionActionType::SetHalfSize;
					}
					else if (setterName == "SetRelativePosition")
					{
						action.type = ReflectionActionType::SetRelativePosition;
					}
					else if (setterName == "SetRelativeScaling")
					{
						action.type = ReflectionActionType::SetRelativeScaling;
					}
					else
					{
						action.type = ReflectionActionType::SetRelativeRotation;
					}
					action.variableName = match[1].str();
					action.vectorValue = value;
					AddAction(classReflection, action);
				}
				continue;
			}

			const std::regex massSetterRegex("\\bSetMass\\s*\\((.*)\\)\\s*$");
			if (std::regex_search(statement, match, massSetterRegex))
			{
				float mass = 0.f;
				if (TryParseFloatLiteral(match[1].str(), mass))
				{
					ReflectionAction action;
					action.type = ReflectionActionType::SetMass;
					action.floatValue = mass;
					AddAction(classReflection, action);
				}
			}
		}
	}

	void ParseProjectReflections(const std::vector<std::filesystem::path>& sourceFiles, const std::map<std::string, ClassInfo>& classInfos)
	{
		projectObjectReflections.clear();
		projectClassInfos = classInfos;

		for (const auto& classInfoPair : classInfos)
		{
			const std::string& className = classInfoPair.first;
			if (kKnownObjectBases.find(className) == kKnownObjectBases.end() &&
				kKnownComponentBases.find(className) == kKnownComponentBases.end() &&
				DerivesFrom(className, kKnownObjectBases, classInfos))
			{
				projectObjectReflections[className].bases = classInfoPair.second.bases;
			}
		}

		if (projectObjectReflections.empty())
		{
			return;
		}

		for (const std::filesystem::path& sourceFilePath : sourceFiles)
		{
			std::ifstream sourceFile(sourceFilePath);
			if (!sourceFile.is_open())
			{
				continue;
			}

			const std::string sourceContent = RemoveComments(std::string(std::istreambuf_iterator<char>(sourceFile), std::istreambuf_iterator<char>()));
			for (auto& classReflectionPair : projectObjectReflections)
			{
				for (const std::string& constructorBody : ExtractConstructorBodies(sourceContent, classReflectionPair.first))
				{
					ParseConstructorReflection(constructorBody, classReflectionPair.second);
				}
			}
		}
	}

	std::string GetComponentPlaceholderBase(const std::string& className)
	{
		if (kKnownComponentBases.find(className) != kKnownComponentBases.end() &&
			kAbstractComponentPlaceholderBases.find(className) == kAbstractComponentPlaceholderBases.end())
		{
			return className;
		}

		return GetPlaceholderBase(className, kKnownComponentBases, kAbstractComponentPlaceholderBases, "Component", projectClassInfos);
	}

	void MarkReflectedComponent(Component* component)
	{
		if (component)
		{
			reflectedComponents.insert(component);
		}
	}

	void MarkExistingComponentsAsReflected(ObjectBase* object)
	{
		if (!object)
		{
			return;
		}

		for (Component* component : object->GetComponents())
		{
			MarkReflectedComponent(component);
		}
	}

	bool ComponentMatchesClassName(Component* component, const std::string& className)
	{
		if (!component)
		{
			return false;
		}

		const std::string registeredClassName = DynamicObjectFactory::GetInstance()->GetRegisteredComponentClassName(component);
		if (registeredClassName == className)
		{
			return true;
		}

		return registeredClassName == GetComponentPlaceholderBase(className);
	}

	bool ComponentIsAlreadyBound(Component* component, const std::unordered_map<std::string, Component*>& componentVariables)
	{
		for (const auto& componentVariablePair : componentVariables)
		{
			if (componentVariablePair.second == component)
			{
				return true;
			}
		}

		return false;
	}

	Component* FindComponentForReflectionAction(
		ObjectBase* object,
		const std::string& className,
		const std::unordered_map<std::string, Component*>& componentVariables,
		bool preferAlreadyReflected)
	{
		if (!object)
		{
			return nullptr;
		}

		for (Component* component : object->GetComponents())
		{
			if (ComponentIsAlreadyBound(component, componentVariables) || !ComponentMatchesClassName(component, className))
			{
				continue;
			}

			if (preferAlreadyReflected && reflectedComponents.find(component) == reflectedComponents.end())
			{
				continue;
			}

			if (!preferAlreadyReflected && reflectedComponents.find(component) != reflectedComponents.end())
			{
				continue;
			}

			return component;
		}

		return nullptr;
	}

	Component* FindReusableComponentForReflectionAction(
		ObjectBase* object,
		const std::string& className,
		const std::unordered_map<std::string, Component*>& componentVariables)
	{
		if (Component* component = FindComponentForReflectionAction(object, className, componentVariables, true))
		{
			return component;
		}

		return FindComponentForReflectionAction(object, className, componentVariables, false);
	}

	Component* AddComponentByClassName(ObjectBase* object, const std::string& className)
	{
		if (!object)
		{
			return nullptr;
		}

		const std::string placeholderBaseName = GetComponentPlaceholderBase(className);
		if (placeholderBaseName == "SocketComponent")
		{
			return object->AddSubComponent<SocketComponent>();
		}
		if (placeholderBaseName == "CameraComponent")
		{
			return object->AddSubComponent<CameraComponent>();
		}
		if (placeholderBaseName == "NavigationTreeComponent")
		{
			return object->AddSubComponent<NavigationTreeComponent>();
		}
		if (placeholderBaseName == "PointLightComponent")
		{
			return object->AddSubComponent<PointLightComponent>();
		}
		if (placeholderBaseName == "DynamicMeshComponent")
		{
			return object->AddSubComponent<DynamicMeshComponent>();
		}
		if (placeholderBaseName == "InstancedStaticMeshComponent")
		{
			return object->AddSubComponent<InstancedStaticMeshComponent>();
		}
		if (placeholderBaseName == "StaticMeshComponent")
		{
			return object->AddSubComponent<StaticMeshComponent>();
		}
		if (placeholderBaseName == "SkeletalMeshComponent")
		{
			return object->AddSubComponent<SkeletalMeshComponent>();
		}
		if (placeholderBaseName == "BillboardParticleSystemComponent")
		{
			return object->AddSubComponent<BillboardParticleSystemComponent>();
		}
		if (placeholderBaseName == "StaticMeshParticleSystemComponent")
		{
			return object->AddSubComponent<StaticMeshParticleSystemComponent>();
		}
		if (placeholderBaseName == "CollisionComponent")
		{
			return object->AddSubComponent<CollisionComponent>();
		}
		if (placeholderBaseName == "BoxCollisionComponent")
		{
			return object->AddSubComponent<BoxCollisionComponent>();
		}
		if (placeholderBaseName == "CapsuleCollisionComponent")
		{
			return object->AddSubComponent<CapsuleCollisionComponent>();
		}
		if (placeholderBaseName == "SphereCollisionComponent")
		{
			return object->AddSubComponent<SphereCollisionComponent>();
		}
		if (placeholderBaseName == "MovingTriangleMeshCollisionComponent")
		{
			return object->AddSubComponent<MovingTriangleMeshCollisionComponent>();
		}
		if (placeholderBaseName == "NonMovingTriangleMeshCollisionComponent")
		{
			return object->AddSubComponent<NonMovingTriangleMeshCollisionComponent>();
		}
		if (placeholderBaseName == "MultipleCollisionComponent")
		{
			return object->AddSubComponent<MultipleCollisionComponent>();
		}
		if (placeholderBaseName == "PhysicsMovementComponent")
		{
			return object->AddSubComponent<PhysicsMovementComponent>();
		}

		return object->AddSubComponent<Component>();
	}

	Component* ResolveComponentVariable(ObjectBase* object, std::unordered_map<std::string, Component*>& componentVariables, const std::string& variableName)
	{
		const auto componentVariableIterator = componentVariables.find(variableName);
		if (componentVariableIterator != componentVariables.end())
		{
			return componentVariableIterator->second;
		}

		Component* component = nullptr;
		if (variableName == "rootComponent_")
		{
			component = object ? object->GetRootComponent() : nullptr;
		}
		else if (variableName == "capsuleCollisionComponent_")
		{
			component = object ? object->GetFirstComponentOfType<CapsuleCollisionComponent>() : nullptr;
		}
		else if (variableName == "skeletalMeshComponent_")
		{
			component = object ? object->GetFirstComponentOfType<SkeletalMeshComponent>() : nullptr;
		}
		else if (variableName == "cameraComponent_" || variableName == "thirdPersonCameraComponent_")
		{
			component = object ? object->GetFirstComponentOfType<CameraComponent>() : nullptr;
		}
		else if (variableName == "staticMeshComponent_")
		{
			component = object ? object->GetFirstComponentOfType<StaticMeshComponent>() : nullptr;
		}
		else if (variableName == "movementComponent_" || variableName == "defaultCharacterMovementComponent_")
		{
			component = object ? object->GetFirstComponentOfType<PhysicsMovementComponent>() : nullptr;
		}

		if (component)
		{
			componentVariables[variableName] = component;
		}

		return component;
	}

	void ApplyObjectReflection(
		const std::string& className,
		ObjectBase* object,
		std::set<std::string>& appliedClassNames,
		std::unordered_map<std::string, Component*>& componentVariables,
		bool reuseExistingComponents)
	{
		if (!object || appliedClassNames.find(className) != appliedClassNames.end())
		{
			return;
		}
		appliedClassNames.insert(className);

		const auto classInfoIterator = projectClassInfos.find(className);
		if (classInfoIterator != projectClassInfos.end())
		{
			for (const std::string& baseClassName : classInfoIterator->second.bases)
			{
				if (projectObjectReflections.find(baseClassName) != projectObjectReflections.end())
				{
					ApplyObjectReflection(baseClassName, object, appliedClassNames, componentVariables, reuseExistingComponents);
				}
			}
		}

		const auto classReflectionIterator = projectObjectReflections.find(className);
		if (classReflectionIterator == projectObjectReflections.end())
		{
			return;
		}

		for (const ReflectionAction& action : classReflectionIterator->second.actions)
		{
			if (action.type == ReflectionActionType::AddComponent)
			{
				Component* component = reuseExistingComponents ?
					FindReusableComponentForReflectionAction(object, action.componentClassName, componentVariables) :
					nullptr;
				if (!component)
				{
					component = AddComponentByClassName(object, action.componentClassName);
				}
				MarkReflectedComponent(component);
				if (component && !action.variableName.empty())
				{
					componentVariables[action.variableName] = component;
				}
				continue;
			}

			Component* component = ResolveComponentVariable(object, componentVariables, action.variableName);
			MarkReflectedComponent(component);
			switch (action.type)
			{
			case ReflectionActionType::SetRootComponent:
				if (component)
				{
					object->SetRootComponent(component);
				}
				break;
			case ReflectionActionType::SetStaticMesh:
				{
					StaticMesh* staticMesh = engine->GetResourceManager()->GetContent<StaticMesh>(action.resourcePath);
					if (StaticMeshComponent* staticMeshComponent = dynamic_cast<StaticMeshComponent*>(component))
					{
						staticMeshComponent->SetMesh(staticMesh);
					}
					else if (MovingTriangleMeshCollisionComponent* movingTriangleMeshCollisionComponent = dynamic_cast<MovingTriangleMeshCollisionComponent*>(component))
					{
						movingTriangleMeshCollisionComponent->SetMesh(staticMesh);
					}
					else if (NonMovingTriangleMeshCollisionComponent* nonMovingTriangleMeshCollisionComponent = dynamic_cast<NonMovingTriangleMeshCollisionComponent*>(component))
					{
						nonMovingTriangleMeshCollisionComponent->SetMesh(staticMesh);
					}
				}
				break;
			case ReflectionActionType::SetSkeletalMesh:
				if (SkeletalMeshComponent* skeletalMeshComponent = dynamic_cast<SkeletalMeshComponent*>(component))
				{
					SkeletalMesh* skeletalMesh = engine->GetResourceManager()->GetContent<SkeletalMesh>(action.resourcePath);
					skeletalMeshComponent->SetMesh(skeletalMesh);
				}
				break;
			case ReflectionActionType::SetRadius:
				if (SphereCollisionComponent* sphereCollisionComponent = dynamic_cast<SphereCollisionComponent*>(component))
				{
					sphereCollisionComponent->SetRadius(action.floatValue);
				}
				else if (CapsuleCollisionComponent* capsuleCollisionComponent = dynamic_cast<CapsuleCollisionComponent*>(component))
				{
					capsuleCollisionComponent->SetRadius(action.floatValue);
				}
				break;
			case ReflectionActionType::SetHeight:
				if (CapsuleCollisionComponent* capsuleCollisionComponent = dynamic_cast<CapsuleCollisionComponent*>(component))
				{
					capsuleCollisionComponent->SetHeight(action.floatValue);
				}
				break;
			case ReflectionActionType::SetHalfSize:
				if (BoxCollisionComponent* boxCollisionComponent = dynamic_cast<BoxCollisionComponent*>(component))
				{
					boxCollisionComponent->SetHalfSize(action.vectorValue);
				}
				break;
			case ReflectionActionType::SetRelativePosition:
				if (component)
				{
					component->SetRelativePosition(action.vectorValue);
				}
				break;
			case ReflectionActionType::SetRelativeRotation:
				if (component)
				{
					component->SetRelativeRotation(Quaternion::FromEulerDegrees(action.vectorValue));
				}
				break;
			case ReflectionActionType::SetRelativeScaling:
				if (component)
				{
					component->SetRelativeScaling(action.vectorValue);
				}
				break;
			case ReflectionActionType::SetMass:
				if (RigidBody* rigidBody = dynamic_cast<RigidBody*>(object))
				{
					rigidBody->SetMass(action.floatValue);
				}
				break;
			case ReflectionActionType::AddComponent:
			default:
				break;
			}
		}
	}

	void ApplyObjectReflection(const std::string& className, ObjectBase* object)
	{
		if (!applyReflectionsOnCreate)
		{
			return;
		}

		std::set<std::string> appliedClassNames;
		std::unordered_map<std::string, Component*> componentVariables;
		ApplyObjectReflection(className, object, appliedClassNames, componentVariables, false);
	}

	template <typename BaseType>
	void RegisterObjectPlaceholder(const std::string& className)
	{
		DynamicObjectFactory::GetInstance()->RegisterClass(
			className,
			[className]() -> ObjectBase*
			{
				ObjectBase* object = new RuntimeObjectPlaceholder<BaseType>(className);
				MarkExistingComponentsAsReflected(object);
				ApplyObjectReflection(className, object);
				return object;
			},
			[className](const ObjectBase* object) -> bool
			{
				const auto* placeholder = dynamic_cast<const RuntimeObjectPlaceholder<BaseType>*>(object);
				return placeholder && placeholder->GetRegisteredClassName() == className;
			});
	}

	template <typename BaseType>
	void RegisterComponentPlaceholder(const std::string& className, DynamicComponentOwnerRequirement ownerRequirement)
	{
		DynamicObjectFactory::GetInstance()->RegisterComponentClass(
			className,
			[className](Component* parentComponent) -> Component*
			{
				return new RuntimeComponentPlaceholder<BaseType>(parentComponent, className);
			},
			[className](const Component* component) -> bool
			{
				const auto* placeholder = dynamic_cast<const RuntimeComponentPlaceholder<BaseType>*>(component);
				return placeholder && placeholder->GetRegisteredClassName() == className;
			},
			ownerRequirement);
	}

	void RegisterObjectClass(const std::string& className, const std::string& placeholderBaseName)
	{
		if (placeholderBaseName == "DebugObject")
		{
			RegisterObjectPlaceholder<DebugObject>(className);
		}
		else if (placeholderBaseName == "HUD")
		{
			RegisterObjectPlaceholder<HUD>(className);
		}
		else if (placeholderBaseName == "PhysicsObject")
		{
			RegisterObjectPlaceholder<PhysicsObject>(className);
		}
		else if (placeholderBaseName == "OverlappingPhysicsObject")
		{
			RegisterObjectPlaceholder<OverlappingPhysicsObject>(className);
		}
		else if (placeholderBaseName == "RigidBody")
		{
			RegisterObjectPlaceholder<RigidBody>(className);
		}
		else if (placeholderBaseName == "Character")
		{
			RegisterObjectPlaceholder<Character>(className);
		}
		else if (placeholderBaseName == "ReflectionProbeObject")
		{
			RegisterObjectPlaceholder<ReflectionProbeObject>(className);
		}
		else if (placeholderBaseName == "PlayerStart")
		{
			RegisterObjectPlaceholder<PlayerStart>(className);
		}
		else
		{
			RegisterObjectPlaceholder<ObjectBase>(className);
		}
	}

	void RegisterComponentClass(const std::string& className, const std::string& placeholderBaseName, DynamicComponentOwnerRequirement ownerRequirement)
	{
		if (placeholderBaseName == "SocketComponent")
		{
			RegisterComponentPlaceholder<SocketComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "CameraComponent")
		{
			RegisterComponentPlaceholder<CameraComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "NavigationTreeComponent")
		{
			RegisterComponentPlaceholder<NavigationTreeComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "PointLightComponent")
		{
			RegisterComponentPlaceholder<PointLightComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "DynamicMeshComponent")
		{
			RegisterComponentPlaceholder<DynamicMeshComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "InstancedStaticMeshComponent")
		{
			RegisterComponentPlaceholder<InstancedStaticMeshComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "StaticMeshComponent")
		{
			RegisterComponentPlaceholder<StaticMeshComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "SkeletalMeshComponent")
		{
			RegisterComponentPlaceholder<SkeletalMeshComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "BillboardParticleSystemComponent")
		{
			RegisterComponentPlaceholder<BillboardParticleSystemComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "StaticMeshParticleSystemComponent")
		{
			RegisterComponentPlaceholder<StaticMeshParticleSystemComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "CollisionComponent")
		{
			RegisterComponentPlaceholder<CollisionComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "BoxCollisionComponent")
		{
			RegisterComponentPlaceholder<BoxCollisionComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "CapsuleCollisionComponent")
		{
			RegisterComponentPlaceholder<CapsuleCollisionComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "SphereCollisionComponent")
		{
			RegisterComponentPlaceholder<SphereCollisionComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "MovingTriangleMeshCollisionComponent")
		{
			RegisterComponentPlaceholder<MovingTriangleMeshCollisionComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "NonMovingTriangleMeshCollisionComponent")
		{
			RegisterComponentPlaceholder<NonMovingTriangleMeshCollisionComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "MultipleCollisionComponent")
		{
			RegisterComponentPlaceholder<MultipleCollisionComponent>(className, ownerRequirement);
		}
		else if (placeholderBaseName == "PhysicsMovementComponent")
		{
			RegisterComponentPlaceholder<PhysicsMovementComponent>(className, ownerRequirement);
		}
		else
		{
			RegisterComponentPlaceholder<Component>(className, ownerRequirement);
		}
	}
}

void EditorRuntimeDynamicObjectFactoryRegistrar::RegisterProjectClasses(const std::string& projectRootPath)
{
	const std::filesystem::path sourceRoot = std::filesystem::path(NormalizePath(projectRootPath)) / "Source";
	std::error_code errorCode;
	if (!std::filesystem::exists(sourceRoot, errorCode) || !std::filesystem::is_directory(sourceRoot, errorCode))
	{
		return;
	}

	std::vector<std::filesystem::path> sourceFiles;
	for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(sourceRoot, std::filesystem::directory_options::skip_permission_denied, errorCode))
	{
		if (errorCode)
		{
			break;
		}

		if (entry.is_regular_file(errorCode) && IsSourceFile(entry.path()))
		{
			sourceFiles.push_back(entry.path());
		}
	}
	std::sort(sourceFiles.begin(), sourceFiles.end());

	std::map<std::string, ClassInfo> classInfos;
	for (const std::filesystem::path& sourceFile : sourceFiles)
	{
		ParseClassDeclarations(sourceFile, classInfos);
	}
	ParseProjectReflections(sourceFiles, classInfos);

	std::vector<std::pair<int, std::string>> objectClasses;
	std::vector<std::pair<int, std::string>> componentClasses;
	for (const auto& classInfoPair : classInfos)
	{
		const std::string& className = classInfoPair.first;
		if (kKnownObjectBases.find(className) != kKnownObjectBases.end() || kKnownComponentBases.find(className) != kKnownComponentBases.end())
		{
			continue;
		}

		const int classDepth = GetClassDepth(className, classInfos);
		if (DerivesFrom(className, kKnownObjectBases, classInfos))
		{
			objectClasses.emplace_back(classDepth, className);
		}
		else if (DerivesFrom(className, kKnownComponentBases, classInfos))
		{
			componentClasses.emplace_back(classDepth, className);
		}
	}

	std::sort(objectClasses.begin(), objectClasses.end());
	std::sort(componentClasses.begin(), componentClasses.end());

	for (const auto& objectClass : objectClasses)
	{
		const std::string& className = objectClass.second;
		const std::string placeholderBaseName = GetPlaceholderBase(className, kKnownObjectBases, kAbstractObjectPlaceholderBases, "ObjectBase", classInfos);
		RegisterObjectClass(className, placeholderBaseName);
	}

	for (const auto& componentClass : componentClasses)
	{
		const std::string& className = componentClass.second;
		const std::string placeholderBaseName = GetPlaceholderBase(className, kKnownComponentBases, kAbstractComponentPlaceholderBases, "Component", classInfos);

		DynamicComponentOwnerRequirement ownerRequirement = DynamicComponentOwnerRequirement::ObjectBase;
		if (DerivesFrom(className, kOverlappingComponentBases, classInfos))
		{
			ownerRequirement = DynamicComponentOwnerRequirement::OverlappingPhysicsObject;
		}
		else if (DerivesFrom(className, kPhysicsComponentBases, classInfos))
		{
			ownerRequirement = DynamicComponentOwnerRequirement::PhysicsObject;
		}

		RegisterComponentClass(className, placeholderBaseName, ownerRequirement);
	}
}

void EditorRuntimeDynamicObjectFactoryRegistrar::SetApplyReflectionsOnCreate(bool shouldApplyReflectionsOnCreate)
{
	applyReflectionsOnCreate = shouldApplyReflectionsOnCreate;
}

bool EditorRuntimeDynamicObjectFactoryRegistrar::GetApplyReflectionsOnCreate()
{
	return applyReflectionsOnCreate;
}

void EditorRuntimeDynamicObjectFactoryRegistrar::ClearReflectedComponentMarkers()
{
	reflectedComponents.clear();
}

void EditorRuntimeDynamicObjectFactoryRegistrar::ApplyReflectionsToObject(ObjectBase* object)
{
	if (!object)
	{
		return;
	}

	const std::string className = DynamicObjectFactory::GetInstance()->GetRegisteredObjectClassName(object);
	if (className.empty() || projectObjectReflections.find(className) == projectObjectReflections.end())
	{
		return;
	}

	std::set<std::string> appliedClassNames;
	std::unordered_map<std::string, Component*> componentVariables;
	ApplyObjectReflection(className, object, appliedClassNames, componentVariables, true);
}

bool EditorRuntimeDynamicObjectFactoryRegistrar::IsReflectedComponent(const Component* component)
{
	return component && reflectedComponents.find(component) != reflectedComponents.end();
}
