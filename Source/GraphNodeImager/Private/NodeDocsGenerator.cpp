// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#pragma once

#include "GraphNodeImager.h"
#include "NodeDocsGenerator.h"
#include "SGraphNode.h"
#include "Editor/GraphEditor/Private/NodeFactory.h"
#include "Editor/GraphEditor/Private/SGraphPanel.h"
#include "EdGraphSchema_K2.h"
#include "BlueprintEditorUtils.h"
#include "KismetEditorUtilities.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "HighResScreenshot.h"
#include "XmlFile.h"


FNodeDocsGenerator::~FNodeDocsGenerator()
{
	CleanUp();
}

bool FNodeDocsGenerator::Init(FString const& DocsTitle)
{
	DummyBP = CastChecked< UBlueprint >(FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),	// @TODO: hacky, this is the class the bp derives from
		::GetTransientPackage(),
		NAME_None,
		EBlueprintType::BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		NAME_None
	));
	if(!DummyBP)
	{
		return false;
	}

	//			auto Graph = NewObject< UEdGraph >();
	//			Graph->Schema = UEdGraphSchema_K2::StaticClass();
	Graph = FBlueprintEditorUtils::CreateNewGraph(DummyBP, TEXT("TempoGraph"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	//			Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);

	GraphPanel = SNew(SGraphPanel)
		.GraphObj(Graph)
		;
	// We want full detail for rendering, passing a super-high zoom value will guarantee the highest LOD.
	GraphPanel->RestoreViewSettings(FVector2D(0, 0), 10.0f);

	HostStyle = FCoreStyle::Get().GetWidgetStyle<FWindowStyle>("Window");
	HostStyle.SetBackgroundBrush(FSlateColorBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f)));
	//HostStyle.SetChildBackgroundBrush(FSlateColorBrush(FLinearColor(1.0f, 1.0f, 0.0f, 1.0f)));
	HostStyle.SetOutlineBrush(FSlateNoResource());
	HostStyle.SetBorderBrush(FSlateNoResource());

	HostWindow = SNew(SWindow)
		.CreateTitleBar(false)
		// @TODO: size hack
		.ClientSize(FVector2D(1000.f, 1000.f))
		.SupportsMaximize(false).SupportsMinimize(false)
		.HasCloseButton(false)
		//.SizingRule(ESizingRule::Autosized)
		.SizingRule(ESizingRule::FixedSize)
		.UseOSWindowBorder(false)
		.LayoutBorder(FMargin())
		.SupportsTransparency(EWindowTransparency::PerPixel)
		.Style(&HostStyle)
		// Push the window offscreen, though unfortunately we still show in the taskbar (see comment below)
		.AutoCenter(EAutoCenter::None)
		.SaneWindowPlacement(false)
		.ScreenPosition(FVector2D(-5000, -5000))
//		.IsInitiallyMinimized(true)
//		.Visibility(EVisibility::Hidden)
		;

	//SetIndependentViewportSize

	// @TODO: Ideally we would not have to show the window in order to render our node images, but so far not sure if/how this can
	// be achieved. If not shown, the widget desired size doesn't get computed, which prevents us from taking the screenshot.
	FSlateApplication::Get().AddWindow(HostWindow.ToSharedRef());// , false);

	IndexXml = InitIndexXml(DocsTitle);
	ClassDocsMap.Empty();

	return true;
}

int32 FNodeDocsGenerator::ProcessSourceObject(UObject* Object, FString OutputPath)
{
	auto& BPActionMap = FBlueprintActionDatabase::Get().GetAllActions();

	int32 Count = 0;
	if(auto ActionList = BPActionMap.Find(Object))
	{
		for(auto Spawner : *ActionList)
		{
			if(!IsSpawnerDocumentable(Spawner))
			{
				continue;
			}

			// Spawn an instance into the graph
			auto NodeInst = Spawner->Invoke(Graph, TSet< TWeakObjectPtr< UObject > >(), FVector2D(0, 0));

			// Currently Blueprint nodes only
			auto K2NodeInst = Cast< UK2Node >(NodeInst);
			check(K2NodeInst);

			auto AssociatedClass = MapToAssociatedClass(K2NodeInst, Object);

			if(!ClassDocsMap.Contains(AssociatedClass))
			{
				// New class xml file needs adding
				ClassDocsMap.Add(AssociatedClass, InitClassDocXml(AssociatedClass));
				// Also update the index xml
				UpdateIndexDocWithClass(IndexXml.Get(), AssociatedClass);
			}
			auto ClassDocXml = ClassDocsMap.FindChecked(AssociatedClass);

			FString ClassDocsPath = OutputPath / GetClassDocId(AssociatedClass);

			AdjustNodeForSnapshot(NodeInst);

			FString RelImageBasePath = TEXT("img");
			FString ImageBasePath = ClassDocsPath / RelImageBasePath;
			FString ImageFilename;
			bool bImageSuccess = GenerateNodeImage(K2NodeInst, ImageBasePath, ImageFilename);
			if(!bImageSuccess)
			{
				// @TODO:
				continue;
			}

			FString NodePath = ClassDocsPath / TEXT("nodes");
			if(!GenerateNodeDocs(K2NodeInst, NodePath, RelImageBasePath / ImageFilename))
			{
				// @TODO:
				continue;
			}

			if(!UpdateClassDocWithNode(ClassDocXml.Get(), NodeInst))
			{
				// @TODO:
				continue;
			}

			// @TODO: THink i need more cleanup here, like removing graph node from graph?

			// Success!
			++Count;
		}

		// @TODO: single page for class docs, at ClassDocsPath/<class_name>.html
	}

	return Count;
}

bool FNodeDocsGenerator::Finalize(FString OutputPath)
{
	if(!SaveClassDocXml(OutputPath))
	{
		return false;
	}

	if(!SaveIndexXml(OutputPath))
	{
		return false;
	}

	return true;
}

void FNodeDocsGenerator::CleanUp()
{
	if(HostWindow.IsValid())
	{
		HostWindow->SetContent(SNullWidget::NullWidget);
		HostWindow->RequestDestroyWindow();
		HostWindow.Reset();
	}

	if(GraphPanel.IsValid())
	{
		GraphPanel.Reset();
	}

	Graph = nullptr;
	DummyBP = nullptr;
}

bool FNodeDocsGenerator::GenerateNodeImage(UEdGraphNode* Node, FString const& ImagePath, FString& OutFilename)
{
	bool bSuccess = false;

	FString NodeName = GetNodeDocId(Node);

	//GraphPanel->AddGraphNode(NodeWidget.ToSharedRef());
	auto NodeWidget = FNodeFactory::CreateNodeWidget(Node);
	NodeWidget->SetOwner(GraphPanel.ToSharedRef());

	HostWindow->SetContent(
		/*
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
		*/
		NodeWidget.ToSharedRef()
		/*
		]
		*/
	);

	// Need to force a draw pass so that desired sizes are calculated
	FSlateApplication::Get().ForceRedrawWindow(HostWindow.ToSharedRef());

	auto Desired = NodeWidget->GetDesiredSize();
	FIntRect Rect = FIntRect(0, 0, (int32)Desired.X, (int32)Desired.Y);
	TArray< FColor > Bitmap;
	FIntVector ImgSize;
	if(FSlateApplication::Get().TakeScreenshot(
		NodeWidget.ToSharedRef(),
		Rect,
		Bitmap,
		ImgSize
	))
	{
		FHighResScreenshotConfig& HighResScreenshotConfig = GetHighResScreenshotConfig();
		HighResScreenshotConfig.SetHDRCapture(false);

		FString ImgFilename = FString::Printf(TEXT("nd_img_%s.png"), *NodeName);
		FString ScreenshotSaveName = ImagePath / ImgFilename;
		if(HighResScreenshotConfig.SaveImage(ScreenshotSaveName, Bitmap, FIntPoint(ImgSize.X, ImgSize.Y)))
		{
			// Success!
			bSuccess = true;
			//InOutImagePath = ScreenshotSaveName;
			OutFilename = ImgFilename;
		}
		else
		{
			UE_LOG(LogGraphNodeImager, Warning, TEXT("Failed to save screenshot image for node: %s"), *NodeName);
		}
	}
	else
	{
		UE_LOG(LogGraphNodeImager, Warning, TEXT("Failed to take screenshot of node: %s"), *NodeName);
	}

	return bSuccess;
}

inline FString WrapAsCDATA(FString const& InString)
{
	return TEXT("<![CDATA[") + InString + TEXT("]]>");
}

inline FXmlNode* AppendChild(FXmlNode* Parent, FString const& Name)
{
	Parent->AppendChildNode(Name, FString());
	return Parent->GetChildrenNodes().Last();
}

inline FXmlNode* AppendChildCDATA(FXmlNode* Parent, FString const& Name, FString const& TextContent)
{
	Parent->AppendChildNode(Name, WrapAsCDATA(TextContent));
	return Parent->GetChildrenNodes().Last();
}

// For K2 pins only!
bool ExtractPinInformation(UEdGraphPin* Pin, FString& OutName, FString& OutType, FString& OutDescription)
{
	FString Tooltip;
	Pin->GetOwningNode()->GetPinHoverText(*Pin, Tooltip);

	if(!Tooltip.IsEmpty())
	{
		// @NOTE: This is based on the formatting in UEdGraphSchema_K2::ConstructBasicPinTooltip.
		// If that is changed, this will fail!
		
		auto TooltipPtr = *Tooltip;

		// Parse name line
		FParse::Line(&TooltipPtr, OutName);
		// Parse type line
		FParse::Line(&TooltipPtr, OutType);

		// Currently there is an empty line here, but FParse::Line seems to gobble up empty lines as part of the previous call.
		// Anyway, attempting here to deal with this generically in case that weird behaviour changes.
		while(*TooltipPtr == TEXT('\n'))
		{
			FString Buf;
			FParse::Line(&TooltipPtr, Buf);
		}

		// What remains is the description
		OutDescription = TooltipPtr;
	}

	// @NOTE: Currently overwriting the name and type as suspect this is more robust to future engine changes.

	OutName = Pin->GetDisplayName().ToString();
	if(OutName.IsEmpty() && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		OutName = Pin->Direction == EEdGraphPinDirection::EGPD_Input ? TEXT("In") : TEXT("Out");
	}

	OutType = UEdGraphSchema_K2::TypeToText(Pin->PinType).ToString();

	return true;
}

TSharedPtr< FXmlFile > FNodeDocsGenerator::InitIndexXml(FString const& IndexTitle)
{
	const FString FileTemplate = R"xxx(<?xml version="1.0" encoding="UTF-8"?>
<root></root>)xxx";

	TSharedPtr< FXmlFile > File = MakeShareable(new FXmlFile(FileTemplate, EConstructMethod::ConstructFromBuffer));
	auto Root = File->GetRootNode();

	AppendChildCDATA(Root, TEXT("display_name"), IndexTitle);
	AppendChild(Root, TEXT("classes"));

	return File;
}

TSharedPtr< FXmlFile > FNodeDocsGenerator::InitClassDocXml(UClass* Class)
{
	const FString FileTemplate = R"xxx(<?xml version="1.0" encoding="UTF-8"?>
<root></root>)xxx";

	TSharedPtr< FXmlFile > File = MakeShareable(new FXmlFile(FileTemplate, EConstructMethod::ConstructFromBuffer));
	auto Root = File->GetRootNode();

	AppendChildCDATA(Root, TEXT("id"), GetClassDocId(Class));
	AppendChildCDATA(Root, TEXT("display_name"), Class->GetName());	// @TODO: forgotten what the engine function is for class display name
	AppendChild(Root, TEXT("nodes"));

	return File;
}

bool FNodeDocsGenerator::UpdateIndexDocWithClass(FXmlFile* DocFile, UClass* Class)
{
	auto ClassId = GetClassDocId(Class);
	auto Classes = DocFile->GetRootNode()->FindChildNode(TEXT("classes"));
	auto ClassElem = AppendChild(Classes, TEXT("class"));
	AppendChildCDATA(ClassElem, TEXT("id"), ClassId);
	AppendChildCDATA(ClassElem, TEXT("display_name"), Class->GetName());	// @TODO: display name as above comment
	return true;
}

bool FNodeDocsGenerator::UpdateClassDocWithNode(FXmlFile* DocFile, UEdGraphNode* Node)
{
	auto NodeId = GetNodeDocId(Node);
	auto Nodes = DocFile->GetRootNode()->FindChildNode(TEXT("nodes"));
	auto NodeElem = AppendChild(Nodes, TEXT("node"));
	AppendChildCDATA(NodeElem, TEXT("id"), NodeId);
	AppendChildCDATA(NodeElem, TEXT("shorttitle"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
	return true;
}

inline bool ShouldDocumentPin(UEdGraphPin* Pin)
{
	return !Pin->bHidden;
}

bool FNodeDocsGenerator::GenerateNodeDocs(UK2Node* Node, FString const& NodeDocsPath, FString const& RelImagePath)
{
	FString DocFilePath = NodeDocsPath / (GetNodeDocId(Node) + TEXT(".xml"));

	const FString FileTemplate = R"xxx(<?xml version="1.0" encoding="UTF-8"?>
<root></root>)xxx";

	FXmlFile File(FileTemplate, EConstructMethod::ConstructFromBuffer);
	auto Root = File.GetRootNode();
	
	AppendChildCDATA(Root, TEXT("shorttitle"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());

	FString NodeFullTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	auto TargetIdx = NodeFullTitle.Find(TEXT("Target is "), ESearchCase::CaseSensitive);
	if(TargetIdx != INDEX_NONE)
	{
		NodeFullTitle = NodeFullTitle.Left(TargetIdx);
	}
	AppendChildCDATA(Root, TEXT("fulltitle"), NodeFullTitle);

	FString NodeDesc = Node->GetTooltipText().ToString();
	TargetIdx = NodeDesc.Find(TEXT("Target is "), ESearchCase::CaseSensitive);
	if(TargetIdx != INDEX_NONE)
	{
		NodeDesc = NodeDesc.Left(TargetIdx);
	}
	AppendChildCDATA(Root, TEXT("description"), NodeDesc);
	AppendChildCDATA(Root, TEXT("imgpath"), RelImagePath);
	AppendChildCDATA(Root, TEXT("category"), Node->GetMenuCategory().ToString());
	
	auto Inputs = AppendChild(Root, TEXT("inputs"));
	for(auto Pin : Node->Pins)
	{
		if(Pin->Direction == EEdGraphPinDirection::EGPD_Input)
		{
			if(ShouldDocumentPin(Pin))
			{
				auto Input = AppendChild(Inputs, TEXT("param"));

				FString PinName, PinType, PinDesc;
				ExtractPinInformation(Pin, PinName, PinType, PinDesc);

				AppendChildCDATA(Input, TEXT("name"), PinName);
				AppendChildCDATA(Input, TEXT("type"), PinType);
				AppendChildCDATA(Input, TEXT("description"), PinDesc);
			}
		}
	}

	auto Outputs = AppendChild(Root, TEXT("outputs"));
	for(auto Pin : Node->Pins)
	{
		if(Pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			if(ShouldDocumentPin(Pin))
			{
				auto Output = AppendChild(Outputs, TEXT("param"));

				FString PinName, PinType, PinDesc;
				ExtractPinInformation(Pin, PinName, PinType, PinDesc);

				AppendChildCDATA(Output, TEXT("name"), PinName);
				AppendChildCDATA(Output, TEXT("type"), PinType);
				AppendChildCDATA(Output, TEXT("description"), PinDesc);
			}
		}
	}

	if(!File.Save(DocFilePath))
	{
		return false;
	}

	//InOutNodeDocsPath = DocFilePath;
	return true;
}

bool FNodeDocsGenerator::SaveIndexXml(FString const& OutputDir)
{
	auto Path = OutputDir / TEXT("index.xml");
	IndexXml->Save(Path);

	return true;
}

bool FNodeDocsGenerator::SaveClassDocXml(FString const& OutputDir)
{
	for(auto const& Entry : ClassDocsMap)
	{
		auto ClassId = GetClassDocId(Entry.Key);
		auto Path = OutputDir / ClassId / (ClassId + TEXT(".xml"));
		Entry.Value->Save(Path);
	}

	return true;
}


void FNodeDocsGenerator::AdjustNodeForSnapshot(UEdGraphNode* Node)
{
	// Hide default value box containing 'self' for Target pin
	if(auto K2_Schema = Cast< UEdGraphSchema_K2 >(Node->GetSchema()))
	{
		if(auto TargetPin = Node->FindPin(K2_Schema->PN_Self))
		{
			TargetPin->bDefaultValueIsIgnored = true;
		}
	}
}

FString FNodeDocsGenerator::GetClassDocId(UClass* Class)
{
	return Class->GetName();
}

FString FNodeDocsGenerator::GetNodeDocId(UEdGraphNode* Node)
{
	// @TODO: Not sure this is right thing to use
	return Node->GetDocumentationExcerptName();
}


#include "BlueprintVariableNodeSpawner.h"
#include "BlueprintDelegateNodeSpawner.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"

/*
This takes a graph node object and attempts to map it to the class which the node conceptually belong to.
If there is no special mapping for the node, the function determines the class from the source object.
*/
UClass* FNodeDocsGenerator::MapToAssociatedClass(UK2Node* NodeInst, UObject* Source)
{
	// For nodes derived from UK2Node_CallFunction, associate with the class owning the called function.
	if(auto FuncNode = Cast< UK2Node_CallFunction >(NodeInst))
	{
		auto Func = FuncNode->GetTargetFunction();
		if(Func)
		{
			return Func->GetOwnerClass();
		}
	}

	// Default fallback
	if(auto SourceClass = Cast< UClass >(Source))
	{
		return SourceClass;
	}
	else if(auto SourceBP = Cast< UBlueprint >(Source))
	{
		return SourceBP->GeneratedClass;
	}
	else
	{
		return nullptr;
	}
}

bool FNodeDocsGenerator::IsSpawnerDocumentable(UBlueprintNodeSpawner* Spawner)
{
	// Spawners of or deriving from the following classes will be excluded
	static const TSubclassOf< UBlueprintNodeSpawner > ExcludedSpawnerClasses[] = {
		UBlueprintVariableNodeSpawner::StaticClass(),
		UBlueprintDelegateNodeSpawner::StaticClass(),
	};

	// Spawners for nodes of these types (or their subclasses) will be excluded
	static const TSubclassOf< UK2Node > ExcludedNodeClasses[] = {
		UK2Node_DynamicCast::StaticClass(),
	};

	// Function spawners for functions with any of the following metadata tags will also be excluded
	static const FName ExcludedFunctionMeta[] = {
		TEXT("BlueprintAutocast")
	};


	for(auto ExclSpawnerClass : ExcludedSpawnerClasses)
	{
		if(Spawner->IsA(ExclSpawnerClass))
		{
			return false;
		}
	}

	for(auto ExclNodeClass : ExcludedNodeClasses)
	{
		if(Spawner->NodeClass->IsChildOf(ExclNodeClass))
		{
			return false;
		}
	}

	if(auto FuncSpawner = Cast< UBlueprintFunctionNodeSpawner >(Spawner))
	{
		for(auto const& Meta : ExcludedFunctionMeta)
		{
			if(FuncSpawner->GetFunction()->HasMetaData(Meta))
			{
				return false;
			}
		}
	}

	return true;
}

