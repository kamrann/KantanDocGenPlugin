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

class IDocTreeObject;

class IDocTreeArray
{
public:
	virtual TSharedPtr<IDocTreeArray> AddArray(const FString& ArrayName) = 0;
	virtual TSharedPtr<IDocTreeObject> AddObject(const FString& ObjectName) = 0;
};

class IDocTreeObject
{
public:
	virtual TSharedPtr<IDocTreeObject> AddObject(const FString& FieldName) = 0;
	virtual TSharedPtr<IDocTreeArray> AddArray(const FString& FieldName) = 0;

	virtual void AddField(const FString& FieldName, const FString& FieldValue ) = 0;
	virtual void AddField(const FString& FieldName, const double& FieldValue ) = 0;
	virtual void AddField(const FString& FieldName, const uint64 FieldValue) = 0;
};

class XMLDocTreeArray : public IDocTreeArray
{
	virtual TSharedPtr<IDocTreeArray> AddArray(const FString& ArrayName)
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