#pragma once

#include "Algo/Transform.h"
#include "Containers/UnrealString.h"
#include "DocGenOutputProcessor.h"
#include "HAL/PlatformFilemanager.h"
#include "JsonDomBuilder.h"
#include "KantanDocGenLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Optional.h"
#include "Serialization/JsonSerializer.h"

class DocGenJsonOutputProcessor : public IDocGenOutputProcessor
{
	FString Quote(const FString& In)
	{
		if (In.TrimStartAndEnd().StartsWith("\""))
		{
			return In;
		}
		return "\"" + In.TrimStartAndEnd() + "\"";
	}
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
		CopyJsonField("funcname", ParsedNode, OutNode);
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
	EIntermediateProcessingResult ConvertJsonToAdoc(FString IntermediateDir)
	{
		const FFilePath InJsonPath {IntermediateDir / "consolidated.json"};
		const FFilePath OutAdocPath {IntermediateDir / "docs.adoc"};

		void* PipeRead = nullptr;
		void* PipeWrite = nullptr;
		verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));

		const FString Args =
			Quote(TemplatePath.FilePath) + " " + Quote(InJsonPath.FilePath) + " " + Quote(OutAdocPath.FilePath);

		FProcHandle Proc = FPlatformProcess::CreateProc(*(BinaryPath.Path / "convert.exe"), *Args, true, false,
														false, nullptr, 0, nullptr, PipeWrite);

		int32 ReturnCode = 0;
		if (Proc.IsValid())
		{
			FString BufferedText;
			for (bool bProcessFinished = false; !bProcessFinished;)
			{
				bProcessFinished = FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode);

				/*			if(!bProcessFinished && Warn->ReceivedUserCancel())
				{
				FPlatformProcess::TerminateProc(ProcessHandle);
				bProcessFinished = true;
				}
				*/
				BufferedText += FPlatformProcess::ReadPipe(PipeRead);

				int32 EndOfLineIdx;
				while (BufferedText.FindChar('\n', EndOfLineIdx))
				{
					FString Line = BufferedText.Left(EndOfLineIdx);
					Line.RemoveFromEnd(TEXT("\r"));

					UE_LOG(LogKantanDocGen, Log, TEXT("[KantanDocGen] %s"), *Line);

					BufferedText = BufferedText.Mid(EndOfLineIdx + 1);
				}

				FPlatformProcess::Sleep(0.1f);
			}
			FPlatformProcess::CloseProc(Proc);
			Proc.Reset();

			if (ReturnCode != 0)
			{
				UE_LOG(LogKantanDocGen, Error, TEXT("KantanDocGen tool failed (code %i), see above output."),
					   ReturnCode);
				return EIntermediateProcessingResult::UnknownError;
			}
			else
			{
				return EIntermediateProcessingResult::Success;
			}
		}
		else
		{
			return EIntermediateProcessingResult::UnknownError;
		}
	}
	EIntermediateProcessingResult ConvertAdocToHTML(FString IntermediateDir, FString OutputDir)
	{
		const FFilePath InAdocPath {IntermediateDir / "docs.adoc"};
		const FFilePath OutHTMLPath {OutputDir / "documentation.html"};
		void* PipeRead = nullptr;
		void* PipeWrite = nullptr;
		verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));

		const FString Args = Quote(BinaryPath.Path / "scripts" / "render_html.rb") + " " + Quote(InAdocPath.FilePath) +
							 " " + Quote(OutHTMLPath.FilePath);

		FProcHandle Proc = FPlatformProcess::CreateProc(*(RubyExecutablePath.FilePath), *Args, true, false, false,
														nullptr, 0, *(BinaryPath.Path / "scripts"), PipeWrite);

		int32 ReturnCode = 0;
		if (Proc.IsValid())
		{
			FString BufferedText;
			for (bool bProcessFinished = false; !bProcessFinished;)
			{
				bProcessFinished = FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode);

				/*			if(!bProcessFinished && Warn->ReceivedUserCancel())
				{
				FPlatformProcess::TerminateProc(ProcessHandle);
				bProcessFinished = true;
				}
				*/
				BufferedText += FPlatformProcess::ReadPipe(PipeRead);

				int32 EndOfLineIdx;
				while (BufferedText.FindChar('\n', EndOfLineIdx))
				{
					FString Line = BufferedText.Left(EndOfLineIdx);
					Line.RemoveFromEnd(TEXT("\r"));

					UE_LOG(LogKantanDocGen, Log, TEXT("[KantanDocGen] %s"), *Line);

					BufferedText = BufferedText.Mid(EndOfLineIdx + 1);
				}

				FPlatformProcess::Sleep(0.1f);
			}
			FPlatformProcess::CloseProc(Proc);
			Proc.Reset();

			if (ReturnCode != 0)
			{
				UE_LOG(LogKantanDocGen, Error, TEXT("KantanDocGen tool failed (code %i), see above output."),
					   ReturnCode);
				return EIntermediateProcessingResult::UnknownError;
			}
			else
			{
				return EIntermediateProcessingResult::Success;
			}
		}
		else
		{
			return EIntermediateProcessingResult::UnknownError;
		}
	}
	FFilePath TemplatePath;
	FDirectoryPath BinaryPath;
	FFilePath RubyExecutablePath;

public:
	DocGenJsonOutputProcessor(TOptional<FFilePath> TemplatePathOverride, TOptional<FDirectoryPath> BinaryPathOverride,
							  TOptional<FFilePath> RubyExecutablePathOverride)
	{
		if (BinaryPathOverride.IsSet())
		{
			BinaryPath = BinaryPathOverride.GetValue();
		}
		else
		{
			BinaryPath.Path = FString("bin");
		}

		if (TemplatePathOverride.IsSet())
		{
			TemplatePath = TemplatePathOverride.GetValue();
		}
		else
		{
			TemplatePath.FilePath = BinaryPath.Path / "template" / "docs.adoc.in";
		}

		if (RubyExecutablePathOverride.IsSet())
		{
			RubyExecutablePath = RubyExecutablePathOverride.GetValue();
		}
		else
		{
			RubyExecutablePath.FilePath = "ruby.exe";
		}
	}
	virtual EIntermediateProcessingResult ProcessIntermediateDocs(FString const& IntermediateDir,
																  FString const& OutputDir, FString const& DocTitle,
																  bool bCleanOutput) override
	{
		FJsonDomBuilder::FArray StaticFunctionList;
		FJsonDomBuilder::FObject ClassFunctionList;

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
				FJsonDomBuilder::FArray Nodes;
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
						}
						else
						{
							Nodes.Add(MakeShared<FJsonValueObject>(NodeJson));
						}
					}
					else
					{
						return EIntermediateProcessingResult::UnknownError;
					}
				}
				//We don't want classes in our classlist if all their nodes are static
				if (Nodes.Num())
				{
					FJsonDomBuilder::FObject ClassObj;
					ClassObj.Set("functions", Nodes);
					ClassObj.Set("class_id", ClassName);
					ClassFunctionList.Set(ClassName, ClassObj);
				}
			}
		}

		ConsolidatedOutput->SetField("functions", StaticFunctionList.AsJsonValue());
		ConsolidatedOutput->SetField("classes", ClassFunctionList.AsJsonValue());

		FString Result;
		auto JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Result);
		FJsonSerializer::Serialize(ConsolidatedOutput.ToSharedRef(), JsonWriter);

		if (!FFileHelper::SaveStringToFile(Result, *(IntermediateDir / "consolidated.json"),
										   FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return EIntermediateProcessingResult::DiskWriteFailure;
		}
		if (ConvertJsonToAdoc(IntermediateDir) == EIntermediateProcessingResult::Success)
		{
			return ConvertAdocToHTML(IntermediateDir, OutputDir);
		}
		return EIntermediateProcessingResult::UnknownError;
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