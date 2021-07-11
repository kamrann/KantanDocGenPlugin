#pragma once
#include "CoreMinimal.h"
#include "Containers/UnrealString.h"

enum EIntermediateProcessingResult : uint8
{
	Success,
	SuccessWithErrors,
	UnknownError,
	DiskWriteFailure,
};

struct IDocGenOutputProcessor
{
	virtual ~IDocGenOutputProcessor() {};
	virtual EIntermediateProcessingResult ProcessIntermediateDocs(FString const& IntermediateDir,
																  FString const& OutputDir, FString const& DocTitle,
																  bool bCleanOutput) = 0;
};