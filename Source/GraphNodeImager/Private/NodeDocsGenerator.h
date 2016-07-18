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
	bool Init(FString const& DocsTitle);
	int32 ProcessSourceObject(UObject* Object, FString OutputPath);
	bool Finalize(FString OutputPath);

protected:
	void CleanUp();
	bool GenerateNodeImage(class UEdGraphNode* Node, FString const& ImagePath, FString& OutFilename);
	TSharedPtr< FXmlFile > InitIndexXml(FString const& IndexTitle);
	TSharedPtr< FXmlFile > InitClassDocXml(UClass* Class);
	bool UpdateIndexDocWithClass(FXmlFile* DocFile, UClass* Class);
	bool UpdateClassDocWithNode(FXmlFile* DocFile, UEdGraphNode* Node);
	bool GenerateNodeDocs(class UK2Node* Node, FString const& NodeDocsPath, FString const& RelImagePath);
	bool SaveIndexXml(FString const& OutputDir);
	bool SaveClassDocXml(FString const& OutputDir);

	static void AdjustNodeForSnapshot(UEdGraphNode* Node);
	static FString GetClassDocId(UClass* Class);
	static FString GetNodeDocId(UEdGraphNode* Node);
	static UClass* MapToAssociatedClass(UK2Node* NodeInst, UObject* Source);
	static bool IsSpawnerDocumentable(UBlueprintNodeSpawner* Spawner);

protected:
	class UBlueprint* DummyBP;
	class UEdGraph* Graph;
	TSharedPtr< class SGraphPanel > GraphPanel;
	//FWindowStyle HostStyle;
	//TSharedPtr< class SWindow > HostWindow;

	TSharedPtr< FXmlFile > IndexXml;
	TMap< UClass*, TSharedPtr< FXmlFile > > ClassDocsMap;

public:
	//
	double GenerateNodeImageTime = 0.0;
	double GenerateNodeDocsTime = 0.0;
	//
};


