#include "OutputFormats/DocGenJsonOutputFormat.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "OutputFormats/DocGenJsonOutputProcessor.h"
#include "OutputFormats/DocGenOutputProcessor.h"

FString DocGenJsonSerializer::EscapeString(const FString& InString)
{
	return InString;
}

FString DocGenJsonSerializer::GetFileExtension()
{
	return ".json";
}

void DocGenJsonSerializer::SerializeObject(const DocTreeNode::Object& Obj)
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

TSharedPtr<FJsonValueArray> DocGenJsonSerializer::SerializeArray(const TArray<TSharedPtr<DocTreeNode>> ArrayElements)
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

void DocGenJsonSerializer::SerializeString(const FString& InString)
{
	TargetObject = MakeShared<FJsonValueString>(InString);
}

void DocGenJsonSerializer::SerializeNull()
{
	TargetObject = MakeShared<FJsonValueNull>();
}

DocGenJsonSerializer::DocGenJsonSerializer()
	: TopLevelObject(MakeShared<FJsonValueObject>(MakeShared<FJsonObject>())),
	  TargetObject(TopLevelObject)
{}

DocGenJsonSerializer::DocGenJsonSerializer(TSharedPtr<FJsonValue>& TargetObject) : TargetObject(TargetObject) {}

bool DocGenJsonSerializer::SaveToFile(const FString& OutFileDirectory, const FString& OutFileName)
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
}

TSharedPtr<struct DocTreeNode::IDocTreeSerializer> UDocGenJsonOutputFactory::CreateSerializer()
{
	return MakeShared<DocGenJsonSerializer>();
}

TSharedPtr<struct IDocGenOutputProcessor> UDocGenJsonOutputFactory::CreateIntermediateDocProcessor()
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

FString UDocGenJsonOutputFactory::GetFormatIdentifier()
{
	return "json";
}

void UDocGenJsonOutputFactory::LoadSettings(const FDocGenOutputFormatFactorySettings& Settings)
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

FDocGenOutputFormatFactorySettings UDocGenJsonOutputFactory::SaveSettings()
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
}
