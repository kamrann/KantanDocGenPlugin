// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "DocGenTaskProcessor.h"	// TUniquePtr seems to need full definition...


/*
Module implementation
*/
class FGraphNodeImagerModule : public FDefaultGameModuleImpl
{
public:
	FGraphNodeImagerModule()
	{}

public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	void GenerateDocs(struct FKantanDocGenSettings const& Settings);

protected:
	void ProcessIntermediateDocs(FString const& IntermediateDir, FString const& OutputDir, FString const& DocTitle, bool bCleanOutput);
	void ShowDocGenUI();

protected:
	TUniquePtr< FDocGenTaskProcessor > Processor;

	TSharedPtr< FUICommandList > UICommands;
};


