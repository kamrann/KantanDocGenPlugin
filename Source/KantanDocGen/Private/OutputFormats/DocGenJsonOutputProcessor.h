#pragma once

#include "Algo/Transform.h"
#include "Containers/UnrealString.h"
#include "DocGenOutputProcessor.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Optional.h"
#include "Serialization/JsonSerializer.h"

class DocGenJsonOutputProcessor : public IDocGenOutputProcessor
{
	TOptional<FString> GetObjectStringField(const TSharedPtr<FJsonValue> Obj, const FString& FieldName)
	{
		const TSharedPtr<FJsonObject>* UnderlyingObject = nullptr;
		if (!Obj->TryGetObject(UnderlyingObject))
		{
			return {};
		}
		else
		{
			return GetObjectStringField(*UnderlyingObject, FieldName);
		}
	}

	TOptional<FString> GetObjectStringField(const TSharedPtr<FJsonObject> Obj, const FString& FieldName)
	{
		FString FieldValue;
		if (!Obj->TryGetStringField(FieldName, FieldValue))
		{
			return {};
		}
		else
		{
			return FieldValue;
		}
	}

	TOptional<TArray<FString>> GetNodeNamesFromClassFile(const FString& ClassFile)
	{
		TSharedPtr<FJsonObject> ParsedClass = LoadFileToJson(ClassFile);
		if (!ParsedClass)
		{
			return {};
		}

		if (ParsedClass->HasTypedField<EJson::Array>("nodes"))
		{
			TArray<FString> NodeNames;
			for (const auto& Value : ParsedClass->GetArrayField("nodes"))
			{
				TOptional<FString> FuncID = GetObjectStringField(Value, "id");
				if (FuncID.IsSet())
				{
					NodeNames.Add(FuncID.GetValue());
				}
			}
			return NodeNames;
		}
		else if (ParsedClass->HasTypedField<EJson::Object>("nodes"))
		{
			TArray<FString> NodeNames;
			for (const auto& Node : ParsedClass->GetObjectField("nodes")->Values)
			{
				TOptional<FString> Name = GetObjectStringField(Node.Value, "id");
				if (Name.IsSet())
				{
					NodeNames.Add(Name.GetValue());
				}
			}
			return NodeNames;
		}
		return {};
	}

	TSharedPtr<FJsonObject> ParseNodeFile(const FString& NodeFilePath)
	{
		TSharedPtr<FJsonObject> ParsedNode = LoadFileToJson(NodeFilePath);
		if (!ParsedNode)
		{
			return {};
		}

		TSharedPtr<FJsonObject> OutNode = MakeShared<FJsonObject>();

		CopyJsonField("inputs", ParsedNode, OutNode);
		CopyJsonField("outputs", ParsedNode, OutNode);
		CopyJsonField("rawsignature", ParsedNode, OutNode);
		CopyJsonField("class_id", ParsedNode, OutNode);
		CopyJsonField("doxygen", ParsedNode, OutNode);
		CopyJsonField("imgpath", ParsedNode, OutNode);
		CopyJsonField("shorttitle", ParsedNode, OutNode);
		CopyJsonField("fulltitle", ParsedNode, OutNode);
		CopyJsonField("static", ParsedNode, OutNode);

		return OutNode;
	}

	void CopyJsonField(const FString& FieldName, TSharedPtr<FJsonObject> ParsedNode, TSharedPtr<FJsonObject> OutNode)
	{
		if (TSharedPtr<FJsonValue> Field = ParsedNode->TryGetField(FieldName))
		{
			OutNode->SetField(FieldName, Field);
		}
	}
	TSharedPtr<FJsonObject> InitializeMainOutputFromIndex(TSharedPtr<FJsonObject> ParsedIndex)
	{
		TSharedPtr<FJsonObject> Output = MakeShared<FJsonObject>();

		CopyJsonField("display_name", ParsedIndex, Output);
		return Output;
	}

public:
	virtual EIntermediateProcessingResult ProcessIntermediateDocs(FString const& IntermediateDir,
																  FString const& OutputDir, FString const& DocTitle,
																  bool bCleanOutput) override
	{
		TArray<TSharedPtr<FJsonValue>> StaticFunctionList;
		TMap<FString, TSharedPtr<FJsonValue>> ClassFunctionList;
		TSharedPtr<FJsonObject> ParsedIndex = LoadFileToJson(IntermediateDir / "index.json");

		TSharedPtr<FJsonObject> ConsolidatedOutput = InitializeMainOutputFromIndex(ParsedIndex);

		TOptional<TArray<FString>> ClassNames = GetClassNamesFromIndexFile(ParsedIndex);
		if (!ClassNames.IsSet())
		{
			return EIntermediateProcessingResult::UnknownError;
		}

		for (const auto& ClassName : ClassNames.GetValue())
		{
			const FString ClassFilePath = IntermediateDir / ClassName / ClassName + ".json";
			TOptional<TArray<FString>> NodeNames = GetNodeNamesFromClassFile(ClassFilePath);
			if (!NodeNames.IsSet())
			{
				return EIntermediateProcessingResult::UnknownError;
			}
			else
			{
				TOptional<FString> NodeClassID;
				TArray<TSharedPtr<FJsonValue>> Nodes;
				for (const auto& NodeName : NodeNames.GetValue())
				{
					const FString NodeFilePath = IntermediateDir / ClassName / "nodes" / NodeName + ".json";

					if (TSharedPtr<FJsonObject> NodeJson = ParseNodeFile(NodeFilePath))
					{
						bool FunctionIsStatic = false;
						NodeJson->TryGetBoolField("static", FunctionIsStatic);

						if (FunctionIsStatic)
						{
							StaticFunctionList.Add(MakeShared<FJsonValueObject>(NodeJson));
							// ConsolidatedOutput->GetArrayField("functions").Add(NodeJson);
						}
						else
						{
							Nodes.Add(MakeShared<FJsonValueObject>(NodeJson));
							NodeClassID = GetObjectStringField(NodeJson, "class_id");
						}
					}
					else
					{
						return EIntermediateProcessingResult::UnknownError;
					}
				}

				if (NodeClassID.IsSet())
				{
					ClassFunctionList.Add(NodeClassID.GetValue(),MakeShared<FJsonValueArray>(Nodes));
				}
			}
		}

		ConsolidatedOutput->SetArrayField("functions", StaticFunctionList);
		auto ClassList = MakeShared<FJsonObject>();
		ClassList->Values = ClassFunctionList;
		ConsolidatedOutput->SetObjectField("classes", ClassList);

		FString Result;
		auto JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Result);
		FJsonSerializer::Serialize(ConsolidatedOutput.ToSharedRef(), JsonWriter);

		if (!FFileHelper::SaveStringToFile(Result, *(IntermediateDir / "consolidated.json"),
										   FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return EIntermediateProcessingResult::DiskWriteFailure;
		}

		return EIntermediateProcessingResult::Success;
	}

	TOptional<TArray<FString>> GetClassNamesFromIndexFile(TSharedPtr<FJsonObject> ParsedIndex)
	{
		if (!ParsedIndex)
		{
			return {};
		}

		const TArray<TSharedPtr<FJsonValue>>* ClassEntries;
		if (!ParsedIndex->TryGetArrayField("classes", ClassEntries))
		{
			return {};
		}
		TArray<FString> ClassJsonFiles;
		for (const auto& ClassEntry : *ClassEntries)
		{
			TOptional<FString> ClassID = GetObjectStringField(ClassEntry, "id");
			if (ClassID.IsSet())
			{
				ClassJsonFiles.Add(ClassID.GetValue());
			}
		}
		if (ClassJsonFiles.Num())
		{
			return ClassJsonFiles;
		}
		else
		{
			return {};
		}
	}

	TSharedPtr<FJsonObject> LoadFileToJson(FString const& FilePath)
	{
		FString IndexFileString;
		if (!FFileHelper::LoadFileToString(IndexFileString, &FPlatformFileManager::Get().GetPlatformFile(), *FilePath))
		{
			return nullptr;
		}

		TSharedPtr<FJsonStringReader> TopLevelJson = FJsonStringReader::Create(IndexFileString);
		TSharedPtr<FJsonObject> ParsedFile;
		if (!FJsonSerializer::Deserialize<TCHAR>(*TopLevelJson, ParsedFile, FJsonSerializer::EFlags::None))
		{
			return nullptr;
		}
		else
		{
			return ParsedFile;
		}
	}
};