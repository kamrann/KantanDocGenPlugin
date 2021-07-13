// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#pragma once

#include "DocGenSettings.h"

#include "Containers/Queue.h"
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "UObject/WeakObjectPtrTemplates.h"

class ISourceObjectEnumerator;
class FNodeDocsGenerator;

class UBlueprintNodeSpawner;

class FDocGenTaskProcessor : public FRunnable
{
public:
	FDocGenTaskProcessor();

public:
	void QueueTask(FKantanDocGenSettings const& Settings);
	bool IsRunning() const;

public:
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;
	virtual void Stop() override;

protected:
	struct FDocGenTask
	{
		FKantanDocGenSettings Settings;

	protected:
		TSharedPtr<class SNotificationItem> Notification;

	public:
		FDocGenTask();
		// Begin safe wrappers around notification functions
		void NotifyExpireDuration(float Duration);
		void NotifyExpireFadeOut();
		void NotifySetText(FText TextToDisplay);
		void NotifySetCompletionState(uint32 State);
		void NotifySetHyperlink(const FSimpleDelegate& InHyperlink,
								const TAttribute<FText>& InHyperlinkText = TAttribute<FText>());
		// end safe wrappers around notification functions
	};

	struct FDocGenCurrentTask
	{
		TSharedPtr<FDocGenTask> Task;

		TQueue<TSharedPtr<ISourceObjectEnumerator>> Enumerators;
		TSet<FName> Excluded;
		TSet<TWeakObjectPtr<UObject>> Processed;

		TSharedPtr<ISourceObjectEnumerator> CurrentEnumerator;
		TWeakObjectPtr<UObject> SourceObject;
		TArray<TWeakObjectPtr<UObject>> TypesToParseForMembers;
		TQueue<TWeakObjectPtr<UBlueprintNodeSpawner>> CurrentSpawners;

		TUniquePtr<FNodeDocsGenerator> DocGen;
	};

	struct FDocGenOutputTask
	{
		TSharedPtr<FDocGenTask> Task;
	};

protected:
	void ProcessTask(TSharedPtr<FDocGenTask> InTask);

protected:
	TQueue<TSharedPtr<FDocGenTask>> Waiting;
	TSharedPtr<FDocGenCurrentTask> Current;
	// TQueue< TSharedPtr< FDocGenOutputTask > > Converting;

	FThreadSafeBool bRunning; // @NOTE: Using this to sync with module calls from game thread is not 100% okay (we're
							  // not atomically testing), but whatevs.
	FThreadSafeBool bTerminationRequest;
};
