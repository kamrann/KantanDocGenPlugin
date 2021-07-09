// Fill out your copyright notice in the Description page of Project Settings.

#include "DocGenCommandlet.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "DocGenSettings.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "KantanDocGenModule.h"
#include "Modules/ModuleManager.h"

UDocGenCommandlet::UDocGenCommandlet()
{
	HelpParamNames.Add("doctitle");
	HelpParamDescriptions.Add("Title of the documentation");

	HelpParamNames.Add("nativemodules");
	HelpParamDescriptions.Add("Comma-separated list of native modules to search");

	HelpParamNames.Add("contentpaths");
	HelpParamDescriptions.Add(
		"Comma-separated list of content subfolders, eg /PluginName/Subfolder or /Game/Subfolder");

	HelpParamNames.Add("includeclass");
	HelpParamDescriptions.Add("Comma-separated list of classes to document");

	HelpParamNames.Add("excludeclass");
	HelpParamDescriptions.Add("Comma-separated list of classes to exclude");

	HelpParamNames.Add("outputdir");
	HelpParamDescriptions.Add("Directory to place processed documentation in");

	HelpParamNames.Add("contextclass");
	HelpParamDescriptions.Add("Blueprint Context Class");

	HelpParamNames.Add("formats");
	HelpParamDescriptions.Add("Comma-separated list of output formats to generate");

	HelpParamNames.Add("cleanoutput");
	HelpParamDescriptions.Add("cleans the output directory before generating the documentation");
}

int32 UDocGenCommandlet::Main(const FString& Params)
{
	Super::Main(Params);
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParsedParams;
	ParseCommandLine(*Params, Tokens, Switches, ParsedParams);
	UE_LOG(LogTemp, Display, TEXT("Tokens:"));
	for (const FString& Token : Tokens)
	{
		UE_LOG(LogTemp, Display, TEXT("%s"), *Token);
	}
	UE_LOG(LogTemp, Display, TEXT("Switches:"));
	for (const FString& Switch : Switches)
	{
		UE_LOG(LogTemp, Display, TEXT("%s"), *Switch);
	}
	UE_LOG(LogTemp, Display, TEXT("Params:"));
	for (const auto& Param : ParsedParams)
	{
		UE_LOG(LogTemp, Display, TEXT("%s : %s"), *Param.Key, *Param.Value);
	}
	FKantanDocGenSettings Settings;
	// Defaults
	Settings.DocumentationTitle = FApp::GetProjectName();
	Settings.OutputDirectory.Path = FPaths::ProjectSavedDir() / TEXT("KantanDocGen");
	Settings.BlueprintContextClass = AActor::StaticClass();

	if (ParsedParams.Contains("doctitle"))
	{
		Settings.DocumentationTitle = ParsedParams["doctitle"];
	}

	if (ParsedParams.Contains("nativemodules"))
	{
		TArray<FString> Values;
		ParsedParams["nativemodules"].ParseIntoArray(Values, TEXT(","));
		for (const auto& Value : Values)
		{
			Settings.NativeModules.Add(FName(Value));
		}
	}

	if (ParsedParams.Contains("contentpaths"))
	{
		TArray<FString> Values;
		ParsedParams["contentpaths"].ParseIntoArray(Values, TEXT(","));
		for (const auto& Value : Values)
		{
			Settings.ContentPaths.Add(FDirectoryPath {Value});
		}
	}

	if (ParsedParams.Contains("includeclass"))
	{
		TArray<FString> Values;
		ParsedParams["includeclass"].ParseIntoArray(Values, TEXT(","));
		for (const auto& Value : Values)
		{
			Settings.SpecificClasses.Add(FName(Value));
		}
	}

	if (ParsedParams.Contains("excludeclass"))
	{
		TArray<FString> Values;
		ParsedParams["excludeclass"].ParseIntoArray(Values, TEXT(","));
		for (const auto& Value : Values)
		{
			Settings.ExcludedClasses.Add(FName(Value));
		}
	}

	if (ParsedParams.Contains("outputdir"))
	{
		Settings.OutputDirectory.Path = ParsedParams["outputdir"];
	}

	if (ParsedParams.Contains("formats"))
	{
		auto OutputFormatFactories = GetAllOutputFormatFactories();

		TArray<FString> Values;
		ParsedParams["formats"].ParseIntoArray(Values, TEXT(","));
		for (const auto& Value : Values)
		{
			for (const auto& Factory : OutputFormatFactories)
			{
				auto FactoryObject = NewObject<UObject>(GetTransientPackage(), Factory);
				const auto& FactoryInterface = Cast<IDocGenOutputFormatFactory>(FactoryObject);
				if (!FactoryInterface)
				{
					continue;
				}
				if (FactoryInterface->GetFormatIdentifier() == Value)
				{
					Settings.OutputFormats.Add(Factory);
				}
			}
		}
	}
	if (Switches.Contains("cleanoutput"))
	{
		Settings.bCleanOutputDirectory = true;
	}
	auto& Module = FModuleManager::LoadModuleChecked<FKantanDocGenModule>(TEXT("KantanDocGen"));
	auto GenerateDocsResult = Module.GenerateDocs(Settings);
	while (!GenerateDocsResult.IsReady())
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FTicker::GetCoreTicker().Tick(FApp::GetDeltaTime());
		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();
		FPlatformProcess::Sleep(0);
	}

	return 0;
}

void UDocGenCommandlet::CreateCustomEngine(const FString& Params)
{
	FSlateApplication::InitializeAsStandaloneApplication(
		FModuleManager::Get().GetModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlateRHIRenderer());
}

TArray<UClass*> UDocGenCommandlet::GetAllOutputFormatFactories()
{
	TArray<UClass*> OutputFormatFactories;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;

		// Only interested in native C++ classes
		if (!Class->IsNative())
		{
			continue;
		}

		// Ignore deprecated
		if (Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		// Check this class inplements our factory
		if (!Class->ImplementsInterface(UDocGenOutputFormatFactory::StaticClass()))
		{
			continue;
		}

		// Add this class
		OutputFormatFactories.Add(Class);
	}
	return OutputFormatFactories;
}
