// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#include "KantanDocGenPCH.h"
#include "KantanDocGenModule.h"
#include "KantanDocGenCommands.h"
#include "IConsoleManager.h"
#include "IMainFrameModule.h"
#include "LevelEditor.h"
#include "DocGenSettings.h"
#include "DocGenTaskProcessor.h"
#include "UI/SKantanDocGenWidget.h"

#define LOCTEXT_NAMESPACE "KantanDocGen"


IMPLEMENT_MODULE(FKantanDocGenModule, KantanDocGen)

DEFINE_LOG_CATEGORY(LogKantanDocGen);


void FKantanDocGenModule::StartupModule()
{
	{
		// Create command list
		UICommands = MakeShareable< FUICommandList >(new FUICommandList);

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

void FKantanDocGenModule::GenerateDocs(FKantanDocGenSettings const& Settings)
{
	if(!Processor.IsValid())
	{
		Processor = MakeUnique< FDocGenTaskProcessor >();
	}
	
	Processor->QueueTask(Settings);

	if(!Processor->IsRunning())
	{
		FRunnableThread::Create(Processor.Get(), TEXT("KantanDocGenProcessorThread"), 0, TPri_BelowNormal);
	}
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
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window.ToSharedRef());
	}
}


#undef LOCTEXT_NAMESPACE


