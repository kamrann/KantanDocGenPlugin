// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#include "KantanDocGenPCH.h"
#include "KantanDocsCommandlet.h"
#include "NodeDocsGenerator.h"


namespace KantanDocsCommandletUtils
{
	/**
	* A collection of variables that represent the various command switches
	* that users can specify when running the commandlet. See the HelpString
	* variable for a listing of supported switches.
	*/
	struct CommandletOptions
	{
		CommandletOptions():
			SourceClass(nullptr)
		{}

		/**
		* Parses the string command switches into flags, class pointers, and
		* booleans that will govern what should be dumped. Logs errors if any
		* switch was misused.
		*/
		CommandletOptions(TArray<FString> const& Switches);

		UClass*    SourceClass;
		FString    OutputDir;
	};

	/** Static instance of the command switches (so we don't have to pass one along the call stack) */
	static CommandletOptions CommandOptions;
}

KantanDocsCommandletUtils::CommandletOptions::CommandletOptions(TArray<FString> const& Switches)
	: SourceClass(AActor::StaticClass())
{
	for(FString const& Switch : Switches)
	{
		if(Switch.StartsWith("class="))
		{
			FString ClassSwitch, ClassName;
			Switch.Split(TEXT("="), &ClassSwitch, &ClassName);
			if(auto Class = FindObject<UClass>(ANY_PACKAGE, *ClassName))
			{
				SourceClass = Class;
			}
			else
			{
				UE_LOG(LogKantanDocGen, Error, TEXT("Unrecognized class '%s', defaulting to 'Actor'"), *ClassName);
			}
		}
		else if(Switch.StartsWith("path="))
		{
			FString PathSwitch;
			Switch.Split(TEXT("="), &PathSwitch, &OutputDir);
		}
	}
}


int32 UKantanDocsCommandlet::Main(FString const& Params)
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	KantanDocsCommandletUtils::CommandOptions = KantanDocsCommandletUtils::CommandletOptions(Switches);
	KantanDocsCommandletUtils::CommandletOptions const& CommandOptions = KantanDocsCommandletUtils::CommandOptions;
#if 0
	{
		FNodeDocsGenerator Gen;
		if(Gen.Init("Temp"))
		{
			//for(auto const& ClsName : Args)
			{
				//if(auto Class = FindObject< UClass >(ANY_PACKAGE, *ClsName))
				auto Class = CommandOptions.SourceClass;
				{
					Gen.ProcessSourceObject(Class, CommandOptions.OutputDir);
				}
			}
		}
	}
#endif
	return 0;
}


