// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"


class UClass;
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UK2Node;
class UBlueprintNodeSpawner;
class FXmlFile;

class FNodeDocsGenerator
{
public:
	FNodeDocsGenerator(const TArray<class UDocGenOutputFormatFactoryBase*>& OutputFormats)
	:OutputFormats(OutputFormats)
	{}
	~FNodeDocsGenerator();

public:
	struct FNodeProcessingState
	{
		TSharedPtr<class DocTreeNode> ClassDocTree;
		FString ClassDocsPath;
		FString RelImageBasePath;
		FString ImageFilename;

		FNodeProcessingState():
			ClassDocTree()
			, ClassDocsPath()
			, RelImageBasePath()
			, ImageFilename()
		{}
	};

public:
	/** Callable only from game thread */
	bool GT_Init(FString const& InDocsTitle, FString const& InOutputDir, UClass* BlueprintContextClass = AActor::StaticClass());
	UK2Node* GT_InitializeForSpawner(UBlueprintNodeSpawner* Spawner, UObject* SourceObject, FNodeProcessingState& OutState);
	bool GT_Finalize(FString OutputPath);
	/**/

	/** Callable from background thread */
	bool GenerateNodeImage(UEdGraphNode* Node, FNodeProcessingState& State);
	bool GenerateNodeDocTree(UK2Node* Node, FNodeProcessingState& State);
	bool GenerateTypeMembers(UObject* Type);
	/**/

protected:
	void CleanUp();
	bool SaveIndexFile(FString const& OutDir);
	bool SaveClassDocFile(FString const& OutDir);

	TSharedPtr<DocTreeNode> InitIndexDocTree(FString const& IndexTitle);
	TSharedPtr<DocTreeNode> InitClassDocTree(UClass* Class);
	bool UpdateIndexDocWithClass(TSharedPtr<DocTreeNode> DocTree, UClass* Class);
	bool UpdateIndexDocWithStruct(TSharedPtr<DocTreeNode> DocTree, UStruct* Struct);
	bool UpdateIndexDocWithEnum(TSharedPtr<DocTreeNode> DocTree, UEnum* Enum);
	bool UpdateClassDocWithNode(TSharedPtr<DocTreeNode> DocTree, UEdGraphNode* Node);
	
	static void AdjustNodeForSnapshot(UEdGraphNode* Node);
	static FString GetClassDocId(UClass* Class);
	static FString GetNodeDocId(UEdGraphNode* Node);
	static UClass* MapToAssociatedClass(UK2Node* NodeInst, UObject* Source);
	static bool IsSpawnerDocumentable(UBlueprintNodeSpawner* Spawner, bool bIsBlueprint);

protected:
	TWeakObjectPtr< UBlueprint > DummyBP;
	TWeakObjectPtr< UEdGraph > Graph;
	TSharedPtr< class SGraphPanel > GraphPanel;

	FString DocsTitle;
	TSharedPtr<DocTreeNode> IndexTree;
	TMap<TWeakObjectPtr<UClass>, TSharedPtr<DocTreeNode>> ClassDocTreeMap;
	TArray<UDocGenOutputFormatFactoryBase*> OutputFormats;
	FString OutputDir;
	bool SaveAllFormats(FString const& OutDir, TSharedPtr<DocTreeNode> Document){};
public:
	//
	double GenerateNodeImageTime = 0.0;
	double GenerateNodeDocsTime = 0.0;
	//
};


