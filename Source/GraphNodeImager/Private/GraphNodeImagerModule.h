// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#pragma once

#include "ModuleManager.h"


/*
Module implementation
*/
class FGraphNodeImagerModule : public FDefaultGameModuleImpl
{
public:
	FGraphNodeImagerModule():
		DumpNodes_CCmd(
			TEXT("imagenodes"),
			TEXT("Dumps images of graph nodes to file."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FGraphNodeImagerModule::DumpNodes)
		)
	{}

public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:
	void DumpNodes(TArray< FString > const& Args);
	void ProcessIntermediateDocs(FString const& IntermediateDir, FString const& OutputDir, FString const& DocTitle, bool bCleanOutput);

protected:
	FAutoConsoleCommand DumpNodes_CCmd;
};


