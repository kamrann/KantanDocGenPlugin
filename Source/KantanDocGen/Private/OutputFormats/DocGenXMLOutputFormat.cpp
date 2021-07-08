#include "OutputFormats/DocGenXMLOutputFormat.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformProcess.h"

EIntermediateProcessingResult DocGenXMLOutputProcessor::ProcessIntermediateDocs(FString const& IntermediateDir,
																				FString const& OutputDir,
																				FString const& DocTitle,
																				bool bCleanOutput)
{
	auto& PluginManager = IPluginManager::Get();
	auto Plugin = PluginManager.FindPlugin(TEXT("KantanDocGen"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogKantanDocGen, Error, TEXT("Failed to locate plugin info"));
		return EIntermediateProcessingResult::UnknownError;
	}

	const FString DocGenToolBinPath =
		Plugin->GetBaseDir() / TEXT("ThirdParty") / TEXT("KantanDocGenTool") / TEXT("bin");
	const FString DocGenToolExeName = TEXT("KantanDocGen.exe");
	const FString DocGenToolPath = DocGenToolBinPath / DocGenToolExeName;

	// Create a read and write pipe for the child process
	void* PipeRead = nullptr;
	void* PipeWrite = nullptr;
	verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));

	FString Args = FString(TEXT("-outputdir=")) + TEXT("\"") + OutputDir + TEXT("\"") +
				   TEXT(" -fromintermediate -intermediatedir=") + TEXT("\"") + IntermediateDir + TEXT("\"") +
				   TEXT(" -name=") + DocTitle + (bCleanOutput ? TEXT(" -cleanoutput") : TEXT(""));
	UE_LOG(LogKantanDocGen, Log, TEXT("Invoking conversion tool: %s %s"), *DocGenToolPath, *Args);
	FProcHandle Proc =
		FPlatformProcess::CreateProc(*DocGenToolPath, *Args, true, false, false, nullptr, 0, nullptr, PipeWrite);

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

		// FPlatformProcess::WaitForProc(Proc);
		// FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode);
		FPlatformProcess::CloseProc(Proc);
		Proc.Reset();

		if (ReturnCode != 0)
		{
			UE_LOG(LogKantanDocGen, Error, TEXT("KantanDocGen tool failed (code %i), see above output."), ReturnCode);
		}
	}

	// Close the pipes
	FPlatformProcess::ClosePipe(0, PipeRead);
	FPlatformProcess::ClosePipe(0, PipeWrite);

	switch (ReturnCode)
	{
		case 0:
			return EIntermediateProcessingResult::Success;
		case -1:
			return EIntermediateProcessingResult::UnknownError;
		case -2:
			return EIntermediateProcessingResult::DiskWriteFailure;
		default:
			return EIntermediateProcessingResult::SuccessWithErrors;
	}
}
