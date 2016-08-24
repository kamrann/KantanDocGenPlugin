// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#include "KantanDocGenPCH.h"
#include "KantanDocGenCommands.h"


#define LOCTEXT_NAMESPACE "KantanDocGen"


void FKantanDocGenCommands::RegisterCommands()
{
	UI_COMMAND(ShowDocGenUI, "Kantan DocGen", "Shows the Kantan Doc Gen tab", EUserInterfaceActionType::Button, FInputGesture());
	NameToCommandMap.Add(TEXT("ShowDocGenUI"), ShowDocGenUI);
}


#undef LOCTEXT_NAMESPACE


