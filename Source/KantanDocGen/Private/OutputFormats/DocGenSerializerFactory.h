#pragma once
#include "UObject/Interface.h"
#include "Templates/SharedPointer.h"

#include "DocGenSerializerFactory.generated.h"

UINTERFACE()
class UDocGenSerializerFactory : public UInterface
{
	GENERATED_BODY()
};

class KANTANDOCGEN_API IDocGenSerializerFactory
{
	GENERATED_BODY()
	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual TSharedPtr<struct DocTreeNode::IDocTreeSerializer> CreateSerializer()  = 0;
};
