// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#pragma once

#include "DocGenTaskProcessor.h" // TUniquePtr seems to need full definition...
#include "Modules/ModuleManager.h"

class FUICommandList;

/*
Module implementation
*/
class FKantanDocGenModule : public FDefaultGameModuleImpl
{
public:
	FKantanDocGenModule() {}

public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/// @brief Check if we are currently processing documentation. Intended only for use in commandlets
	/// @return true if the processor is valid and currently running
	bool IsProcessorRunning();

public:
	TFuture<void> GenerateDocs(struct FKantanDocGenSettings const& Settings);

protected:
	void ProcessIntermediateDocs(FString const& IntermediateDir, FString const& OutputDir, FString const& DocTitle,
								 bool bCleanOutput);
	void ShowDocGenUI();

protected:
	TUniquePtr<FDocGenTaskProcessor> Processor;

	TSharedPtr<FUICommandList> UICommands;
};
