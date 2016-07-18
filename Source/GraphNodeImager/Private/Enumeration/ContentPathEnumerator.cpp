// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#include "GraphNodeImager.h"
#include "ContentPathEnumerator.h"
#include "AssetRegistryModule.h"


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
			UE_LOG(LogGraphNodeImager, Log, TEXT("Enumerating object '%s' at '%s'"), *Blueprint->GetName(), *AssetData.ObjectPath.ToString());

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

