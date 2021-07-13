#pragma once
#include "OutputFormats/DocGenOutputProcessor.h"

class DocGenXMLOutputProcessor : public IDocGenOutputProcessor
{
public:
	virtual EIntermediateProcessingResult ProcessIntermediateDocs(FString const& IntermediateDir,
																  FString const& OutputDir, FString const& DocTitle,
																  bool bCleanOutput) override;
};