#pragma once
#include "Templates/SharedPointer.h"
#include "UObject/Interface.h"

#include "DocGenOutputFormatFactory.generated.h"

UINTERFACE(BlueprintType)
class UDocGenOutputFormatFactory : public UInterface
{
	GENERATED_BODY()
};

class KANTANDOCGEN_API IDocGenOutputFormatFactory
{
	GENERATED_BODY()
	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/// @brief returns a string identifier for this output format to make specifying formats on the command line easier
	virtual FString GetFormatIdentifier() = 0;
	/// @brief Constructs an instance of an object implementing the doc tree serialization interface
	/// @return shared pointer to the instance
	virtual TSharedPtr<struct DocTreeNode::IDocTreeSerializer> CreateSerializer() = 0;
	/// @brief Constructs an instance of an object implementing the post-processing interface for document generation
	/// @return shared pointer to the instance
	virtual TSharedPtr<struct IDocGenOutputProcessor> CreateIntermediateDocProcessor() = 0;
};

USTRUCT(BlueprintType)
struct FDocGenOutputSettings
{
	GENERATED_BODY()
	
	/// @brief Overrides the template file used when rendering the output
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFilePath TemplateFile;
};

UCLASS(Abstract, EditInlineNew)
class UDocGenOutputFormatFactoryBase : public UObject, public IDocGenOutputFormatFactory
{
	GENERATED_BODY()

public:
	virtual FString GetFormatIdentifier()
		PURE_VIRTUAL(IDocGenOutputFormatFactory::GetFormatIdentifier, return FString(););
	virtual TSharedPtr<struct DocTreeNode::IDocTreeSerializer> CreateSerializer()
		PURE_VIRTUAL(IDocGenOutputFormatFactory::CreateSerializer, return nullptr;);
	virtual TSharedPtr<struct IDocGenOutputProcessor> CreateIntermediateDocProcessor()
		PURE_VIRTUAL(IDocGenOutputFormatFactory::CreateIntermediateDocProcessor, return nullptr;)

	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	FDocGenOutputSettings OutputSettings;
};