// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#include "KantanDocGenModule.h"
#include "KantanDocGenLog.h"
#include "KantanDocGenCommands.h"
#include "DocGenSettings.h"
#include "DocGenTaskProcessor.h"
#include "UI/SKantanDocGenWidget.h"

#include "HAL/IConsoleManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/RunnableThread.h"
#include "Async/Async.h"


#define LOCTEXT_NAMESPACE "KantanDocGen"


IMPLEMENT_MODULE(FKantanDocGenModule, KantanDocGen)

DEFINE_LOG_CATEGORY(LogKantanDocGen);


void FKantanDocGenModule::StartupModule()
{
	{
		// Create command list
		UICommands = MakeShared< FUICommandList >();

		FKantanDocGenCommands::Register();

		// Map commands
		FUIAction ShowDocGenUI_UIAction(
			FExecuteAction::CreateRaw(this, &FKantanDocGenModule::ShowDocGenUI),
			FCanExecuteAction::CreateLambda([] { return true; })
		);

		auto CmdInfo = FKantanDocGenCommands::Get().ShowDocGenUI;
		UICommands->MapAction(CmdInfo, ShowDocGenUI_UIAction);

		// Setup menu extension
		auto AddMenuExtension = [](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(FKantanDocGenCommands::Get().ShowDocGenUI);
		};

		auto& LevelEditorModule = FModuleManager::LoadModuleChecked< FLevelEditorModule >("LevelEditor");
		TSharedRef< FExtender > MenuExtender(new FExtender());
		MenuExtender->AddMenuExtension(
			TEXT("FileProject"),
			EExtensionHook::After,
			UICommands.ToSharedRef(),
			FMenuExtensionDelegate::CreateLambda(AddMenuExtension)
		);
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
}

void FKantanDocGenModule::ShutdownModule()
{
	FKantanDocGenCommands::Unregister();
}

// @TODO: Idea was to allow quoted values containing spaces, but this isn't possible since the initial console string has
// already been split by whitespace, ignoring quotes...
inline bool MatchPotentiallyQuoted(const TCHAR* Stream, const TCHAR* Match, FString& Value)
{
	while((*Stream == ' ') || (*Stream == 9))
		Stream++;

	if(FCString::Strnicmp(Stream, Match, FCString::Strlen(Match)) == 0)
	{
		Stream += FCString::Strlen(Match);

		return FParse::Token(Stream, Value, false);
	}
	
	return false;
}

bool FKantanDocGenModule::IsProcessorRunning()
{
	return (Processor.IsValid() && Processor->IsRunning());
}

TFuture<void> FKantanDocGenModule::GenerateDocs(FKantanDocGenSettings const& Settings)
{
	if(!Processor.IsValid())
	{
		Processor = MakeUnique< FDocGenTaskProcessor >();
	}
	
	Processor->QueueTask(Settings);

	if(!Processor->IsRunning())
	{
		return Async(EAsyncExecution::Thread, [Processor = Processor.Get()](){Processor->Run();});
		//FRunnableThread::Create(Processor.Get(), TEXT("KantanDocGenProcessorThread"), 0, TPri_BelowNormal);
	}
	TPromise<void> EmptyPromise;
	EmptyPromise.SetValue();
	return EmptyPromise.GetFuture();
}

void FKantanDocGenModule::ShowDocGenUI()
{
	const FText WindowTitle = LOCTEXT("DocGenWindowTitle", "Kantan Doc Gen");

	TSharedPtr< SWindow > Window =
		SNew(SWindow)
		.Title(WindowTitle)
		.MinWidth(400.0f)
		.MinHeight(300.0f)
		.MaxHeight(600.0f)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::Autosized)
		;

	TSharedRef< SWidget > DocGenContent = SNew(SKantanDocGenWidget);
	Window->SetContent(DocGenContent);

	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked< IMainFrameModule >("MainFrame");
	TSharedPtr< SWindow > ParentWindow = MainFrame.GetParentWindow();

	if(ParentWindow.IsValid())
	{
		FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), ParentWindow.ToSharedRef());

		auto Settings = UKantanDocGenSettingsObject::Get();
		Settings->SaveConfig();
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window.ToSharedRef());
	}
}


#undef LOCTEXT_NAMESPACE


