#pragma once
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "VariantWrapper.h"

#include "DocGenOutput.generated.h"

class DocTreeNode
{
	using Array = TArray<TSharedPtr<DocTreeNode>>;
	using Object = TMultiMap<FString, TSharedPtr<DocTreeNode>>;
	struct NullValue
	{};

	enum class InternalDataType
	{
		Array,
		String,
		Object,
		Null
	};
	// Nodes are either an array, or an object, or a string value
	DocGen::Variant<Object, Array, FString, NullValue> Value;
	InternalDataType CurrentDataType;

public:
	void SetValue(const FString& NewValue)
	{
		CurrentDataType = InternalDataType::String;
		Value.Set<FString>(NewValue);
	}

};

UCLASS(abstract)
class UDocTree : public UObject
{
	GENERATED_BODY()

	TSharedPtr<DocTreeNode> RootNode;

public:
	TSharedPtr<DocTreeNode> GetRootNode() const
	{
		return RootNode;
	}
};

/*

UINTERFACE()
class UDocGenOutput : public UInterface
{
	GENERATED_BODY();
};

class IDocGenOutput
{
	GENERATED_BODY();

	virtual bool LoadFile(const FString& Path);
	virtual bool LoadBuffer(const FString& Buffer);
	virtual IOutputNode* GetRootNode();
	virtual bool Save(const FString& OutputPath);
};*/