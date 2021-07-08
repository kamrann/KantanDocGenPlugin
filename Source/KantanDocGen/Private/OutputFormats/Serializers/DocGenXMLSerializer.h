#pragma once
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "DocGenOutput.h"
#include "OutputFormats/DocGenSerializerFactory.h"
#include "XmlFile.h"

#include "DocGenXMLSerializer.generated.h"

class DocGenXMLSerializer : public DocTreeNode::IDocTreeSerializer
{
	TSharedPtr<FXmlFile> TopLevelFile;
	FXmlNode* TargetNode;
	virtual FString EscapeString(const FString& InString) override
	{
		return TEXT("<![CDATA[") + InString + TEXT("]]>");
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
		const FString FileTemplate = R"xxx(<?xml version="1.0" encoding="UTF-8"?><root></root>)xxx";

		TopLevelFile = MakeShared<FXmlFile>(FileTemplate, EConstructMethod::ConstructFromBuffer);
		TargetNode = TopLevelFile->GetRootNode();
	}
	virtual bool SaveToFile(const FString& OutFile)
	{
		if (!TopLevelFile)
		{
			return false;
		}
		return TopLevelFile->Save(OutFile);
	};
};

UCLASS()
class UDocGenXMLSerializerFactory : public UObject, public IDocGenSerializerFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<struct DocTreeNode::IDocTreeSerializer> CreateSerializer() override
	{
		return MakeShared<DocGenXMLSerializer>();
	}
};