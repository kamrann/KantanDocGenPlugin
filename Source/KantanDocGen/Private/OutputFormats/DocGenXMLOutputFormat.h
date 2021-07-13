#pragma once
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "DocTreeNode.h"
#include "OutputFormats/DocGenOutputFormatFactoryBase.h"

#include "DocGenXMLOutputFormat.generated.h"

class DocGenXMLSerializer : public DocTreeNode::IDocTreeSerializer
{
	TSharedPtr<class FXmlFile> TopLevelFile;
	class FXmlNode* TargetNode;
	virtual FString EscapeString(const FString& InString) override;
	virtual FString GetFileExtension() override;
	virtual void SerializeObject(const DocTreeNode::Object& Obj) override;
	virtual void SerializeString(const FString& InString) override;
	virtual void SerializeNull() override;

public:
	// constructor taking initial document state
	DocGenXMLSerializer(FXmlNode* TargetNode);
	DocGenXMLSerializer();
	virtual bool SaveToFile(const FString& OutFileDirectory, const FString& OutFileName);;
};



UCLASS(meta = (DisplayName = "XML"), Meta = (ShowOnlyInnerProperties))
class UDocGenXMLOutputFactory : public UDocGenOutputFormatFactoryBase
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<struct DocTreeNode::IDocTreeSerializer> CreateSerializer() override;
	virtual TSharedPtr<struct IDocGenOutputProcessor> CreateIntermediateDocProcessor() override;
	virtual FString GetFormatIdentifier() override;

	virtual void LoadSettings(const FDocGenOutputFormatFactorySettings& Settings) override;

	virtual FDocGenOutputFormatFactorySettings SaveSettings() override;;
};