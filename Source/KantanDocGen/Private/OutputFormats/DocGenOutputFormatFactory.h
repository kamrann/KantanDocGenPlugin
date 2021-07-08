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
	virtual TSharedPtr<struct DocTreeNode::IDocTreeSerializer> CreateSerializer() = 0;
	virtual TSharedPtr<struct IDocGenOutputProcessor> CreateIntermediateDocProcessor() = 0;
};
