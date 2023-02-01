// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Copyright (C) 2016-2017 Cameron Angus. All Rights Reserved.

#include "NodeDocsGenerator.h"
#include "Async/Async.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintBoundNodeSpawner.h"
#include "BlueprintComponentNodeSpawner.h"
#include "BlueprintEventNodeSpawner.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintNodeSpawner.h"
#include "DocTreeNode.h"
#include "DoxygenParserHelpers.h"
#include "EdGraphSchema_K2.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HighResScreenshot.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Message.h"
#include "KantanDocGenLog.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/EngineVersionComparison.h"
#include "NodeFactory.h"
#include "OutputFormats/DocGenOutputFormatFactoryBase.h"
#include "Runtime/ImageWriteQueue/Public/ImageWriteTask.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "Slate/WidgetRenderer.h"
#include "Stats/StatsMisc.h"
#include "TextureResource.h"
#include "ThreadingHelpers.h"

FNodeDocsGenerator::~FNodeDocsGenerator()
{
	CleanUp();
}

bool FNodeDocsGenerator::GT_Init(FString const& InDocsTitle, FString const& InOutputDir, UClass* BlueprintContextClass)
{
	DummyBP = CastChecked<UBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		BlueprintContextClass, ::GetTransientPackage(), NAME_None, EBlueprintType::BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None));
	if (!DummyBP.IsValid())
	{
		return false;
	}

	Graph = FBlueprintEditorUtils::CreateNewGraph(DummyBP.Get(), TEXT("TempoGraph"), UEdGraph::StaticClass(),
												  UEdGraphSchema_K2::StaticClass());

	DummyBP->AddToRoot();
	Graph->AddToRoot();

	GraphPanel = SNew(SGraphPanel).GraphObj(Graph.Get());
	// We want full detail for rendering, passing a super-high zoom value will guarantee the highest LOD.
	GraphPanel->RestoreViewSettings(FVector2D(0, 0), 10.0f);

	DocsTitle = InDocsTitle;

	IndexTree = InitIndexDocTree(DocsTitle);

	ClassDocTreeMap.Empty();
	OutputDir = InOutputDir;

	return true;
}

UK2Node* FNodeDocsGenerator::GT_InitializeForSpawner(UBlueprintNodeSpawner* Spawner, UObject* SourceObject,
													 FNodeProcessingState& OutState)
{
	if (!IsSpawnerDocumentable(Spawner, SourceObject->IsA<UBlueprint>()))
	{
		return nullptr;
	}

	// Spawn an instance into the graph
	auto NodeInst = Spawner->Invoke(Graph.Get(), IBlueprintNodeBinder::FBindingSet {}, FVector2D(0, 0));

	// Currently Blueprint nodes only
	auto K2NodeInst = Cast<UK2Node>(NodeInst);

	if (K2NodeInst == nullptr)
	{
		UE_LOG(LogKantanDocGen, Warning, TEXT("Failed to create node from spawner of class %s with node class %s."),
			   *Spawner->GetClass()->GetName(), Spawner->NodeClass ? *Spawner->NodeClass->GetName() : TEXT("None"));
		return nullptr;
	}

	auto AssociatedClass = MapToAssociatedClass(K2NodeInst, SourceObject);

	if (!ClassDocTreeMap.Contains(AssociatedClass))
	{
		ClassDocTreeMap.Add(AssociatedClass, InitClassDocTree(AssociatedClass));
		UpdateIndexDocWithClass(IndexTree, AssociatedClass);
	}

	OutState = FNodeProcessingState();
	OutState.ClassDocsPath = OutputDir / GetClassDocId(AssociatedClass);
	OutState.ClassDocTree = ClassDocTreeMap.FindChecked(AssociatedClass);

	return K2NodeInst;
}

bool FNodeDocsGenerator::GT_Finalize(FString OutputPath)
{
	if (!SaveClassDocFile(OutputPath))
	{
		return false;
	}
	if (!SaveEnumDocFile(OutputPath))
	{
		return false;
	}
	if (!SaveStructDocFile(OutputPath))
	{
		return false;
	}
	if (!SaveIndexFile(OutputPath))
	{
		return false;
	}

	return true;
}

void FNodeDocsGenerator::CleanUp()
{
	if (GraphPanel.IsValid())
	{
		GraphPanel.Reset();
	}

	if (DummyBP.IsValid())
	{
		DummyBP->RemoveFromRoot();
		DummyBP.Reset();
	}
	if (Graph.IsValid())
	{
		Graph->RemoveFromRoot();
		Graph.Reset();
	}
}

bool FNodeDocsGenerator::GenerateNodeImage(UEdGraphNode* Node, FNodeProcessingState& State)
{
	SCOPE_SECONDS_COUNTER(GenerateNodeImageTime);

	const FVector2D DrawSize(1024.0f, 1024.0f);

	bool bSuccess = false;

	AdjustNodeForSnapshot(Node);

	FString NodeName = GetNodeDocId(Node);

	FIntRect Rect;

	TUniquePtr<TImagePixelData<FColor>> PixelData;

	auto RenderNodeResult = Async(EAsyncExecution::TaskGraphMainThread, [this, Node, DrawSize, &Rect, &PixelData] {
		auto NodeWidget = FNodeFactory::CreateNodeWidget(Node);
		NodeWidget->SetOwner(GraphPanel.ToSharedRef());

		const bool bUseGammaCorrection = false;
		FWidgetRenderer Renderer(false);
		Renderer.SetIsPrepassNeeded(true);
		auto RenderTarget = Renderer.DrawWidget(NodeWidget.ToSharedRef(), DrawSize);
		auto Desired = NodeWidget->GetDesiredSize();
#if UE_VERSION_NEWER_THAN(5, 0, 0)
		FlushRenderingCommands();
#else 
		FlushRenderingCommands(true);
#endif
		FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
		Rect = FIntRect(0, 0, (int32) Desired.X, (int32) Desired.Y);
		FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
		ReadPixelFlags.SetLinearToGamma(true); // @TODO: is this gamma correction, or something else?

		PixelData = MakeUnique<TImagePixelData<FColor>>(FIntPoint((int32) Desired.X, (int32) Desired.Y));
		PixelData->Pixels.SetNumUninitialized(Desired.X * Desired.Y);

		if (RTResource->ReadPixelsPtr(PixelData->Pixels.GetData(), ReadPixelFlags, Rect) == false)
		{
			UE_LOG(LogKantanDocGen, Warning, TEXT("Failed to read pixels for node image."));
			return false;
		}
		BeginReleaseResource(RTResource);
		return true;
	});

	if (!RenderNodeResult.Get())
	{
		return false;
	}

	State.RelImageBasePath = TEXT("../img");
	FString ImageBasePath = State.ClassDocsPath / TEXT("img"); // State.RelImageBasePath;
	if (!IFileManager::Get().DirectoryExists(*ImageBasePath))
	{
		IFileManager::Get().MakeDirectory(*ImageBasePath, true);
	}
	FString ImgFilename = FString::Printf(TEXT("nd_img_%s.png"), *NodeName);
	FString ScreenshotSaveName = ImageBasePath / ImgFilename;

	TUniquePtr<FImageWriteTask> ImageTask = MakeUnique<FImageWriteTask>();
	ImageTask->PixelData = MoveTemp(PixelData);
	ImageTask->Filename = ScreenshotSaveName;
	ImageTask->Format = EImageFormat::PNG;
	ImageTask->CompressionQuality = (int32) EImageCompressionQuality::Default;
	ImageTask->bOverwriteFile = true;
	ImageTask->PixelPreProcessors.Add([](FImagePixelData* PixelData) {
		check(PixelData->GetType() == EImagePixelType::Color);

		TImagePixelData<FColor>* ColorData = static_cast<TImagePixelData<FColor>*>(PixelData);
		for (FColor& Pixel : static_cast<TImagePixelData<FColor>*>(PixelData)->Pixels)
		{
			if (Pixel.A >= 90)
			{
				Pixel.A = 255;
			}
		}
	});

	if (ImageTask->RunTask())
	{
		// Success!
		bSuccess = true;
		State.ImageFilename = ImgFilename;
	}
	else
	{
		UE_LOG(LogKantanDocGen, Warning, TEXT("Failed to save screenshot image for node: %s"), *NodeName);
	}

	return bSuccess;
}

// For K2 pins only!
bool ExtractPinInformation(UEdGraphPin* Pin, FString& OutName, FString& OutType, FString& OutDescription)
{
	FString Tooltip;
	Pin->GetOwningNode()->GetPinHoverText(*Pin, Tooltip);

	if (!Tooltip.IsEmpty())
	{
		// @NOTE: This is based on the formatting in UEdGraphSchema_K2::ConstructBasicPinTooltip.
		// If that is changed, this will fail!

		auto TooltipPtr = *Tooltip;

		// Parse name line
		FParse::Line(&TooltipPtr, OutName);
		// Parse type line
		FParse::Line(&TooltipPtr, OutType);

		// Currently there is an empty line here, but FParse::Line seems to gobble up empty lines as part of the
		// previous call. Anyway, attempting here to deal with this generically in case that weird behaviour changes.
		while (*TooltipPtr == TEXT('\n'))
		{
			FString Buf;
			FParse::Line(&TooltipPtr, Buf);
		}

		// What remains is the description
		OutDescription = TooltipPtr;
	}

	// @NOTE: Currently overwriting the name and type as suspect this is more robust to future engine changes.

	OutName = Pin->GetDisplayName().ToString();
	if (OutName.IsEmpty() && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		OutName = Pin->Direction == EEdGraphPinDirection::EGPD_Input ? TEXT("In") : TEXT("Out");
	}

	OutType = UEdGraphSchema_K2::TypeToText(Pin->PinType).ToString();

	return true;
}

TSharedPtr<DocTreeNode> FNodeDocsGenerator::InitIndexDocTree(FString const& IndexTitle)
{
	TSharedPtr<DocTreeNode> IndexDocTree = MakeShared<DocTreeNode>();
	IndexDocTree->AppendChildWithValueEscaped(TEXT("display_name"), IndexTitle);
	IndexDocTree->AppendChild(TEXT("classes"));
	IndexDocTree->AppendChild(TEXT("structs"));
	IndexDocTree->AppendChild(TEXT("enums"));
	return IndexDocTree;
}

TSharedPtr<DocTreeNode> FNodeDocsGenerator::InitClassDocTree(UClass* Class)
{
	TSharedPtr<DocTreeNode> ClassDoc = MakeShared<DocTreeNode>();
	ClassDoc->AppendChildWithValueEscaped(TEXT("docs_name"), DocsTitle);
	ClassDoc->AppendChildWithValueEscaped(TEXT("id"), GetClassDocId(Class));
	ClassDoc->AppendChildWithValueEscaped(TEXT("display_name"),
										  FBlueprintEditorUtils::GetFriendlyClassDisplayName(Class).ToString());
	ClassDoc->AppendChild(TEXT("nodes"));
	ClassDoc->AppendChild(TEXT("fields"));
	return ClassDoc;
}

TSharedPtr<DocTreeNode> FNodeDocsGenerator::InitStructDocTree(UScriptStruct* Struct)
{
	TSharedPtr<DocTreeNode> StructDoc = MakeShared<DocTreeNode>();
	StructDoc->AppendChildWithValueEscaped(TEXT("docs_name"), DocsTitle);
	StructDoc->AppendChildWithValueEscaped(TEXT("id"), Struct->GetName());
	if (Struct->HasMetaData(TEXT("DisplayName")))
	{
		StructDoc->AppendChildWithValueEscaped(TEXT("display_name"), Struct->GetMetaData(TEXT("DisplayName")));
	}
	else
	{
		StructDoc->AppendChildWithValueEscaped(TEXT("display_name"),
											   FName::NameToDisplayString(Struct->GetName(), false));
	}
	StructDoc->AppendChild(TEXT("fields"));
	return StructDoc;
}

TSharedPtr<DocTreeNode> FNodeDocsGenerator::InitEnumDocTree(UEnum* Enum)
{
	TSharedPtr<DocTreeNode> EnumDoc = MakeShared<DocTreeNode>();
	EnumDoc->AppendChildWithValueEscaped(TEXT("docs_name"), DocsTitle);
	EnumDoc->AppendChildWithValueEscaped(TEXT("id"), Enum->GetName());
	if (Enum->HasMetaData(TEXT("DisplayName")))
	{
		EnumDoc->AppendChildWithValueEscaped(TEXT("display_name"), Enum->GetMetaData(TEXT("DisplayName")));
	}
	else
	{
		EnumDoc->AppendChildWithValueEscaped(TEXT("display_name"), Enum->GetName());
	}
	EnumDoc->AppendChild(TEXT("values"));
	return EnumDoc;
}

bool FNodeDocsGenerator::UpdateIndexDocWithClass(TSharedPtr<DocTreeNode> DocTree, UClass* Class)
{
	auto DocTreeClassesElement = DocTree->FindChildByName("classes");
	auto DocTreeClass = DocTreeClassesElement->AppendChild("class");
	DocTreeClass->AppendChildWithValueEscaped(TEXT("id"), GetClassDocId(Class));
	DocTreeClass->AppendChildWithValueEscaped(TEXT("display_name"),
											  FBlueprintEditorUtils::GetFriendlyClassDisplayName(Class).ToString());
	return true;
}

bool FNodeDocsGenerator::UpdateIndexDocWithStruct(TSharedPtr<DocTreeNode> DocTree, UStruct* Struct)
{
	auto DocTreeStructsElement = DocTree->FindChildByName("structs");
	auto DocTreeStruct = DocTreeStructsElement->AppendChild("struct");
	DocTreeStruct->AppendChildWithValueEscaped(TEXT("id"), Struct->GetName());
	if (Struct->HasMetaData(TEXT("DisplayName")))
	{
		DocTreeStruct->AppendChildWithValueEscaped(TEXT("display_name"), Struct->GetMetaData(TEXT("DisplayName")));
	}
	else
	{
		DocTreeStruct->AppendChildWithValueEscaped(TEXT("display_name"),
												   FName::NameToDisplayString(Struct->GetName(), false));
	}
	return true;
}

bool FNodeDocsGenerator::UpdateIndexDocWithEnum(TSharedPtr<DocTreeNode> DocTree, UEnum* Enum)
{
	auto DocTreeEnumsElement = DocTree->FindChildByName("enums");
	auto DocTreeEnum = DocTreeEnumsElement->AppendChild("enum");
	DocTreeEnum->AppendChildWithValueEscaped(TEXT("id"), Enum->GetName());
	if (Enum->HasMetaData(TEXT("DisplayName")))
	{
		DocTreeEnum->AppendChildWithValueEscaped(TEXT("display_name"), Enum->GetMetaData(TEXT("DisplayName")));
	}
	else
	{
		DocTreeEnum->AppendChildWithValueEscaped(TEXT("display_name"), Enum->GetName());
		// FName::NameToDisplayString(Enum->GetName(), false));
	}
	return true;
}

bool FNodeDocsGenerator::UpdateClassDocWithNode(TSharedPtr<DocTreeNode> DocTree, UEdGraphNode* Node)
{
	auto DocTreeNodesElement = DocTree->FindChildByName("nodes");
	auto DocTreeNode = DocTreeNodesElement->AppendChild("node");
	DocTreeNode->AppendChildWithValueEscaped(TEXT("id"), GetNodeDocId(Node));
	DocTreeNode->AppendChildWithValueEscaped(TEXT("shorttitle"),
											 Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
	return true;
}

inline bool ShouldDocumentPin(UEdGraphPin* Pin)
{
	return !Pin->bHidden;
}

bool FNodeDocsGenerator::GenerateNodeDocTree(UK2Node* Node, FNodeProcessingState& State)
{
	if (auto EventNode = Cast<UK2Node_Event>(Node))
	{
		return true; // Skip events
	}
	SCOPE_SECONDS_COUNTER(GenerateNodeDocsTime);

	auto NodeDocsPath = State.ClassDocsPath / TEXT("nodes");

	TSharedPtr<DocTreeNode> NodeDocFile = MakeShared<DocTreeNode>();
	NodeDocFile->AppendChildWithValueEscaped("docs_name", DocsTitle);
	NodeDocFile->AppendChildWithValueEscaped("class_id", State.ClassDocTree->FindChildByName("id")->GetValue());
	NodeDocFile->AppendChildWithValueEscaped("class_name",
											 State.ClassDocTree->FindChildByName("display_name")->GetValue());
	NodeDocFile->AppendChildWithValueEscaped("shorttitle",
											 Node->GetNodeTitle(ENodeTitleType::ListView).ToString().TrimEnd());

	FString NodeFullTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	auto TargetIdx = NodeFullTitle.Find(TEXT("Target is "), ESearchCase::CaseSensitive);
	if (TargetIdx != INDEX_NONE)
	{
		NodeFullTitle = NodeFullTitle.Left(TargetIdx).TrimEnd();
	}
	NodeDocFile->AppendChildWithValueEscaped("fulltitle", NodeFullTitle);

	FString NodeDesc = Node->GetTooltipText().ToString();
	TargetIdx = NodeDesc.Find(TEXT("Target is "), ESearchCase::CaseSensitive);
	if (TargetIdx != INDEX_NONE)
	{
		NodeDesc = NodeDesc.Left(TargetIdx).TrimEnd();
	}
	NodeDocFile->AppendChildWithValueEscaped("description", NodeDesc);

	NodeDocFile->AppendChildWithValueEscaped("imgpath", State.RelImageBasePath / State.ImageFilename);
	NodeDocFile->AppendChildWithValueEscaped("category", Node->GetMenuCategory().ToString());

	if (auto FuncNode = Cast<UK2Node_CallFunction>(Node))
	{
		auto Func = FuncNode->GetTargetFunction();
		if (Func)
		{
			NodeDocFile->AppendChildWithValueEscaped("funcname", Func->GetAuthoredName());
			NodeDocFile->AppendChildWithValueEscaped("rawcomment", Func->GetMetaData(TEXT("Comment")));
			NodeDocFile->AppendChildWithValue("static", Func->HasAnyFunctionFlags(FUNC_Static) ? "true" : "false");
			NodeDocFile->AppendChildWithValue("autocast",
											  Func->HasMetaData(TEXT("BlueprintAutocast")) ? "true" : "false");
			TArray<FStringFormatArg> Args;

			if (FProperty* RetProp = Func->GetReturnProperty())
			{
				FString ExtendedParameters;
				FString RetValType = RetProp->GetCPPType(&ExtendedParameters);
				Args.Add({RetValType + ExtendedParameters});
			}
			else
			{
				Args.Add({"void"});
			}
			Args.Add({Func->GetAuthoredName()});
			FString FuncParams;
			for (TFieldIterator<FProperty> PropertyIterator(Func);
				 PropertyIterator && (PropertyIterator->PropertyFlags & CPF_Parm | CPF_Parm); ++PropertyIterator)
			{
				FProperty* FuncParameter = *PropertyIterator;

				// Skip the return type as we handled it earlier
				if (FuncParameter->HasAllPropertyFlags(CPF_ReturnParm))
				{
					continue;
				}

				FString ExtendedParameters;
				FString ParamType = FuncParameter->GetCPPType(&ExtendedParameters);

				FString ParamString = ParamType + ExtendedParameters + " " + FuncParameter->GetAuthoredName();
				if (FuncParams.Len() != 0)
				{
					FuncParams.Append(", ");
				}
				FuncParams.Append(ParamString);
			}
			Args.Add({FuncParams});
			Args.Add({Func->HasAnyFunctionFlags(FUNC_Const) ? " const" : ""});
			NodeDocFile->AppendChildWithValueEscaped("rawsignature", FString::Format(TEXT("{0} {1}({2}){3}"), Args));

			auto Tags = Detail::ParseDoxygenTagsForString(Func->GetMetaData(TEXT("Comment")));
			if (Tags.Num())
			{
				auto DoxygenElement = NodeDocFile->AppendChild("doxygen");
				for (auto CurrentTag : Tags)
				{
					for (auto CurrentValue : CurrentTag.Value)
					{
						DoxygenElement->AppendChildWithValueEscaped(CurrentTag.Key, CurrentValue);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogKantanDocGen, Warning, TEXT("[KantanDocGen] Failed to get target function for node %s "),
				   *NodeFullTitle);
		}
	}
	else
	{
		UE_LOG(LogKantanDocGen, Warning, TEXT("[KantanDocGen] Cannot get type for node %s "), *NodeFullTitle);
	}
	auto InputNode = NodeDocFile->AppendChild("inputs");

	for (auto Pin : Node->Pins)
	{
		if (Pin->Direction == EEdGraphPinDirection::EGPD_Input)
		{
			if (ShouldDocumentPin(Pin))
			{
				auto Input = InputNode->AppendChild(TEXT("param"));

				FString PinName, PinType, PinDesc;
				ExtractPinInformation(Pin, PinName, PinType, PinDesc);

				Input->AppendChildWithValueEscaped(TEXT("name"), PinName);
				Input->AppendChildWithValueEscaped(TEXT("type"), PinType);
				Input->AppendChildWithValueEscaped(TEXT("description"), PinDesc);
			}
		}
	}

	auto OutputNode = NodeDocFile->AppendChild(TEXT("outputs"));
	for (auto Pin : Node->Pins)
	{
		if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			if (ShouldDocumentPin(Pin))
			{
				auto Output = OutputNode->AppendChild(TEXT("param"));

				FString PinName, PinType, PinDesc;
				ExtractPinInformation(Pin, PinName, PinType, PinDesc);

				Output->AppendChildWithValueEscaped(TEXT("name"), PinName);
				Output->AppendChildWithValueEscaped(TEXT("type"), PinType);
				Output->AppendChildWithValueEscaped(TEXT("description"), PinDesc);
			}
		}
	}

	for (const auto& FactoryObject : OutputFormats)
	{
		auto Serializer = FactoryObject->CreateSerializer();
		NodeDocFile->SerializeWith(Serializer);
		Serializer->SaveToFile(NodeDocsPath, GetNodeDocId(Node));
	}

	if (!UpdateClassDocWithNode(State.ClassDocTree, Node))
	{
		return false;
	}

	return true;
}

bool FNodeDocsGenerator::GenerateTypeMembers(UObject* Type)
{
	if (Type)
	{
		UE_LOG(LogKantanDocGen, Display, TEXT("generating type members for : %s"), *Type->GetName());
		if (Type->GetClass() == UClass::StaticClass())
		{
			UClass* ClassInstance = Cast<UClass>(Type);
			TSharedPtr<DocTreeNode>* FoundClassDocTree = ClassDocTreeMap.Find(ClassInstance);
			TSharedPtr<DocTreeNode> ClassDocTree;
			if (!FoundClassDocTree)
			{
				ClassDocTree = InitClassDocTree(ClassInstance);
			}
			else
			{
				ClassDocTree = *FoundClassDocTree;
			}
			bool bClassShouldBeDocumented = false;
			auto MemberList = ClassDocTree->FindChildByName("fields");
			for (TFieldIterator<FProperty> PropertyIterator(ClassInstance);
				 PropertyIterator && ((PropertyIterator->PropertyFlags & CPF_BlueprintVisible) ||
										  (PropertyIterator->HasAnyPropertyFlags(CPF_Deprecated))); ++PropertyIterator)
			{
				bClassShouldBeDocumented = true;
				UE_LOG(LogKantanDocGen, Display, TEXT("member for class found : %s"), *PropertyIterator->GetNameCPP());
				auto Member = MemberList->AppendChild(TEXT("field"));
				Member->AppendChildWithValueEscaped("name", PropertyIterator->GetNameCPP());
				FString ExtendedTypeString;
				FString TypeString = PropertyIterator->GetCPPType(&ExtendedTypeString);
				Member->AppendChildWithValueEscaped("type", TypeString + ExtendedTypeString);
				if (PropertyIterator->HasAnyPropertyFlags(CPF_Deprecated))
				{
					FText DetailedMessage =
						FText::FromString(PropertyIterator->GetMetaData(FBlueprintMetadata::MD_DeprecationMessage));
					Member->AppendChildWithValueEscaped("deprecated", DetailedMessage.ToString());
				}
				const FString& Comment = PropertyIterator->GetMetaData(TEXT("Comment"));
				auto MemberTags = Detail::ParseDoxygenTagsForString(Comment);
				if (MemberTags.Num())
				{
					auto DoxygenElement = Member->AppendChild("doxygen");
					for (auto CurrentTag : MemberTags)
					{
						for (auto CurrentValue : CurrentTag.Value)
						{
							DoxygenElement->AppendChildWithValueEscaped(CurrentTag.Key, CurrentValue);
						}
					}
				}
				else
				{
					// Avoid any property that is part of the superclass and then "redefined" in this Class
					bool IsInSuper = PropertyIterator->IsInContainer(ClassInstance->GetSuperClass());
					bool HasComment = Comment.Len() > 0;
					// UE_LOG(LogKantanDocGen, Warning, TEXT("Name: %s, comment (%i): %s"), *Type->GetName(),
					// Comment.Len(), *Comment);
					if (IsInSuper == false && HasComment == false)
					{
						bool IsPublic = PropertyIterator->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPublic);
						FString LogStr = FString::Printf(TEXT("##teamcity[message status='WARNING' text='No doc for "
															  "UClass-MemberTag (IsPublic %i): %s::%s']\n"),
														 IsPublic, *Type->GetName(), *PropertyIterator->GetNameCPP());
						FPlatformMisc::LocalPrint(*LogStr);
					}
				}
			}

			const FString& Comment = ClassInstance->GetMetaData(TEXT("Comment"));
			bool HasComment = Comment.Len() > 0;
			// UE_LOG(LogKantanDocGen, Warning, TEXT("UClass: %s, comment (%i): %s"), *Type->GetName(), Comment.Len(),
			// *Comment);

			// Only insert this into the map of classdocs if it wasnt already in there, we actually need it to be
			// included and does not have any comments
			if (!FoundClassDocTree && bClassShouldBeDocumented && HasComment == false)
			{
				ClassDocTreeMap.Add(ClassInstance, ClassDocTree);
				UpdateIndexDocWithClass(IndexTree, ClassInstance);
				FString LogStr = FString::Printf(
					TEXT("##teamcity[message status='WARNING' text='No doc for UClass: %s']\n"), *Type->GetName());
				FPlatformMisc::LocalPrint(*LogStr);
			}
		}
		else if (Type->GetClass() == UScriptStruct::StaticClass())
		{
			UScriptStruct* Struct = Cast<UScriptStruct>(Type);
			if (!Struct->HasAnyFlags(EObjectFlags::RF_ArchetypeObject | EObjectFlags::RF_ClassDefaultObject))
			{
				auto StructDocTree = InitStructDocTree(Struct);
				auto MemberList = StructDocTree->FindChildByName("fields");
				const FString& Comment = Struct->GetMetaData(TEXT("Comment"));
				bool HasComment = Comment.Len() > 0;
				auto StructTags = Detail::ParseDoxygenTagsForString(Comment);
				if (StructTags.Num())
				{
					auto DoxygenElement = StructDocTree->AppendChild("doxygen");
					for (auto CurrentTag : StructTags)
					{
						for (auto CurrentValue : CurrentTag.Value)
						{
							DoxygenElement->AppendChildWithValueEscaped(CurrentTag.Key, CurrentValue);
						}
					}
				}
				else if (HasComment == false)
				{
					FString LogStr = FString::Printf(
						TEXT("##teamcity[message status='WARNING' text='Warning in UScriptStruct: %s']\n"),
						*Type->GetName());
					FPlatformMisc::LocalPrint(*LogStr);
				}

				for (TFieldIterator<FProperty> PropertyIterator(Struct);
					 PropertyIterator && ((PropertyIterator->PropertyFlags & CPF_BlueprintVisible) ||
										  (PropertyIterator->HasAnyPropertyFlags(CPF_Deprecated)));
					 ++PropertyIterator)
				{
					// Move into its own function for use in parsing classes
					auto Member = MemberList->AppendChild("field");
					Member->AppendChildWithValueEscaped("name", PropertyIterator->GetNameCPP());
					FString ExtendedTypeString;
					FString TypeString = PropertyIterator->GetCPPType(&ExtendedTypeString);

					Member->AppendChildWithValueEscaped("type", TypeString + ExtendedTypeString);
					if (PropertyIterator->HasAnyPropertyFlags(CPF_Deprecated))
					{
						FText DetailedMessage =
							FText::FromString(PropertyIterator->GetMetaData(FBlueprintMetadata::MD_DeprecationMessage));
						Member->AppendChildWithValueEscaped("deprecated", DetailedMessage.ToString());
					}
					const FString& CommentIterator = PropertyIterator->GetMetaData(TEXT("Comment"));
					bool HasCommentIterator = CommentIterator.Len() > 0;
					auto MemberTags = Detail::ParseDoxygenTagsForString(CommentIterator);
					if (MemberTags.Num())
					{
						auto DoxygenElement = Member->AppendChild("doxygen");
						for (auto CurrentTag : MemberTags)
						{
							for (auto CurrentValue : CurrentTag.Value)
							{
								DoxygenElement->AppendChildWithValueEscaped(CurrentTag.Key, CurrentValue);
							}
						}
					}
					else if (HasCommentIterator == false)
					{
						// Avoid any property that is part of the parent struct and then "redefined" in this Struct
						bool IsInSuper = PropertyIterator->IsInContainer(Struct->GetSuperStruct());

						if (IsInSuper == false)
						{
							FString LogStr = FString::Printf(
								TEXT("##teamcity[message status='WARNING' text='Warning in UScriptStruct-property: "
									 "%s::%s']\n"),
								*Type->GetName(), *PropertyIterator->GetNameCPP());
							FPlatformMisc::LocalPrint(*LogStr);
						}
					}
				}

				StructDocTreeMap.Add(Struct, StructDocTree);
				UpdateIndexDocWithStruct(IndexTree, Struct);
			}
		}
		else if (Type->GetClass() == UEnum::StaticClass())
		{
			UEnum* EnumInstance = Cast<UEnum>(Type);
			if ((EnumInstance != NULL) && EnumInstance->HasAnyFlags(RF_NeedLoad))
			{
				EnumInstance->GetLinker()->Preload(EnumInstance);
			}
			EnumInstance->ConditionalPostLoad();

			auto EnumDocTree = InitEnumDocTree(EnumInstance);
			const FString& Comment = EnumInstance->GetMetaData(TEXT("Comment"));
			bool HasComment = Comment.Len() > 0;
			auto EnumTags = Detail::ParseDoxygenTagsForString(Comment);
			if (EnumTags.Num())
			{
				auto DoxygenElement = EnumDocTree->AppendChild("doxygen");
				for (auto CurrentTag : EnumTags)
				{
					for (auto CurrentValue : CurrentTag.Value)
					{
						DoxygenElement->AppendChildWithValueEscaped(CurrentTag.Key, CurrentValue);
					}
				}
			}
			else if (HasComment == false)
			{
				FString LogStr = FString::Printf(
					TEXT("##teamcity[message status='WARNING' text='Warning in UEnum %s']\n"), *Type->GetName());
				FPlatformMisc::LocalPrint(*LogStr);
			}

			auto ValueList = EnumDocTree->FindChildByName("values");
			for (int32 EnumIndex = 0; EnumIndex < EnumInstance->NumEnums() - 1; ++EnumIndex)
			{
				bool const bShouldBeHidden = EnumInstance->HasMetaData(TEXT("Hidden"), EnumIndex) ||
											 EnumInstance->HasMetaData(TEXT("Spacer"), EnumIndex);
				if (!bShouldBeHidden)
				{
					auto Value = ValueList->AppendChild("value");
					Value->AppendChildWithValueEscaped("name", EnumInstance->GetNameStringByIndex(EnumIndex));
					Value->AppendChildWithValueEscaped("displayname",
													   EnumInstance->GetDisplayNameTextByIndex(EnumIndex).ToString());
					Value->AppendChildWithValueEscaped("description",
													   EnumInstance->GetToolTipTextByIndex(EnumIndex).ToString());
				}
			}
			UpdateIndexDocWithEnum(IndexTree, EnumInstance);
			EnumDocTreeMap.Add(EnumInstance, EnumDocTree);
		}
	}

	return true;
}

bool FNodeDocsGenerator::SaveIndexFile(FString const& OutDir)
{
	for (const auto& FactoryObject : OutputFormats)
	{
		auto Serializer = FactoryObject->CreateSerializer();
		IndexTree->SerializeWith(Serializer);
		Serializer->SaveToFile(OutDir, "index");
	}
	return true;
}

bool FNodeDocsGenerator::SaveClassDocFile(FString const& OutDir)
{
	for (const auto& Entry : ClassDocTreeMap)
	{
		auto ClassId = GetClassDocId(Entry.Key.Get());
		auto Path = OutDir / ClassId;
		auto DummyImagePath = OutDir / ClassId / "img";
		if (!IFileManager::Get().DirectoryExists(*DummyImagePath))
		{
			IFileManager::Get().MakeDirectory(*DummyImagePath);
		}
		for (const auto& FactoryObject : OutputFormats)
		{
			auto Serializer = FactoryObject->CreateSerializer();
			Entry.Value->SerializeWith(Serializer);
			Serializer->SaveToFile(Path, ClassId);
		}
	}
	return true;
}

bool FNodeDocsGenerator::SaveEnumDocFile(FString const& OutDir)
{
	for (const auto& Entry : EnumDocTreeMap)
	{
		auto EnumId = Entry.Key.Get()->GetName();
		auto Path = OutDir / EnumId;
		auto DummyImagePath = OutDir / EnumId / "img";
		if (!IFileManager::Get().DirectoryExists(*DummyImagePath))
		{
			IFileManager::Get().MakeDirectory(*DummyImagePath, true);
		}
		for (const auto& FactoryObject : OutputFormats)
		{
			auto Serializer = FactoryObject->CreateSerializer();
			Entry.Value->SerializeWith(Serializer);
			Serializer->SaveToFile(Path, EnumId);
		}
	}
	return true;
}

bool FNodeDocsGenerator::SaveStructDocFile(FString const& OutDir)
{
	for (const auto& Entry : StructDocTreeMap)
	{
		auto StructId = Entry.Key.Get()->GetName();
		auto Path = OutDir / StructId;
		auto DummyImagePath = OutDir / StructId / "img";
		if (!IFileManager::Get().DirectoryExists(*DummyImagePath))
		{
			IFileManager::Get().MakeDirectory(*DummyImagePath, true);
		}

		for (const auto& FactoryObject : OutputFormats)
		{
			auto Serializer = FactoryObject->CreateSerializer();
			Entry.Value->SerializeWith(Serializer);
			Serializer->SaveToFile(Path, StructId);
		}
	}
	return true;
}

void FNodeDocsGenerator::AdjustNodeForSnapshot(UEdGraphNode* Node)
{
	// Hide default value box containing 'self' for Target pin
	if (auto K2_Schema = Cast<UEdGraphSchema_K2>(Node->GetSchema()))
	{
		if (auto TargetPin = Node->FindPin(K2_Schema->PN_Self))
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

#include "BlueprintDelegateNodeSpawner.h"
#include "BlueprintVariableNodeSpawner.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"

/*
This takes a graph node object and attempts to map it to the class which the node conceptually belong to.
If there is no special mapping for the node, the function determines the class from the source object.
*/
UClass* FNodeDocsGenerator::MapToAssociatedClass(UK2Node* NodeInst, UObject* Source)
{
	// For nodes derived from UK2Node_CallFunction, associate with the class owning the called function.
	if (auto FuncNode = Cast<UK2Node_CallFunction>(NodeInst))
	{
		auto Func = FuncNode->GetTargetFunction();
		if (Func)
		{
			return Func->GetOwnerClass();
		}
	}

	// Default fallback
	if (auto SourceClass = Cast<UClass>(Source))
	{
		return SourceClass;
	}
	else if (auto SourceBP = Cast<UBlueprint>(Source))
	{
		return SourceBP->GeneratedClass;
	}
	else
	{
		return nullptr;
	}
}

bool FNodeDocsGenerator::IsSpawnerDocumentable(UBlueprintNodeSpawner* Spawner, bool bIsBlueprint)
{
	// Spawners of or deriving from the following classes will be excluded
	static const TSubclassOf<UBlueprintNodeSpawner> ExcludedSpawnerClasses[] = {
		UBlueprintVariableNodeSpawner::StaticClass(),
		UBlueprintDelegateNodeSpawner::StaticClass(),
		UBlueprintBoundNodeSpawner::StaticClass(),
		UBlueprintComponentNodeSpawner::StaticClass(),
	};

	// Spawners of or deriving from the following classes will be excluded in a blueprint context
	static const TSubclassOf<UBlueprintNodeSpawner> BlueprintOnlyExcludedSpawnerClasses[] = {
		UBlueprintEventNodeSpawner::StaticClass(),
	};

	// Spawners for nodes of these types (or their subclasses) will be excluded
	static const TSubclassOf<UK2Node> ExcludedNodeClasses[] = {
		UK2Node_DynamicCast::StaticClass(),
		UK2Node_Message::StaticClass(),
	};

	// Function spawners for functions with any of the following metadata tags will also be excluded
	static const FName ExcludedFunctionMeta[] = {TEXT("BlueprintAutocast")};

	static const uint32 PermittedAccessSpecifiers = (FUNC_Public | FUNC_Protected);

	for (auto ExclSpawnerClass : ExcludedSpawnerClasses)
	{
		if (Spawner->IsA(ExclSpawnerClass))
		{
			return false;
		}
	}

	if (bIsBlueprint)
	{
		for (auto ExclSpawnerClass : BlueprintOnlyExcludedSpawnerClasses)
		{
			if (Spawner->IsA(ExclSpawnerClass))
			{
				return false;
			}
		}
	}

	for (auto ExclNodeClass : ExcludedNodeClasses)
	{
		if (Spawner->NodeClass->IsChildOf(ExclNodeClass))
		{
			return false;
		}
	}

	if (auto FuncSpawner = Cast<UBlueprintFunctionNodeSpawner>(Spawner))
	{
		auto Func = FuncSpawner->GetFunction();

		// @NOTE: We exclude based on access level, but only if this is not a spawner for a blueprint event
		// (custom events do not have any access specifiers)
		if ((Func->FunctionFlags & FUNC_BlueprintEvent) == 0 && (Func->FunctionFlags & PermittedAccessSpecifiers) == 0)
		{
			return false;
		}

		for (auto const& Meta : ExcludedFunctionMeta)
		{
			if (Func->HasMetaData(Meta))
			{
				return false;
			}
		}
	}

	return true;
}
