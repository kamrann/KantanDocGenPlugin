// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#pragma once

#include "Editor/EditorStyle/Public/EditorStyleSet.h"


class FGraphNodeImagerCommands : public TCommands< FGraphNodeImagerCommands >
{
public:
	FGraphNodeImagerCommands() : TCommands< FGraphNodeImagerCommands >
	(
		"GraphNodeImager", // Context name for fast lookup
		NSLOCTEXT("Contexts", "GraphNodeImager", "Graph Node Imager"), // Localized context name for displaying
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


