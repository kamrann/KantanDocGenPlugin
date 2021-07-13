#include "OutputFormats/DocGenXMLOutputFormat.h"
#include "OutputFormats/DocGenXMLOutputProcessor.h"
#include "XmlFile.h"
 
FString DocGenXMLSerializer::EscapeString(const FString& InString)
{
	return TEXT("<![CDATA[") + InString + TEXT("]]>");
}

FString DocGenXMLSerializer::GetFileExtension()
{
	return ".xml";
}

void DocGenXMLSerializer::SerializeObject(const DocTreeNode::Object& Obj)
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

void DocGenXMLSerializer::SerializeString(const FString& InString)
{
	TargetNode->SetContent(InString);
}

void DocGenXMLSerializer::SerializeNull() {}

DocGenXMLSerializer::DocGenXMLSerializer()
{
	const FString FileTemplate = R"xxx(<?xml version="1.0" encoding="UTF-8"?>)xxx"
								 "\r\n"
								 R"xxx(<root></root>)xxx";

	TopLevelFile = MakeShared<FXmlFile>(FileTemplate, EConstructMethod::ConstructFromBuffer);
	TargetNode = TopLevelFile->GetRootNode();
}

DocGenXMLSerializer::DocGenXMLSerializer(FXmlNode* TargetNode) : TargetNode(TargetNode) {}

bool DocGenXMLSerializer::SaveToFile(const FString& OutFileDirectory, const FString& OutFileName)
{
	if (!TopLevelFile)
	{
		return false;
	}
	return TopLevelFile->Save(OutFileDirectory / OutFileName + GetFileExtension());
}

TSharedPtr<struct DocTreeNode::IDocTreeSerializer> UDocGenXMLOutputFactory::CreateSerializer()
{
	return MakeShared<DocGenXMLSerializer>();
}

TSharedPtr<struct IDocGenOutputProcessor> UDocGenXMLOutputFactory::CreateIntermediateDocProcessor()
{
	return MakeShared<DocGenXMLOutputProcessor>();
}

FString UDocGenXMLOutputFactory::GetFormatIdentifier()
{
	return "xml";
}

void UDocGenXMLOutputFactory::LoadSettings(const FDocGenOutputFormatFactorySettings& Settings) {}

FDocGenOutputFormatFactorySettings UDocGenXMLOutputFactory::SaveSettings()
{
	FDocGenOutputFormatFactorySettings Settings;
	Settings.FactoryClass = StaticClass();
	return Settings;
}
