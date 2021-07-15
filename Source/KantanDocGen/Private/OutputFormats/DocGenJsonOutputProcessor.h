#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "DocGenOutputProcessor.h"
#include "Engine/EngineTypes.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"

class DocGenJsonOutputProcessor : public IDocGenOutputProcessor
{
	FString Quote(const FString& In);
	TOptional<FString> GetObjectStringField(const TSharedPtr<class FJsonValue> Obj, const FString& FieldName);

	TOptional<FString> GetObjectStringField(const TSharedPtr<FJsonObject> Obj, const FString& FieldName);

	TOptional<TArray<FString>> GetNamesFromFileAtLocation(const FString& NameType, const FString& ClassFile);

	TSharedPtr<class FJsonObject> ParseNodeFile(const FString& NodeFilePath);

	TSharedPtr<FJsonObject> ParseStructFile(const FString& StructFilePath);
	TSharedPtr<FJsonObject> ParseEnumFile(const FString& EnumFilePath);
	void CopyJsonField(const FString& FieldName, TSharedPtr<FJsonObject> ParsedNode, TSharedPtr<FJsonObject> OutNode);
	TSharedPtr<FJsonObject> InitializeMainOutputFromIndex(TSharedPtr<FJsonObject> ParsedIndex);
	EIntermediateProcessingResult ConvertJsonToAdoc(FString IntermediateDir);
	EIntermediateProcessingResult ConvertAdocToHTML(FString IntermediateDir, FString OutputDir);
	FFilePath TemplatePath;
	FDirectoryPath BinaryPath;
	FFilePath RubyExecutablePath;

public:
	DocGenJsonOutputProcessor(TOptional<FFilePath> TemplatePathOverride, TOptional<FDirectoryPath> BinaryPathOverride,
							  TOptional<FFilePath> RubyExecutablePathOverride);
	virtual EIntermediateProcessingResult ProcessIntermediateDocs(FString const& IntermediateDir,
																  FString const& OutputDir, FString const& DocTitle,
																  bool bCleanOutput) override;

	EIntermediateProcessingResult ConsolidateClasses(TSharedPtr<FJsonObject> ParsedIndex,
													 FString const& IntermediateDir, FString const& OutputDir,
													 TSharedPtr<FJsonObject> ConsolidatedOutput);

	EIntermediateProcessingResult ConsolidateStructs(TSharedPtr<FJsonObject> ParsedIndex,
													 FString const& IntermediateDir, FString const& OutputDir,
													 TSharedPtr<FJsonObject> ConsolidatedOutput);
	
	EIntermediateProcessingResult ConsolidateEnums(TSharedPtr<FJsonObject> ParsedIndex,
													 FString const& IntermediateDir, FString const& OutputDir,
													 TSharedPtr<FJsonObject> ConsolidatedOutput);
	

	TOptional<TArray<FString>> GetNamesFromIndexFile(const FString& NameType, TSharedPtr<FJsonObject> ParsedIndex);

	TSharedPtr<FJsonObject> LoadFileToJson(FString const& FilePath);
};