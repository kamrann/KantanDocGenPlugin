// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#pragma once

#include "TickableEditorObject.h"
#include "DocGenSettings.h"
#include "Containers/Array.h"
#include "Containers/Queue.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "WeakObjectPtrTemplates.h"


class ISourceObjectEnumerator;


class FDocGenTaskProcessor//: public FTickableEditorObject
{
public:
	FDocGenTaskProcessor()
	{}

public:
//	virtual bool IsTickable() const override;
//	virtual void Tick(float DeltaTime) override;

	void Run();

protected:
	struct FDocGenTask
	{
		FKantanDocGenSettings Settings;
		TSharedPtr< class SNotificationItem > Notification;
	};

	struct FDocGenCurrentTask
	{
		TUniquePtr< FDocGenTask > Task;
		TQueue< TSharedPtr< ISourceObjectEnumerator > > Enumerators;
		TSet< FName > Excluded;
		TSet< TWeakObjectPtr< UObject > > Processed;

		TSharedPtr< ISourceObjectEnumerator > CurrentEnumerator;
		TWeakObjectPtr< UObject > SourceObject;
		TQueue< TWeakObjectPtr< UBlueprintNodeSpawner > > CurrentSpawners;
	};

	struct FDocGenOutputTask
	{
		TUniquePtr< FDocGenTask > Task;
	};

	TQueue< TSharedPtr< FDocGenTask > > Queued;
	FDocGenCurrentTask Current;
	TQueue< TSharedPtr< FDocGenOutputTask > > Converting;
};


