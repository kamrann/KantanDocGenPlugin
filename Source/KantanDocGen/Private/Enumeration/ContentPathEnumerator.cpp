// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#include "ContentPathEnumerator.h"
#include "KantanDocGenLog.h"
#include "AssetRegistryModule.h"
#include "ARFilter.h"
#include "Engine/Blueprint.h"


FContentPathEnumerator::FContentPathEnumerator(
	FName const& InPath
)
{
	CurIndex = 0;

	Prepass(InPath);
}

void FContentPathEnumerator::Prepass(FName const& Path)
{
	auto& AssetRegistryModule = FModuleManager::LoadModuleChecked< FAssetRegistryModule >("AssetRegistry");
	auto& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
	//Filter.RecursiveClassesExclusionSet.Add();

	AssetRegistry.GetAssetsByPath(Path, AssetList, true);
	AssetRegistry.RunAssetsThroughFilter(AssetList, Filter);
}

UObject* FContentPathEnumerator::GetNext()
{
	UObject* Result = nullptr;

	while(CurIndex < AssetList.Num())
	{
		auto const& AssetData = AssetList[CurIndex];
		++CurIndex;

		if(auto Blueprint = Cast< UBlueprint >(AssetData.GetAsset()))
		{
			UE_LOG(LogKantanDocGen, Log, TEXT("Enumerating object '%s' at '%s'"), *Blueprint->GetName(), *AssetData.ObjectPath.ToString());

			Result = Blueprint;
			break;
		}
	}
	
	return Result;
}

float FContentPathEnumerator::EstimateProgress() const
{
	return (float)CurIndex / (AssetList.Num() - 1);
}

int32 FContentPathEnumerator::EstimatedSize() const
{
	return AssetList.Num();
}

