// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#include "GraphNodeImager.h"
#include "GraphNodeImagerCommands.h"


#define LOCTEXT_NAMESPACE "GraphNodeImager"


void FGraphNodeImagerCommands::RegisterCommands()
{
	UI_COMMAND(ShowDocGenUI, "Show Kantan DocGen", "Shows the Kantan Doc Gen Tab", EUserInterfaceActionType::Button, FInputGesture());
	NameToCommandMap.Add(TEXT("ShowDocGenUI"), ShowDocGenUI);
}


#undef LOCTEXT_NAMESPACE


