// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#pragma once

#include "Editor/EditorStyle/Public/EditorStyleSet.h"


class FKantanDocGenCommands : public TCommands< FKantanDocGenCommands >
{
public:
	FKantanDocGenCommands() : TCommands< FKantanDocGenCommands >
	(
		"KantanDocGen", // Context name for fast lookup
		NSLOCTEXT("Contexts", "KantanDocGen", "Kantan Doc Gen"), // Localized context name for displaying
		NAME_None, // Parent
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
	{
	}
	
	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

public:
	// Mode Switch
	TSharedPtr< FUICommandInfo > ShowDocGenUI;

	// Map
	TMap< FName, TSharedPtr< FUICommandInfo > > NameToCommandMap;
};


