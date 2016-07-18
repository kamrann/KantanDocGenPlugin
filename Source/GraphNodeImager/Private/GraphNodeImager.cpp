// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#include "GraphNodeImager.h"
#include "GraphNodeImagerModule.h"
#include "GraphNodeImagerCommands.h"
#include "IConsoleManager.h"
#include "IMainFrameModule.h"
#include "LevelEditor.h"
//#include "NodeDocsGenerator.h"
#include "SKantanDocGenWidget.h"
//#include "Enumeration/NativeModuleEnumerator.h"
//#include "Enumeration/ContentPathEnumerator.h"
//#include "Enumeration/CompositeEnumerator.h"
#include "DocGenSettings.h"
#include "DocGenTaskProcessor.h"

#define LOCTEXT_NAMESPACE "GraphNodeImager"


IMPLEMENT_MODULE(FGraphNodeImagerModule, GraphNodeImager)

DEFINE_LOG_CATEGORY(LogGraphNodeImager);


void FGraphNodeImagerModule::StartupModule()
{
	{
		// Create command list
		UICommands = MakeShareable< FUICommandList >(new FUICommandList);

		FGraphNodeImagerCommands::Register();

		// Map commands
		FUIAction ShowDocGenUI_UIAction(
			FExecuteAction::CreateRaw(this, &FGraphNodeImagerModule::ShowDocGenUI),
			FCanExecuteAction::CreateLambda([] { return true; })
		);

		auto CmdInfo = FGraphNodeImagerCommands::Get().ShowDocGenUI;
		UICommands->MapAction(CmdInfo, ShowDocGenUI_UIAction);

		// Setup menu extension
		auto AddMenuExtension = [](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(FGraphNodeImagerCommands::Get().ShowDocGenUI);
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

void FGraphNodeImagerModule::ShutdownModule()
{
	FGraphNodeImagerCommands::Unregister();
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

void FGraphNodeImagerModule::GenerateDocs(FKantanDocGenSettings const& Settings)
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

#if 0
	TArray< TUniquePtr< ISourceObjectEnumerator > > Enumerators;
	// @TODO: Specific class enumerator
	Enumerators.Add(MakeUnique< FCompositeEnumerator< FNativeModuleEnumerator > >(Settings.NativeModules));
	Enumerators.Add(MakeUnique< FCompositeEnumerator< FContentPathEnumerator > >(Settings.ContentPaths));

	FString IntermediateDir = FPaths::GameIntermediateDir() / TEXT("KantanDocGen");

	// Initialize the doc generator
	auto Gen = MakeUnique< FNodeDocsGenerator >();
	if(Gen->Init(Settings.DocumentationTitle))
	{
		FNotificationInfo Info(LOCTEXT("DocGenInProgress", "Doc gen in progress"));
		Info.Image = FEditorStyle::GetBrush(TEXT("LevelEditor.RecompileGameCode"));
		Info.FadeInDuration = 0.1f;
		Info.FadeOutDuration = 0.5f;
		//Info.ExpireDuration = 1.5f;
		Info.bUseThrobber = true;
		Info.bUseSuccessFailIcons = false;
		Info.bUseLargeFont = true;
		Info.bFireAndForget = false;
		Info.bAllowThrottleWhenFrameRateIsLow = false;
		auto NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
		//NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
		//NotificationItem->ExpireAndFadeout();
		//GEditor->PlayEditorSound(CompileSuccessSound);


		bool const bCleanIntermediate = true;
		if(bCleanIntermediate)
		{
			IFileManager::Get().DeleteDirectory(*IntermediateDir, false, true);
		}

		TSet< FName > Excluded;
		for(auto const& Name : Settings.ExcludedClasses)
		{
			Excluded.Add(Name);
		}

		double ObjectProcessingTime = 0.0;
		double EnumTime = 0.0;

		TSet< UObject* > Processed;
		int32 NodeCount = 0;
		for(auto& Enumerator : Enumerators)
		{
			double EnumStart = FPlatformTime::Seconds();

			while(auto Obj = Enumerator->GetNext())
			{
				EnumTime += FPlatformTime::Seconds() - EnumStart;

				if(Excluded.Contains(Obj->GetFName()))
				{
					continue;
				}

				{
					SCOPE_SECONDS_COUNTER(ObjectProcessingTime);

					NodeCount += Gen->ProcessSourceObject(Obj, IntermediateDir);
				}

				EnumStart = FPlatformTime::Seconds();
			}

			EnumTime += FPlatformTime::Seconds() - EnumStart;
		}

		Gen->Finalize(IntermediateDir);

		UE_LOG(LogGraphNodeImager, Log, TEXT("Intermediate doc gen timing:"));
		UE_LOG(LogGraphNodeImager, Log, TEXT("Enumeration: %.3fs"), EnumTime);
		UE_LOG(LogGraphNodeImager, Log, TEXT("Processing: %.3fs (Image gen: %.3fs, Doc gen: %.3fs)"), ObjectProcessingTime, Gen->GenerateNodeImageTime, Gen->GenerateNodeDocsTime);

		// Destroy the generator, which will also kill the host window.
		Gen.Reset();

		if(NodeCount > 0)
		{
			UE_LOG(LogGraphNodeImager, Log, TEXT("Intermediate docs generated for %i nodes."), NodeCount);

			SCOPE_LOG_TIME_IN_SECONDS("ProcessIntermediateDocs", nullptr);

			ProcessIntermediateDocs(IntermediateDir, Settings.OutputDirectory.Path, Settings.DocumentationTitle, Settings.bCleanOutputDirectory);
		}
		else
		{
			UE_LOG(LogGraphNodeImager, Warning, TEXT("No nodes documented!"));
		}
	}
	else
	{
		UE_LOG(LogGraphNodeImager, Error, TEXT("Failed to initialize doc generator, aborting."));
	}
#endif
}

void FGraphNodeImagerModule::ShowDocGenUI()
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


