// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#pragma once

#include "ModuleManager.h"


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
		FString RelImageBasePath;	// = TEXT("img");
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
	bool GT_Init(FString const& DocsTitle, FString const& InOutputDir, UClass* BlueprintContextClass = AActor::StaticClass());
	UK2Node* GT_InitializeForSpawner(UBlueprintNodeSpawner* Spawner, UObject* SourceObject, FNodeProcessingState& OutState);
	bool GT_Finalize(FString OutputPath);
	/**/

	/** Callable from background thread */
	bool GenerateNodeImage(class UEdGraphNode* Node, /* FString const& ImagePath, FString& OutFilename,*/ FNodeProcessingState& State);
	bool GenerateNodeDocs(class UK2Node* Node, /* FString const& NodeDocsPath, FString const& RelImagePath,*/ FNodeProcessingState& State);
	/**/

	//	int32 ProcessSourceObject(UObject* Object, FString OutputPath);

protected:
	void CleanUp();
	TSharedPtr< FXmlFile > InitIndexXml(FString const& IndexTitle);
	TSharedPtr< FXmlFile > InitClassDocXml(UClass* Class);
	bool UpdateIndexDocWithClass(FXmlFile* DocFile, UClass* Class);
	bool UpdateClassDocWithNode(FXmlFile* DocFile, UEdGraphNode* Node);
	bool SaveIndexXml(FString const& OutputDir);
	bool SaveClassDocXml(FString const& OutputDir);

	static void AdjustNodeForSnapshot(UEdGraphNode* Node);
	static FString GetClassDocId(UClass* Class);
	static FString GetNodeDocId(UEdGraphNode* Node);
	static UClass* MapToAssociatedClass(UK2Node* NodeInst, UObject* Source);
	static bool IsSpawnerDocumentable(UBlueprintNodeSpawner* Spawner, bool bIsBlueprint);

protected:
	class UBlueprint* DummyBP;
	class UEdGraph* Graph;
	TSharedPtr< class SGraphPanel > GraphPanel;

	TSharedPtr< FXmlFile > IndexXml;
	TMap< TWeakObjectPtr< UClass >, TSharedPtr< FXmlFile > > ClassDocsMap;

	FString OutputDir;

public:
	//
	double GenerateNodeImageTime = 0.0;
	double GenerateNodeDocsTime = 0.0;
	//
};


