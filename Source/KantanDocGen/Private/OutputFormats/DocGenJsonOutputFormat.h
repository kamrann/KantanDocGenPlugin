#pragma once
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "DocTreeNode.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "OutputFormats/DocGenJsonOutputProcessor.h"
#include "OutputFormats/DocGenOutputFormatFactory.h"
#include "OutputFormats/DocGenOutputProcessor.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Templates/SharedPointer.h"

#include "DocGenJsonOutputFormat.generated.h"

class DocGenJsonSerializer : public DocTreeNode::IDocTreeSerializer
{
	TSharedPtr<FJsonValue> TopLevelObject;
	TSharedPtr<FJsonValue>& TargetObject;

	virtual FString EscapeString(const FString& InString) override
	{
		return InString;
	}
	virtual FString GetFileExtension() override
	{
		return ".json";
	}
	virtual void SerializeObject(const DocTreeNode::Object& Obj) override
	{
		TArray<FString> ObjectFieldNames;
		// If we have a single key...
		if (Obj.GetKeys(ObjectFieldNames) == 1)
		{
			TArray<TSharedPtr<DocTreeNode>> ArrayFieldValues;
			// If that single key has multiple values...
			Obj.MultiFind(ObjectFieldNames[0], ArrayFieldValues, true);
			if (ArrayFieldValues.Num() > 1)
			{
				// we are an array
				TargetObject = SerializeArray(ArrayFieldValues);
				return;
			}
		}

		if (!TargetObject.IsValid())
		{
			TargetObject = MakeShared<FJsonValueObject>(MakeShared<FJsonObject>());
		}
		for (auto& FieldName : ObjectFieldNames)
		{
			TArray<TSharedPtr<DocTreeNode>> ArrayFieldValues;
			Obj.MultiFind(FieldName, ArrayFieldValues, true);
			if (ArrayFieldValues.Num() > 1)
			{
				TargetObject->AsObject()->SetField(FieldName, SerializeArray(ArrayFieldValues));
			}
			else
			{
				TSharedPtr<FJsonValue> NewMemberValue;
				ArrayFieldValues[0]->SerializeWith(MakeShared<DocGenJsonSerializer>(NewMemberValue));
				TargetObject->AsObject()->SetField(FieldName, NewMemberValue);
			}
		}
	}

	TSharedPtr<FJsonValueArray> SerializeArray(const TArray<TSharedPtr<DocTreeNode>> ArrayElements)
	{
		TArray<TSharedPtr<FJsonValue>> OutArray;
		for (auto& Element : ArrayElements)
		{
			TSharedPtr<FJsonValue> NewElementValue;
			Element->SerializeWith(MakeShared<DocGenJsonSerializer>(NewElementValue));
			OutArray.Add(NewElementValue);
		}
		return MakeShared<FJsonValueArray>(OutArray);
	}

	virtual void SerializeString(const FString& InString) override
	{
		TargetObject = MakeShared<FJsonValueString>(InString);
	}
	virtual void SerializeNull() override
	{
		TargetObject = MakeShared<FJsonValueNull>();
	}

public:
	DocGenJsonSerializer(TSharedPtr<FJsonValue>& TargetObject) : TargetObject(TargetObject) {};
	DocGenJsonSerializer()
		: TopLevelObject(MakeShared<FJsonValueObject>(MakeShared<FJsonObject>())),
		  TargetObject(TopLevelObject)
	{}
	virtual bool SaveToFile(const FString& OutFileDirectory, const FString& OutFileName)
	{
		if (!TopLevelObject)
		{
			return false;
		}
		else
		{
			FString Result;
			auto JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Result);
			FJsonSerializer::Serialize(TopLevelObject->AsObject().ToSharedRef(), JsonWriter);

			return FFileHelper::SaveStringToFile(Result, *(OutFileDirectory / OutFileName + GetFileExtension()),
												 FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
	};
};

UCLASS(meta = (DisplayName = "JSON"), Meta = (ShowOnlyInnerProperties), Config = EditorPerProjectUserSettings)
class UDocGenJsonOutputFactory : public UDocGenOutputFormatFactoryBase
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<struct DocTreeNode::IDocTreeSerializer> CreateSerializer() override
	{
		return MakeShared<DocGenJsonSerializer>();
	}
	virtual TSharedPtr<struct IDocGenOutputProcessor> CreateIntermediateDocProcessor() override
	{
		TOptional<FFilePath> TemplateOverride;
		if (bOverrideTemplatePath)
		{
			TemplateOverride = TemplatePath;
		}

		TOptional<FDirectoryPath> BinaryOverride;
		if (bOverrideBinaryPath)
		{
			BinaryOverride = BinaryPath;
		}
		TOptional<FFilePath> RubyOverride;
		if (bOverrideRubyPath)
		{
			RubyOverride = RubyPath;
		}
		return MakeShared<DocGenJsonOutputProcessor>(TemplateOverride, BinaryOverride, RubyOverride);
	}
	virtual FString GetFormatIdentifier() override
	{
		return "json";
	}

	virtual void LoadSettings(const FDocGenOutputFormatFactorySettings& Settings)
	{
		if (Settings.SettingValues.Contains("template"))
		{
			TemplatePath.FilePath = Settings.SettingValues["template"];
			if (Settings.SettingValues.Contains("overridetemplate"))
			{
				bOverrideTemplatePath = (Settings.SettingValues["overridetemplate"] == "true");
			}
		}
		if (Settings.SettingValues.Contains("bindir"))
		{
			BinaryPath.Path = Settings.SettingValues["bindir"];
			if (Settings.SettingValues.Contains("overridebindir"))
			{
				bOverrideBinaryPath = (Settings.SettingValues["overridebindir"] == "true");
			}
		}
		if (Settings.SettingValues.Contains("ruby"))
		{
			RubyPath.FilePath = Settings.SettingValues["ruby"];
			if (Settings.SettingValues.Contains("overrideruby"))
			{
				bOverrideRubyPath = (Settings.SettingValues["overrideruby"] == "true");
			}
		}
	}

	virtual FDocGenOutputFormatFactorySettings SaveSettings()
	{
		FDocGenOutputFormatFactorySettings Settings;
		if (bOverrideTemplatePath)
		{
			Settings.SettingValues.Add("overridetemplate", "true");
		}
		Settings.SettingValues.Add("template", TemplatePath.FilePath);
		
		if (bOverrideBinaryPath)
		{
			Settings.SettingValues.Add("overridebindir", "true");
		}
		Settings.SettingValues.Add("bindir", BinaryPath.Path);
		
		if (bOverrideRubyPath)
		{
			Settings.SettingValues.Add("overrideruby", "true");
		}
		Settings.SettingValues.Add("ruby", RubyPath.FilePath);
		
		Settings.FactoryClass = StaticClass();
		return Settings;
	};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bOverrideTemplatePath = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "bOverrideTemplatePath"))
	FFilePath TemplatePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bOverrideBinaryPath = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "bOverrideBinaryPath"))
	FDirectoryPath BinaryPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bOverrideRubyPath = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "bOverrideRubyPath"))
	FFilePath RubyPath;
};