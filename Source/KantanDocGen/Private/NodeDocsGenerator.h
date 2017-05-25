// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
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
	FNodeDocsGenerator()
	{}
	~FNodeDocsGenerator();

public:
	struct FNodeProcessingState
	{
		TSharedPtr< FXmlFile > ClassDocXml;
		FString ClassDocsPath;
		FString RelImageBasePath;
		FString ImageFilename;

		FNodeProcessingState():
			ClassDocXml()
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
	bool GenerateNodeDocs(UK2Node* Node, FNodeProcessingState& State);
	/**/

protected:
	void CleanUp();
	TSharedPtr< FXmlFile > InitIndexXml(FString const& IndexTitle);
	TSharedPtr< FXmlFile > InitClassDocXml(UClass* Class);
	bool UpdateIndexDocWithClass(FXmlFile* DocFile, UClass* Class);
	bool UpdateClassDocWithNode(FXmlFile* DocFile, UEdGraphNode* Node);
	bool SaveIndexXml(FString const& OutDir);
	bool SaveClassDocXml(FString const& OutDir);

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
	TSharedPtr< FXmlFile > IndexXml;
	TMap< TWeakObjectPtr< UClass >, TSharedPtr< FXmlFile > > ClassDocsMap;

	FString OutputDir;

public:
	//
	double GenerateNodeImageTime = 0.0;
	double GenerateNodeDocsTime = 0.0;
	//
};


