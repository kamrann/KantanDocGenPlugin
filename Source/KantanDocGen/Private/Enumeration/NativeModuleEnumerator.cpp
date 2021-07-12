// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#include "NativeModuleEnumerator.h"
#include "KantanDocGenLog.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

FNativeModuleEnumerator::FNativeModuleEnumerator(FName const& InModuleName)
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
	if (Package == nullptr)
	{
		// If it is not in memory, try to load it.
		Package = LoadPackage(nullptr, *PkgName, LOAD_None);
	}
	if (Package == nullptr)
	{
		UE_LOG(LogKantanDocGen, Warning, TEXT("Failed to find specified package '%s', skipping."), *PkgName);
		return;
	}

	// Make sure it's fully loaded (probably unnecessary since only native packages here, but no harm)
	Package->FullyLoad();

	// @TODO: Work out why the below enumeration is called twice for every class in the package (it's called on the
	// exact same uclass instance)
	TSet<UObject*> Processed;

	// Functor to invoke on every object found in the package
	TFunction<void(UObject*)> ObjectEnumFtr = [&](UObject* Obj) {
		// The BP action database appears to be keyed either on native UClass objects, or, in the
		// case of blueprints, on the Blueprint object itself, as opposed to the generated class.

		UObject* ObjectToProcess = nullptr;

		// Native class?
		if (auto Class = Cast<UClass>(Obj))
		{
			if (Class->HasAllClassFlags(CLASS_Native) && !Class->HasAnyFlags(RF_ClassDefaultObject))
			{
				ObjectToProcess = Class;
			}
		}
		if (auto Struct = Cast<UStruct>(Obj))
		{
			if (!Struct->HasAnyFlags(EObjectFlags::RF_ArchetypeObject | EObjectFlags::RF_ClassDefaultObject ) && !Obj->IsA(UFunction::StaticClass()) && !Obj->IsA(UClass::StaticClass()))
			{
				UE_LOG(LogKantanDocGen, Log, TEXT("Found struct '%s' in package '%s'"), *Obj->GetName(), *PkgName);
			}
		}
		if (auto FoundUenum = Cast<UEnum>(Obj))
		{
			if (!FoundUenum->HasAnyFlags(EObjectFlags::RF_ArchetypeObject | EObjectFlags::RF_ClassDefaultObject))
			{
				UE_LOG(LogKantanDocGen, Log, TEXT("Found enum '%s' in package '%s'"), *Obj->GetName(), *PkgName);
			}
		}
		if (ObjectToProcess && !Processed.Contains(ObjectToProcess))
		{
			UE_LOG(LogKantanDocGen, Log, TEXT("Enumerating object '%s' in package '%s'"), *ObjectToProcess->GetName(),
				   *PkgName);

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
	return (float) CurIndex / (ObjectList.Num() - 1);
}

int32 FNativeModuleEnumerator::EstimatedSize() const
{
	return ObjectList.Num();
}
