// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#include "DocGenTaskProcessor.h"
#include "Async/TaskGraphInterfaces.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintNodeSpawner.h"
#include "Enumeration/CompositeEnumerator.h"
#include "Enumeration/ContentPathEnumerator.h"
#include "Enumeration/ISourceObjectEnumerator.h"
#include "Enumeration/NativeModuleEnumerator.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "K2Node.h"
#include "KantanDocGenLog.h"
#include "NodeDocsGenerator.h"
#include "OutputFormats/DocGenOutputFormatFactory.h"
#include "OutputFormats/DocGenOutputProcessor.h"
#include "ThreadingHelpers.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "KantanDocGen"

FDocGenTaskProcessor::FDocGenTaskProcessor()
{
	bRunning = false;
	bTerminationRequest = false;
}

void FDocGenTaskProcessor::QueueTask(FKantanDocGenSettings const& Settings)
{
	TSharedPtr<FDocGenTask> NewTask = MakeShared<FDocGenTask>();
	NewTask->Settings = Settings;

	FNotificationInfo Info(LOCTEXT("DocGenWaiting", "Doc gen waiting"));
	Info.Image = nullptr; // FEditorStyle::GetBrush(TEXT("LevelEditor.RecompileGameCode"));
	Info.FadeInDuration = 0.2f;
	Info.ExpireDuration = 5.0f;
	Info.FadeOutDuration = 1.0f;
	Info.bUseThrobber = true;
	Info.bUseSuccessFailIcons = true;
	Info.bUseLargeFont = true;
	Info.bFireAndForget = false;
	Info.bAllowThrottleWhenFrameRateIsLow = false;
	NewTask->Notification = FSlateNotificationManager::Get().AddNotification(Info);
	NewTask->Notification->SetCompletionState(SNotificationItem::CS_Pending);

	Waiting.Enqueue(NewTask);
}

bool FDocGenTaskProcessor::IsRunning() const
{
	return bRunning;
}

bool FDocGenTaskProcessor::Init()
{
	bRunning = true;
	return true;
}

uint32 FDocGenTaskProcessor::Run()
{
	TSharedPtr<FDocGenTask> Next;
	while (!bTerminationRequest && Waiting.Dequeue(Next))
	{
		ProcessTask(Next);
	}

	return 0;
}

void FDocGenTaskProcessor::Exit()
{
	bRunning = false;
}

void FDocGenTaskProcessor::Stop()
{
	bTerminationRequest = true;
}

void FDocGenTaskProcessor::ProcessTask(TSharedPtr<FDocGenTask> InTask)
{
	/********** Lambdas for the game thread to execute **********/

	auto GameThread_InitDocGen = [this](FString const& DocTitle, FString const& IntermediateDir) -> bool {
		Current->Task->Notification->SetExpireDuration(2.0f);
		Current->Task->Notification->SetText(LOCTEXT("DocGenInProgress", "Doc gen in progress"));

		return Current->DocGen->GT_Init(DocTitle, IntermediateDir, Current->Task->Settings.BlueprintContextClass);
	};

	TFunction<void()> GameThread_EnqueueEnumerators = [this]() {
		// @TODO: Specific class enumerator
		Current->Enumerators.Enqueue(
			MakeShared<FCompositeEnumerator<FNativeModuleEnumerator>>(Current->Task->Settings.NativeModules));

		TArray<FName> ContentPackagePaths;
		for (auto const& Path : Current->Task->Settings.ContentPaths)
		{
			ContentPackagePaths.AddUnique(FName(*Path.Path));
		}
		Current->Enumerators.Enqueue(MakeShared<FCompositeEnumerator<FContentPathEnumerator>>(ContentPackagePaths));
	};

	auto GameThread_EnumerateNextObject = [this]() -> bool {
		Current->SourceObject.Reset();
		Current->CurrentSpawners.Empty();

		while (auto Obj = Current->CurrentEnumerator->GetNext())
		{
			// Ignore if already processed
			if (Current->Processed.Contains(Obj))
			{
				continue;
			}

			// Cache list of spawners for this object
			auto& BPActionMap = FBlueprintActionDatabase::Get().GetAllActions();
			if (auto ActionList = BPActionMap.Find(Obj))
			{
				if (ActionList->Num() == 0)
				{
					continue;
				}

				Current->SourceObject = Obj;
				for (auto Spawner : *ActionList)
				{
					// Add to queue as weak ptr
					check(Current->CurrentSpawners.Enqueue(Spawner));
				}

				// Done
				Current->Processed.Add(Obj);
				return true;
			}
		}

		// This enumerator is finished
		return false;
	};

	auto GameThread_EnumerateNextNode = [this](FNodeDocsGenerator::FNodeProcessingState& OutState) -> UK2Node* {
		// We've just come in from another thread, check the source object is still around
		if (!Current->SourceObject.IsValid())
		{
			UE_LOG(LogKantanDocGen, Warning, TEXT("Object being enumerated expired!"));
			return nullptr;
		}

		// Try to grab the next spawner in the cached list
		TWeakObjectPtr<UBlueprintNodeSpawner> Spawner;
		while (Current->CurrentSpawners.Dequeue(Spawner))
		{
			if (Spawner.IsValid())
			{
				// See if we can document this spawner
				auto K2_NodeInst =
					Current->DocGen->GT_InitializeForSpawner(Spawner.Get(), Current->SourceObject.Get(), OutState);

				if (K2_NodeInst == nullptr)
				{
					continue;
				}

				// Make sure this node object will never be GCd until we're done with it.
				K2_NodeInst->AddToRoot();
				return K2_NodeInst;
			}
		}

		// No spawners left in the queue
		return nullptr;
	};

	auto GameThread_FinalizeDocs = [this](FString const& OutputPath) -> bool {
		bool const Result = Current->DocGen->GT_Finalize(OutputPath);

		if (!Result)
		{
			Current->Task->Notification->SetText(LOCTEXT("DocFinalizationFailed", "Doc gen failed"));
			Current->Task->Notification->SetCompletionState(SNotificationItem::CS_Fail);
			Current->Task->Notification->ExpireAndFadeout();
			// GEditor->PlayEditorSound(CompileSuccessSound);
		}

		return Result;
	};

	/*****************************/

	Current = MakeUnique<FDocGenCurrentTask>();
	Current->Task = InTask;

	FString IntermediateDir =
		FPaths::ProjectIntermediateDir() / TEXT("KantanDocGen") / Current->Task->Settings.DocumentationTitle;

	DocGenThreads::RunOnGameThread(GameThread_EnqueueEnumerators);

	// Initialize the doc generator
	Current->DocGen = MakeUnique<FNodeDocsGenerator>(Current->Task->Settings.OutputFormats);

	if (!DocGenThreads::RunOnGameThreadRetVal(GameThread_InitDocGen, Current->Task->Settings.DocumentationTitle,
											  IntermediateDir))
	{
		UE_LOG(LogKantanDocGen, Error, TEXT("Failed to initialize doc generator!"));
		return;
	}

	bool const bCleanIntermediate = true;
	if (bCleanIntermediate)
	{
		IFileManager::Get().DeleteDirectory(*IntermediateDir, false, true);
	}

	for (auto const& Name : Current->Task->Settings.ExcludedClasses)
	{
		Current->Excluded.Add(Name);
	}

	int SuccessfulNodeCount = 0;
	while (Current->Enumerators.Dequeue(Current->CurrentEnumerator))
	{
		while (DocGenThreads::RunOnGameThreadRetVal(
			GameThread_EnumerateNextObject)) // Game thread: Enumerate next Obj, get spawner list for Obj, store as
											 // array of weak ptrs.
		{
			if (bTerminationRequest)
			{
				return;
			}

			FNodeDocsGenerator::FNodeProcessingState NodeState;
			while (auto NodeInst = DocGenThreads::RunOnGameThreadRetVal(
					   GameThread_EnumerateNextNode,
					   NodeState)) // Game thread: Get next still valid spawner, spawn node, add to root, return it)
			{
				// NodeInst should hopefully not reference anything except stuff we control (ie graph object), and it's
				// rooted so should be safe to deal with here

				// Generate image
				if (!Current->DocGen->GenerateNodeImage(NodeInst, NodeState))
				{
					UE_LOG(LogKantanDocGen, Warning, TEXT("Failed to generate node image!"))
					continue;
				}

				// Generate doc
				if (!Current->DocGen->GenerateNodeDocTree(NodeInst, NodeState))
				{
					UE_LOG(LogKantanDocGen, Warning, TEXT("Failed to generate node doc output!"))
					continue;
				}
				++SuccessfulNodeCount;
			}
		}
	}

	if (SuccessfulNodeCount == 0)
	{
		UE_LOG(LogKantanDocGen, Error, TEXT("No nodes were found to document!"));

		DocGenThreads::RunOnGameThread([this] {
			Current->Task->Notification->SetText(LOCTEXT("DocFinalizationFailed", "Doc gen failed - No nodes found"));
			Current->Task->Notification->SetCompletionState(SNotificationItem::CS_Fail);
			Current->Task->Notification->ExpireAndFadeout();
		});
		// GEditor->PlayEditorSound(CompileSuccessSound);
		return;
	}

	// Game thread: DocGen.GT_Finalize()
	if (!DocGenThreads::RunOnGameThreadRetVal(GameThread_FinalizeDocs, IntermediateDir))
	{
		UE_LOG(LogKantanDocGen, Error, TEXT("Failed to finalize xml docs!"));
		return;
	}

	DocGenThreads::RunOnGameThread(
		[this] { Current->Task->Notification->SetText(LOCTEXT("DocConversionInProgress", "Converting docs")); });
	EIntermediateProcessingResult TransformationResult = Success;
	for (const auto& OutputFormatFactory : Current->Task->Settings.OutputFormats)
	{
		auto FactoryObject = NewObject<UObject>(GetTransientPackage(), OutputFormatFactory.Get());
		const auto& FactoryInterface = Cast<IDocGenOutputFormatFactory>(FactoryObject);
		if (!FactoryInterface)
		{
			continue;
		}
		auto IntermediateProcessor = FactoryInterface->CreateIntermediateDocProcessor();
		EIntermediateProcessingResult Result = IntermediateProcessor->ProcessIntermediateDocs(
			IntermediateDir, Current->Task->Settings.OutputDirectory.Path, Current->Task->Settings.DocumentationTitle,
			Current->Task->Settings.bCleanOutputDirectory);

		if (Result != EIntermediateProcessingResult::Success)
		{
			TransformationResult = Result;
		}
		//Don't abort after performing one transformation, as others may succeed
	}

	if (TransformationResult != EIntermediateProcessingResult::Success)
	{
		UE_LOG(LogKantanDocGen, Error, TEXT("Failed to transform xml to html!"));

		auto Msg = FText::Format(
			LOCTEXT("DocConversionFailed", "Doc gen failed - {0}"),
			TransformationResult == EIntermediateProcessingResult::DiskWriteFailure
				? LOCTEXT(
					  "CouldNotWriteToOutput",
					  "Could not write output, please clear output directory or enable 'Clean Output Directory' option")
				: LOCTEXT("GenericTransformationFailure", "Conversion failure"));
		DocGenThreads::RunOnGameThread([this, Msg] {
			Current->Task->Notification->SetText(Msg);
			Current->Task->Notification->SetCompletionState(SNotificationItem::CS_Fail);
			Current->Task->Notification->ExpireAndFadeout();
		});
		// GEditor->PlayEditorSound(CompileSuccessSound);
		return;
	}

	DocGenThreads::RunOnGameThread([this] {
		FString HyperlinkTarget =
			TEXT("file://") /
			FPaths::ConvertRelativePathToFull(Current->Task->Settings.OutputDirectory.Path /
											  Current->Task->Settings.DocumentationTitle / TEXT("index.html"));
		auto OnHyperlinkClicked = [HyperlinkTarget] {
			UE_LOG(LogKantanDocGen, Log, TEXT("Invoking hyperlink"));
			FPlatformProcess::LaunchURL(*HyperlinkTarget, nullptr, nullptr);
		};

		auto const HyperlinkText = TAttribute<FText>::Create(
			TAttribute<FText>::FGetter::CreateLambda([] { return LOCTEXT("GeneratedDocsHyperlink", "View docs"); }));
		// @TODO: Bug in SNotificationItemImpl::SetHyperlink, ignores non-delegate attributes...
		// LOCTEXT("GeneratedDocsHyperlink", "View docs");

		Current->Task->Notification->SetText(LOCTEXT("DocConversionSuccessful", "Doc gen completed"));
		Current->Task->Notification->SetCompletionState(SNotificationItem::CS_Success);
		Current->Task->Notification->SetHyperlink(FSimpleDelegate::CreateLambda(OnHyperlinkClicked), HyperlinkText);
		Current->Task->Notification->ExpireAndFadeout();
	});

	Current.Reset();
}

#undef LOCTEXT_NAMESPACE
