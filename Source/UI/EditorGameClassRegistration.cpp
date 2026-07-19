#include "EditorGameClassRegistration.h"

#include <algorithm>

namespace
{
	std::vector<EditorGameClassRegistration::ClassMetadata> registeredClassMetadata_;
}

void EditorGameClassRegistration::RegisterClassMetadata(const ClassMetadata& classMetadata)
{
	if (classMetadata.className.empty())
	{
		return;
	}

	const auto existingClassIterator = std::find_if(
		registeredClassMetadata_.begin(),
		registeredClassMetadata_.end(),
		[&classMetadata](const ClassMetadata& registeredClassMetadata)
		{
			return registeredClassMetadata.className == classMetadata.className;
		});

	if (existingClassIterator != registeredClassMetadata_.end())
	{
		*existingClassIterator = classMetadata;
		return;
	}

	registeredClassMetadata_.push_back(classMetadata);
}

const std::vector<EditorGameClassRegistration::ClassMetadata>& EditorGameClassRegistration::GetRegisteredClassMetadata()
{
	return registeredClassMetadata_;
}
