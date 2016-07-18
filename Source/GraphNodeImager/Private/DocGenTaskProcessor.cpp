// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#include "GraphNodeImager.h"
#include "DocGenTaskProcessor.h"
#include "Enumeration/ISourceObjectEnumerator.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintNodeSpawner.h"


//bool IsTickable() const override;
//void Tick(float DeltaTime) override;


void FDocGenTaskProcessor::Run()
{
	auto GameThread_EnumerateNextObject = [this]() -> bool
	{
		bool Result = false;

		// What the game thread will execute
		auto Func = [this, &Result]()
		{
			Current.SourceObject.Reset();
			Current.CurrentSpawners.Empty();

			while(auto Obj = Current.CurrentEnumerator->GetNext())
			{
				// Cache list of spawners for this object
				auto& BPActionMap = FBlueprintActionDatabase::Get().GetAllActions();
				if(auto ActionList = BPActionMap.Find(Obj))
				{
					if(ActionList.Num() == 0)
					{
						continue;
					}

					Current.SourceObject = Obj;
					for(auto Spawner : *ActionList)
					{
						// Add to queue as weak ptr
						check(Current.CurrentSpawners.Enqueue(Spawner));
					}

					// Done
					Result = true;
					return;
				}
			}

			// This enumerator is finished
			return;
		};

		FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(Func, TStatId(), nullptr, ENamedThreads::GameThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);
		
		return Result;
	};

	auto GameThread_EnumerateNextNode = [this]() -> UEdGraphNode*
	{
		UEdGraphNode* Result = nullptr;

		// What the game thread will execute
		auto Func = [this, &Result]()
		{
			TWeakObjectPtr< UBlueprintNodeSpawner > Spawner;
			while(CurrentSpawners.Dequeue(Spawner))
			{
				if(Spawner.IsValid())
				{
					//////////////// Wrap this all in a call to if(!DocGen.GT_InitializeForSpawner(Spawner)) continue;

					if(!IsSpawnerDocumentable(Spawner))
					{
						continue;
					}

					// Spawn an instance into the graph
					auto NodeInst = Spawner->Invoke(Graph, TSet< TWeakObjectPtr< UObject > >(), FVector2D(0, 0));

					// Currently Blueprint nodes only
					auto K2NodeInst = Cast< UK2Node >(NodeInst);

					//check(K2NodeInst);
					if(K2NodeInst == nullptr)
					{
						UE_LOG(LogGraphNodeImager, Warning, TEXT("Failed to create node from spawner of class %s with node class %s."), *Spawner->GetClass()->GetName(), Spawner->NodeClass ? *Spawner->NodeClass->GetName() : TEXT("None"));
						continue;
					}

					auto AssociatedClass = MapToAssociatedClass(K2NodeInst, Current.SourceObject);

					if(!ClassDocsMap.Contains(AssociatedClass))
					{
						// New class xml file needs adding
						ClassDocsMap.Add(AssociatedClass, InitClassDocXml(AssociatedClass));
						// Also update the index xml
						UpdateIndexDocWithClass(IndexXml.Get(), AssociatedClass);
					}
					auto ClassDocXml = ClassDocsMap.FindChecked(AssociatedClass);

					FString ClassDocsPath = OutputPath / GetClassDocId(AssociatedClass);

					//////////////

					// Make sure this node object will never be GCd until we're done with it.
					K2NodeInst->AddToRoot();
					Result = K2NodeInst;
					return;
				}
			}

			// No spawners left in the queue
			Result = nullptr;
			return;
		}

		FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(Func, TStatId(), nullptr, ENamedThreads::GameThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);

		return Result;
	};

	while(Current.Enumerators.Dequeue(Current.CurrentEnumerator))
	{
		while(GameThread_EnumerateNextObject())	// Game thread: Enumerate next Obj, get spawner list for Obj, store as array of weak ptrs.
		{
			while(auto NodeInst = GameThread_EnumerateNextNode())	// Game thread: Get next still valid spawner, spawn node, add to root, return it)
			{
				// NodeInst should hopefully not reference anything except stuff we control (ie graph object), and it's rooted so should be safe to deal with here

				// Generate image

				// Generate doc
			}
		}
	}

	// Game thread: DocGen.GT_FinishStuffOff()
}


