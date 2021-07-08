#pragma once
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "VariantWrapper.h"

class DocTreeNode : public TSharedFromThis<DocTreeNode>
{
public:
	using Object = TMultiMap<FString, TSharedPtr<DocTreeNode>>;

private:
	struct NullValue
	{};

	enum class InternalDataType
	{
		String,
		Object,
		Null
	};
	// Nodes are either an array, or an object, or a string value
	DocGen::Variant<Object, FString, NullValue> Value;
	InternalDataType CurrentDataType = InternalDataType::Null;
	bool bValueRequiresEscaping = false;

public:
	void SetValue(const FString& NewValue, bool bEscapeValue = false)
	{
		if (CurrentDataType == InternalDataType::Null)
		{
			Value.Set<FString>(FString());
			CurrentDataType = InternalDataType::String;
		}
		auto StringPtr = Value.TryGet<FString>();
		check(StringPtr);
		bValueRequiresEscaping = bEscapeValue;
		Value.Set<FString>(NewValue);
	}

	const FString& GetValue()
	{
		if (CurrentDataType == InternalDataType::Null)
		{
			Value.Set<FString>(FString());
			CurrentDataType = InternalDataType::String;
		}
		auto StringPtr = Value.TryGet<FString>();
		check(StringPtr);
		return *StringPtr;
	}

	TSharedPtr<DocTreeNode> FindChildByName(const FString& ChildName)
	{
		Object* ObjPtr = Value.TryGet<Object>();
		check(ObjPtr);
		TSharedPtr<DocTreeNode>* FoundChild = ObjPtr->Find(ChildName);
		if (FoundChild != nullptr)
		{
			return *FoundChild;
		}
		else
		{
			return TSharedPtr<DocTreeNode>();
		}
	}
	TSharedPtr<DocTreeNode> AppendChild(const FString& ChildName)
	{
		if (CurrentDataType == InternalDataType::Null)
		{
			Value.Set<Object>(Object());
			CurrentDataType = InternalDataType::Object;
		}
		auto ObjPtr = Value.TryGet<Object>();
		check(ObjPtr);
		TSharedPtr<DocTreeNode> NewChild = MakeShared<DocTreeNode>();
		ObjPtr->Add(ChildName, NewChild);
		return NewChild;
	}

	TSharedPtr<DocTreeNode> AppendChildWithValue(const FString& ChildName, const FString& NewValue)
	{
		TSharedPtr<DocTreeNode> NewChild = AppendChild(ChildName);
		NewChild->SetValue(NewValue);
		return NewChild;
	}

	TSharedPtr<DocTreeNode> AppendChildWithValueEscaped(const FString& ChildName, const FString& NewValue)
	{
		TSharedPtr<DocTreeNode> NewChild = AppendChild(ChildName);
		NewChild->SetValue(NewValue, true);
		return NewChild;
	}

	struct IDocTreeSerializer
	{
		virtual FString EscapeString(const FString& InString) = 0;
		virtual void SerializeObject(const Object& Object) = 0;
		virtual void SerializeString(const FString& InString) = 0;
		virtual void SerializeNull() = 0;
		virtual bool SaveToFile(const FString& OutFile) = 0;
		virtual ~IDocTreeSerializer() {};
	};

	void SerializeWith(TSharedPtr<IDocTreeSerializer> Serializer)
	{
		switch (CurrentDataType)
		{
			case InternalDataType::Null:
				Serializer->SerializeNull();
				break;
			case InternalDataType::Object:
				Serializer->SerializeObject(Value.Get<Object>());
				break;
			case InternalDataType::String:
				if (bValueRequiresEscaping)
				{
					Serializer->SerializeString(Serializer->EscapeString(Value.Get<FString>()));
				}
				else
				{
					Serializer->SerializeString(Value.Get<FString>());
				}
		}
	}
};
