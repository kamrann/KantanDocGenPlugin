#include "OutputFormats/DocGenJsonOutputProcessor.h"
#include "Algo/Transform.h"
#include "HAL/FileManager.h"

// To define the UE_5_0_OR_LATER below
#include "Misc/EngineVersionComparison.h"
#if UE_VERSION_NEWER_THAN(5, 0, 0)
#include "HAL/PlatformFileManager.h"
#else
#include "HAL/PlatformFilemanager.h"
#endif

#include "Json.h"
#include "JsonDomBuilder.h"
#include "KantanDocGenLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Optional.h"
#include "Misc/Paths.h"

FString DocGenJsonOutputProcessor::Quote(const FString& In)
{
	if (In.TrimStartAndEnd().StartsWith("\""))
	{
		return In;
	}
	return "\"" + In.TrimStartAndEnd() + "\"";
}

TOptional<FString> DocGenJsonOutputProcessor::GetObjectStringField(const TSharedPtr<FJsonObject> Obj,
																   const FString& FieldName)
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

TOptional<FString> DocGenJsonOutputProcessor::GetObjectStringField(const TSharedPtr<FJsonValue> Obj,
																   const FString& FieldName)
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

TOptional<TArray<FString>> DocGenJsonOutputProcessor::GetNamesFromFileAtLocation(const FString& NameType,
																				 const FString& ClassFile)
{
	TSharedPtr<FJsonObject> ParsedClass = LoadFileToJson(ClassFile);
	if (!ParsedClass)
	{
		return {};
	}

	if (ParsedClass->HasTypedField<EJson::Array>(NameType))
	{
		TArray<FString> NodeNames;
		for (const auto& Value : ParsedClass->GetArrayField(NameType))
		{
			TOptional<FString> FuncID = GetObjectStringField(Value, "id");
			if (FuncID.IsSet())
			{
				NodeNames.Add(FuncID.GetValue());
			}
		}
		return NodeNames;
	}
	else if (ParsedClass->HasTypedField<EJson::Object>(NameType))
	{
		TArray<FString> NodeNames;
		for (const auto& Node : ParsedClass->GetObjectField(NameType)->Values)
		{
			TOptional<FString> Name = GetObjectStringField(Node.Value, "id");
			if (Name.IsSet())
			{
				NodeNames.Add(Name.GetValue());
			}
		}
		return NodeNames;
	}
	else if (ParsedClass->HasTypedField<EJson::Null>(NameType))
	{
		return TArray<FString>();
	}
	return {};
}

TSharedPtr<FJsonObject> DocGenJsonOutputProcessor::ParseNodeFile(const FString& NodeFilePath)
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
	CopyJsonField("autocast", ParsedNode, OutNode);
	CopyJsonField("funcname", ParsedNode, OutNode);
	return OutNode;
}
TSharedPtr<FJsonObject> DocGenJsonOutputProcessor::ParseStructFile(const FString& StructFilePath)
{
	TSharedPtr<FJsonObject> ParsedStruct = LoadFileToJson(StructFilePath);
	if (!ParsedStruct)
	{
		return {};
	}

	TSharedPtr<FJsonObject> OutNode = MakeShared<FJsonObject>();
	// Reusing the class template for now so renaming id to class_id to be consistent
	if (TSharedPtr<FJsonValue> Field = ParsedStruct->TryGetField("id"))
	{
		OutNode->SetField("class_id", Field);
	}

	CopyJsonField("doxygen", ParsedStruct, OutNode);
	CopyJsonField("display_name", ParsedStruct, OutNode);
	CopyJsonField("fields", ParsedStruct, OutNode);
	return OutNode;
}

TSharedPtr<FJsonObject> DocGenJsonOutputProcessor::ParseEnumFile(const FString& EnumFilePath)
{
	TSharedPtr<FJsonObject> ParsedEnum = LoadFileToJson(EnumFilePath);
	if (!ParsedEnum)
	{
		return {};
	}

	TSharedPtr<FJsonObject> OutNode = MakeShared<FJsonObject>();

	CopyJsonField("id", ParsedEnum, OutNode);
	CopyJsonField("doxygen", ParsedEnum, OutNode);
	CopyJsonField("display_name", ParsedEnum, OutNode);
	CopyJsonField("values", ParsedEnum, OutNode);

	return OutNode;
}

void DocGenJsonOutputProcessor::CopyJsonField(const FString& FieldName, TSharedPtr<FJsonObject> ParsedNode,
											  TSharedPtr<FJsonObject> OutNode)
{
	if (TSharedPtr<FJsonValue> Field = ParsedNode->TryGetField(FieldName))
	{
		OutNode->SetField(FieldName, Field);
	}
}

TSharedPtr<FJsonObject> DocGenJsonOutputProcessor::InitializeMainOutputFromIndex(TSharedPtr<FJsonObject> ParsedIndex)
{
	TSharedPtr<FJsonObject> Output = MakeShared<FJsonObject>();

	CopyJsonField("display_name", ParsedIndex, Output);
	return Output;
}

EIntermediateProcessingResult DocGenJsonOutputProcessor::ConvertJsonToAdoc(FString IntermediateDir)
{
	const FFilePath InJsonPath {IntermediateDir / "consolidated.json"};
	const FFilePath OutAdocPath {IntermediateDir / "docs.adoc"};

	void* PipeRead = nullptr;
	void* PipeWrite = nullptr;
	verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));

	const FString Args =
		Quote(TemplatePath.FilePath) + " " + Quote(InJsonPath.FilePath) + " " + Quote(OutAdocPath.FilePath);

	FProcHandle Proc = FPlatformProcess::CreateProc(*(BinaryPath.Path / "convert.exe"), *Args, true, false, false,
													nullptr, 0, nullptr, PipeWrite);

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

				UE_LOG(LogKantanDocGen, Error, TEXT("[KantanDocGen] %s"), *Line);

				BufferedText = BufferedText.Mid(EndOfLineIdx + 1);
			}

			FPlatformProcess::Sleep(0.1f);
		}
		FPlatformProcess::CloseProc(Proc);
		Proc.Reset();

		if (ReturnCode != 0)
		{
			UE_LOG(LogKantanDocGen, Error, TEXT("KantanDocGen tool failed (code %i), see above output."), ReturnCode);
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

EIntermediateProcessingResult DocGenJsonOutputProcessor::ConvertAdocToHTML(FString IntermediateDir, FString OutputDir)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectory(*(OutputDir / "img"));
	PlatformFile.CopyDirectoryTree(*(OutputDir / "img"), *(BinaryPath.Path / "img"), true);

	const FFilePath InAdocPath {IntermediateDir / "docs.adoc"};
	const FFilePath OutHTMLPath {OutputDir / "documentation.html"};
	void* PipeRead = nullptr;
	void* PipeWrite = nullptr;
	verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));

	const FString Args = Quote(BinaryPath.Path / "scripts" / "render_html.rb") + " " + Quote(InAdocPath.FilePath) +
						 " " + Quote(OutHTMLPath.FilePath);

	FProcHandle Proc = FPlatformProcess::CreateProc(*(RubyExecutablePath.FilePath), *Args, true, false, false, nullptr,
													0, *(BinaryPath.Path / "scripts"), PipeWrite);

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
			UE_LOG(LogKantanDocGen, Error, TEXT("KantanDocGen tool failed (code %i), see above output."), ReturnCode);
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

DocGenJsonOutputProcessor::DocGenJsonOutputProcessor(TOptional<FFilePath> TemplatePathOverride,
													 TOptional<FDirectoryPath> BinaryPathOverride,
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

EIntermediateProcessingResult DocGenJsonOutputProcessor::ProcessIntermediateDocs(FString const& IntermediateDir,
																				 FString const& OutputDir,
																				 FString const& DocTitle,
																				 bool bCleanOutput)
{
	TSharedPtr<FJsonObject> ParsedIndex = LoadFileToJson(IntermediateDir / "index.json");

	TSharedPtr<FJsonObject> ConsolidatedOutput = InitializeMainOutputFromIndex(ParsedIndex);

	EIntermediateProcessingResult ClassResult =
		ConsolidateClasses(ParsedIndex, IntermediateDir, OutputDir, ConsolidatedOutput);
	if (ClassResult != EIntermediateProcessingResult::Success)
	{
		return ClassResult;
	}

	EIntermediateProcessingResult StructResult =
		ConsolidateStructs(ParsedIndex, IntermediateDir, OutputDir, ConsolidatedOutput);
	if (StructResult != EIntermediateProcessingResult::Success)
	{
		return StructResult;
	}

	EIntermediateProcessingResult EnumResult =
		ConsolidateEnums(ParsedIndex, IntermediateDir, OutputDir, ConsolidatedOutput);
	if (EnumResult != EIntermediateProcessingResult::Success)
	{
		return EnumResult;
	}

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

EIntermediateProcessingResult DocGenJsonOutputProcessor::ConsolidateClasses(TSharedPtr<FJsonObject> ParsedIndex,
																			FString const& IntermediateDir,
																			FString const& OutputDir,
																			TSharedPtr<FJsonObject> ConsolidatedOutput)
{
	FJsonDomBuilder::FArray StaticFunctionList;
	FJsonDomBuilder::FObject ClassFunctionList;
	TOptional<TArray<FString>> ClassNames = GetNamesFromIndexFile("classes", ParsedIndex);
	if (!ClassNames.IsSet())
	{
		return EIntermediateProcessingResult::UnknownError;
	}

	for (const auto& ClassName : ClassNames.GetValue())
	{
		const FString ClassFilePath = IntermediateDir / ClassName / ClassName + ".json";
		TOptional<TArray<FString>> NodeNames = GetNamesFromFileAtLocation("nodes", ClassFilePath);
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
					FString RelImagePath;
					if (NodeJson->TryGetStringField("imgpath", RelImagePath))
					{
						FString SourceImagePath = IntermediateDir / ClassName / "nodes" / RelImagePath;
						SourceImagePath =
							IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*SourceImagePath);
						IFileManager::Get().Copy(*(OutputDir / "img" / FPaths::GetCleanFilename(RelImagePath)),
												 *SourceImagePath, true);
					}
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
			// We don't want classes in our classlist if all their nodes are static
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
	return EIntermediateProcessingResult::Success;
}

EIntermediateProcessingResult DocGenJsonOutputProcessor::ConsolidateStructs(TSharedPtr<FJsonObject> ParsedIndex,
																			FString const& IntermediateDir,
																			FString const& OutputDir,
																			TSharedPtr<FJsonObject> ConsolidatedOutput)
{
	FJsonDomBuilder::FArray StructList;

	TOptional<TArray<FString>> StructNames = GetNamesFromIndexFile("structs", ParsedIndex);
	if (!StructNames.IsSet())
	{
		return EIntermediateProcessingResult::UnknownError;
	}

	for (const auto& StructName : StructNames.GetValue())
	{
		const FString StructFilePath = IntermediateDir / StructName / StructName + ".json";
		TSharedPtr<FJsonObject> StructJson = ParseStructFile(StructFilePath);
		StructList.Add(MakeShared<FJsonValueObject>(StructJson));
	}

	ConsolidatedOutput->SetField("structs", StructList.AsJsonValue());
	return EIntermediateProcessingResult::Success;
}

EIntermediateProcessingResult DocGenJsonOutputProcessor::ConsolidateEnums(TSharedPtr<FJsonObject> ParsedIndex,
																		  FString const& IntermediateDir,
																		  FString const& OutputDir,
																		  TSharedPtr<FJsonObject> ConsolidatedOutput)
{
	FJsonDomBuilder::FArray EnumList;

	TOptional<TArray<FString>> EnumNames = GetNamesFromIndexFile("enums", ParsedIndex);
	if (!EnumNames.IsSet())
	{
		return EIntermediateProcessingResult::UnknownError;
	}

	for (const auto& EnumName : EnumNames.GetValue())
	{
		const FString EnumFilePath = IntermediateDir / EnumName / EnumName + ".json";
		TSharedPtr<FJsonObject> EnumJson = ParseEnumFile(EnumFilePath);
		EnumList.Add(MakeShared<FJsonValueObject>(EnumJson));
	}

	ConsolidatedOutput->SetField("enums", EnumList.AsJsonValue());
	return EIntermediateProcessingResult::Success;
}

TOptional<TArray<FString>> DocGenJsonOutputProcessor::GetNamesFromIndexFile(const FString& NameType,
																			TSharedPtr<FJsonObject> ParsedIndex)
{
	if (!ParsedIndex)
	{
		return {};
	}

	const TArray<TSharedPtr<FJsonValue>>* ClassEntries;
	if (!ParsedIndex->TryGetArrayField(NameType, ClassEntries))
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

TSharedPtr<FJsonObject> DocGenJsonOutputProcessor::LoadFileToJson(FString const& FilePath)
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
