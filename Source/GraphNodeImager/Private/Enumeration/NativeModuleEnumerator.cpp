// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#include "GraphNodeImager.h"
#include "NativeModuleEnumerator.h"


FNativeModuleEnumerator::FNativeModuleEnumerator(
	FName const& InModuleName
)
{
	CurIndex = 0;

	Prepass(InModuleName);
}

void FNativeModuleEnumerator::Prepass(FName const& ModuleName)
{
	// For native package, all classes are already loaded so it's no problem to fully enumerate during prepass.
	// That way we have more info for progress estimation.

	// Attempt to find the package
	auto PkgName = TEXT("/Script/") + ModuleName.ToString();

	auto Package = FindPackage(nullptr, *PkgName);
	if(Package == nullptr)
	{
		// If it is not in memory, try to load it.
		Package = LoadPackage(nullptr, *PkgName, LOAD_None);
	}
	if(Package == nullptr)
	{
		UE_LOG(LogGraphNodeImager, Warning, TEXT("Failed to find specified package '%s', skipping."), *PkgName);
		return;
	}

	// Make sure it's fully loaded (probably unnecessary since only native packages here, but no harm)
	Package->FullyLoad();

	// @TODO: Work out why the below enumeration is called twice for every class in the package (it's called on the exact same uclass instance)
	TSet< UObject* > Processed;

	// Functor to invoke on every object found in the package
	TFunction< void(UObject*) > ObjectEnumFtr = [&](UObject* Obj)
	{
		// The BP action database appears to be keyed either on native UClass objects, or, in the
		// case of blueprints, on the Blueprint object itself, as opposed to the generated class.

		UObject* ObjectToProcess = nullptr;

		// Native class?
		if(auto Class = Cast< UClass >(Obj))
		{
			if(Class->HasAllClassFlags(CLASS_Native) && !Class->HasAnyFlags(RF_ClassDefaultObject))
			{
				ObjectToProcess = Class;
			}
		}

		if(ObjectToProcess && !Processed.Contains(ObjectToProcess))
		{
			UE_LOG(LogGraphNodeImager, Log, TEXT("Enumerating object '%s' in package '%s'"), *ObjectToProcess->GetName(), *PkgName);

			// Store this class
			ObjectList.Add(ObjectToProcess);

			Processed.Add(ObjectToProcess);
		}
	};

	// Enumerate all objects in the package
	ForEachObjectWithOuter(Package, ObjectEnumFtr, true /* Include nested */);
}

UObject* FNativeModuleEnumerator::GetNext()
{
	return CurIndex < ObjectList.Num() ? ObjectList[CurIndex++].Get() : nullptr;
}

float FNativeModuleEnumerator::EstimateProgress() const
{
	return (float)CurIndex / (ObjectList.Num() - 1);
}

int32 FNativeModuleEnumerator::EstimatedSize() const
{
	return ObjectList.Num();
}

