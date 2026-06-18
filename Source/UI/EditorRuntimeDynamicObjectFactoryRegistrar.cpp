#include "EditorRuntimeDynamicObjectFactoryRegistrar.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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

	template <typename BaseType>
	void RegisterObjectPlaceholder(const std::string& className)
	{
		DynamicObjectFactory::GetInstance()->RegisterClass(
			className,
			[className]() -> ObjectBase*
			{
				return new RuntimeObjectPlaceholder<BaseType>(className);
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
