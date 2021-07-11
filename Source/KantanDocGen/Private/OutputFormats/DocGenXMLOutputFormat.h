#pragma once
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "DocTreeNode.h"
#include "OutputFormats/DocGenOutputFormatFactory.h"
#include "OutputFormats/DocGenOutputProcessor.h"
#include "XmlFile.h"

#include "DocGenXMLOutputFormat.generated.h"

class DocGenXMLSerializer : public DocTreeNode::IDocTreeSerializer
{
	TSharedPtr<FXmlFile> TopLevelFile;
	FXmlNode* TargetNode;
	virtual FString EscapeString(const FString& InString) override
	{
		return TEXT("<![CDATA[") + InString + TEXT("]]>");
	}
	virtual FString GetFileExtension() override
	{
		return ".xml";
	}
	virtual void SerializeObject(const DocTreeNode::Object& Obj) override
	{
		for (auto& Member : Obj)
		{
			TargetNode->AppendChildNode(Member.Key, FString());
			Member.Value->SerializeWith(MakeShared<DocGenXMLSerializer>(TargetNode->GetChildrenNodes().Last()));
		}
		// for each value in Obj
		// create a node using the key as a name
		// then call SerializeWith and pass ourselves
	}
	virtual void SerializeString(const FString& InString) override
	{
		TargetNode->SetContent(InString);
	}
	virtual void SerializeNull() override {}

public:
	// constructor taking initial document state
	DocGenXMLSerializer(FXmlNode* TargetNode) : TargetNode(TargetNode) {}
	DocGenXMLSerializer()
	{
		const FString FileTemplate = R"xxx(<?xml version="1.0" encoding="UTF-8"?>)xxx"
									 "\r\n"
									 R"xxx(<root></root>)xxx";

		TopLevelFile = MakeShared<FXmlFile>(FileTemplate, EConstructMethod::ConstructFromBuffer);
		TargetNode = TopLevelFile->GetRootNode();
	}
	virtual bool SaveToFile(const FString& OutFileDirectory, const FString& OutFileName)
	{
		if (!TopLevelFile)
		{
			return false;
		}
		return TopLevelFile->Save(OutFileDirectory / OutFileName + GetFileExtension());
	};
};

class DocGenXMLOutputProcessor : public IDocGenOutputProcessor
{
public:
	virtual EIntermediateProcessingResult ProcessIntermediateDocs(FString const& IntermediateDir,
																  FString const& OutputDir, FString const& DocTitle,
																  bool bCleanOutput) override;
};

UCLASS(meta = (DisplayName = "XML"))
class UDocGenXMLOutputFactory : public UDocGenOutputFormatFactoryBase
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<struct DocTreeNode::IDocTreeSerializer> CreateSerializer() override
	{
		return MakeShared<DocGenXMLSerializer>();
	}
	virtual TSharedPtr<struct IDocGenOutputProcessor> CreateIntermediateDocProcessor() override
	{
		return MakeShared<DocGenXMLOutputProcessor>();
	}
	virtual FString GetFormatIdentifier() override
	{
		return "xml";
	}
};