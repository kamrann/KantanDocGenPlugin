#pragma once

#include "DocTreeNode.h"
#include "OutputFormats/DocGenOutputFormatFactory.h"
#include "OutputFormats/DocGenOutputProcessor.h"

#include "DocGenOutputFormatFactoryBase.generated.h"

UCLASS(Abstract, DefaultToInstanced, PerObjectConfig, EditInlineNew, Meta = (ShowOnlyInnerProperties),
	   Config = EditorPerProjectUserSettings)
class KANTANDOCGEN_API UDocGenOutputFormatFactoryBase : public UObject, public IDocGenOutputFormatFactory
{
	GENERATED_BODY()

public:
	virtual FString GetFormatIdentifier()
		PURE_VIRTUAL(IDocGenOutputFormatFactory::GetFormatIdentifier, return FString(););
	virtual TSharedPtr<DocTreeNode::IDocTreeSerializer> CreateSerializer()
		PURE_VIRTUAL(IDocGenOutputFormatFactory::CreateSerializer, return nullptr;);
	virtual TSharedPtr<struct IDocGenOutputProcessor> CreateIntermediateDocProcessor()
		PURE_VIRTUAL(IDocGenOutputFormatFactory::CreateIntermediateDocProcessor,
					 return nullptr;) virtual void LoadSettings(const FDocGenOutputFormatFactorySettings& Settings)
			PURE_VIRTUAL(IDocGenOutputFormatFactory::LoadSettings, );
	virtual FDocGenOutputFormatFactorySettings SaveSettings()
		PURE_VIRTUAL(IDocGenOutputFormatFactory::SaveSettings, return {};);
};