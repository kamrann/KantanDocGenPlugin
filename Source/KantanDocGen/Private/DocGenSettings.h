// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#pragma once

#include "DocGenOutput.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "OutputFormats/DocGenSerializerFactory.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"

#include "DocGenSettings.generated.h"

USTRUCT()
struct FKantanDocGenSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Documentation")
	FString DocumentationTitle;

	/** List of C++ modules in which to search for blueprint-exposed classes to document. */
	UPROPERTY(EditAnywhere, Category = "Class Search",
			  Meta = (Tooltip = "Raw module names (Do not prefix with '/Script')."))
	TArray<FName> NativeModules;

	/** List of paths in which to search for blueprints to document. */
	UPROPERTY(EditAnywhere, Category = "Class Search",
			  Meta = (ContentDir)) //, Meta = (Tooltip = "Path to content subfolder, e.g. '/Game/MyFolder' or
								   //'/PluginName/MyFolder'."))
	// TArray< FName > ContentPaths;
	TArray<FDirectoryPath> ContentPaths;

	/** Names of specific classes/blueprints to document. */
	UPROPERTY() // EditAnywhere, Category = "Class Search")
	TArray<FName> SpecificClasses;

	/** Names of specific classes/blueprints to exclude. */
	UPROPERTY() // EditAnywhere, Category = "Class Search")
	TArray<FName> ExcludedClasses;

	UPROPERTY(EditAnywhere, Category = "Output")
	FDirectoryPath OutputDirectory;

	UPROPERTY(EditAnywhere, Category = "Class Search", AdvancedDisplay)
	TSubclassOf<UObject> BlueprintContextClass;

	UPROPERTY(EditAnywhere, meta = (MustImplement="DocGenSerializerFactory"),Category = "Output")
	TArray<TSubclassOf<UObject>> OutputFormats;

	UPROPERTY(EditAnywhere, Category = "Output")
	bool bCleanOutputDirectory;

public:
	FKantanDocGenSettings()
	{
		BlueprintContextClass = AActor::StaticClass();
		bCleanOutputDirectory = false;
	}

	bool HasAnySources() const
	{
		return NativeModules.Num() > 0 || ContentPaths.Num() > 0 || SpecificClasses.Num() > 0;
	}
};

UCLASS(Config = EditorPerProjectUserSettings)
class UKantanDocGenSettingsObject : public UObject
{
	GENERATED_BODY()

public:
	static UKantanDocGenSettingsObject* Get()
	{
		static bool bInitialized = false;

		// This is a singleton, use default object
		auto DefaultSettings = GetMutableDefault<UKantanDocGenSettingsObject>();

		if (!bInitialized)
		{
			InitDefaults(DefaultSettings);

			bInitialized = true;
		}

		return DefaultSettings;
	}

	static void InitDefaults(UKantanDocGenSettingsObject* CDO)
	{
		if (CDO->Settings.DocumentationTitle.IsEmpty())
		{
			CDO->Settings.DocumentationTitle = FApp::GetProjectName();
		}

		if (CDO->Settings.OutputDirectory.Path.IsEmpty())
		{
			CDO->Settings.OutputDirectory.Path = FPaths::ProjectSavedDir() / TEXT("KantanDocGen");
		}

		if (CDO->Settings.BlueprintContextClass == nullptr)
		{
			CDO->Settings.BlueprintContextClass = AActor::StaticClass();
		}
	}

public:
	UPROPERTY(EditAnywhere, Config, Category = "Kantan DocGen", Meta = (ShowOnlyInnerProperties))
	FKantanDocGenSettings Settings;
};
