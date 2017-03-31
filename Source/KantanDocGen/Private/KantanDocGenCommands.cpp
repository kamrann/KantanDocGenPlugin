// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#include "KantanDocGenCommands.h"


#define LOCTEXT_NAMESPACE "KantanDocGen"


void FKantanDocGenCommands::RegisterCommands()
{
	UI_COMMAND(ShowDocGenUI, "Kantan DocGen", "Shows the Kantan Doc Gen tab", EUserInterfaceActionType::Button, FInputGesture());
	NameToCommandMap.Add(TEXT("ShowDocGenUI"), ShowDocGenUI);
}


#undef LOCTEXT_NAMESPACE


