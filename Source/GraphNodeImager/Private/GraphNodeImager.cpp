// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#include "GraphNodeImager.h"
#include "GraphNodeImagerModule.h"
#include "GraphNodeImagerCommands.h"
#include "IConsoleManager.h"
#include "IMainFrameModule.h"
#include "NodeDocsGenerator.h"

#define LOCTEXT_NAMESPACE "GraphNodeImager"


IMPLEMENT_MODULE(FGraphNodeImagerModule, GraphNodeImager)

DEFINE_LOG_CATEGORY(LogGraphNodeImager);


void FGraphNodeImagerModule::StartupModule()
{
	FGraphNodeImagerCommands::Register();
}

void FGraphNodeImagerModule::ShutdownModule()
{
	FGraphNodeImagerCommands::Unregister();
}

// @TODO: Idea was to allow quoted values containing spaces, but this isn't possible since the initial console string has
// already been split by whitespace, ignoring quotes...
inline bool MatchPotentiallyQuoted(const TCHAR* Stream, const TCHAR* Match, FString& Value)
{
	while((*Stream == ' ') || (*Stream == 9))
		Stream++;

	if(FCString::Strnicmp(Stream, Match, FCString::Strlen(Match)) == 0)
	{
		Stream += FCString::Strlen(Match);

		return FParse::Token(Stream, Value, false);
	}
	
	return false;
}

void FGraphNodeImagerModule::DumpNodes(TArray< FString > const& Args)
{
	FString OutputDir;
	FString BlueprintClassName;
	FString ClassesString;
	FString PackagesString;
	FString CategoriesString;
	FString DocTitle;
	bool bCleanOutput = false;

	FString IntermediateDir = FPaths::GameIntermediateDir() / TEXT("KantanDocGen");

	for(FString Arg : Args)
	{
		MatchPotentiallyQuoted(*Arg, TEXT("outdir="), OutputDir);
		FParse::Value(*Arg, TEXT("blueprintclass="), BlueprintClassName);
		FParse::Value(*Arg, TEXT("package="), PackagesString, false);
		FParse::Value(*Arg, TEXT("class="), ClassesString, false);
		FParse::Value(*Arg, TEXT("category="), CategoriesString, false);
		MatchPotentiallyQuoted(*Arg, TEXT("title="), DocTitle);
		bCleanOutput = bCleanOutput || (Arg == TEXT("clean"));
	}

	// Must have an output path
	if(!FPaths::ValidatePath(OutputDir))
	{
		UE_LOG(LogGraphNodeImager, Error, TEXT("Invalid output directory (outdir=), aborting."));
		return;
	}

	// Must have a title
	if(DocTitle.IsEmpty())
	{
		UE_LOG(LogGraphNodeImager, Error, TEXT("No documentation title specified (title=), aborting."));
		return;
	}

	auto BlueprintClass = FindObject< UClass >(ANY_PACKAGE, *BlueprintClassName);
	if(BlueprintClass == nullptr)
	{
		BlueprintClass = AActor::StaticClass();
		if(!BlueprintClassName.IsEmpty())
		{
			UE_LOG(LogGraphNodeImager, Warning, TEXT("Blueprint class '%s' not found, defaulting to Actor."), *BlueprintClassName);
		}
	}

	const TCHAR* ListDelims[] = {
		TEXT(","),
		TEXT("|")
	};

	int32 const NumDelims = sizeof(ListDelims) / sizeof(ListDelims[0]);

	// Store optional category filters
	TArray< FString > Categories;
	CategoriesString.ParseIntoArray(Categories, ListDelims, NumDelims);

	// Enumeration function
	typedef TFunction< int32(UObject*) > FObjectProcessFunc;

	TFunction< int32(FObjectProcessFunc&) > BatchProcessFunc;

	// Do we have specific classes?
	if(!ClassesString.IsEmpty())
	{
		TArray< FString > ClassNames;
		ClassesString.ParseIntoArray(ClassNames, ListDelims, NumDelims);

		BatchProcessFunc = [ClassNames](FObjectProcessFunc& ObjectProcessFunc) -> int32
		{
			int32 Result = 0;
			for(auto const& Name : ClassNames)
			{
				UObject* Object = nullptr;
				
				// Native classes don't need loading
				if(auto Class = FindObject< UClass >(ANY_PACKAGE, *Name))
				{
					Object = Class;
				}
				else if(auto Blueprint = FindObject< UBlueprint >(ANY_PACKAGE, *Name))
				{
					Object = Blueprint;
				}
				// For blueprints though, it may be unloaded
				else if(auto Blueprint = LoadObject< UBlueprint >(ANY_PACKAGE, *Name))
				{
					Object = Blueprint;
				}

				if(Object == nullptr)
				{
					UE_LOG(LogGraphNodeImager, Warning, TEXT("Failed to find source class '%s', skipping."), *Name);
					continue;
				}

				Result += ObjectProcessFunc(Object);
			}

			return Result;
		};
		
		if(!PackagesString.IsEmpty())
		{
			UE_LOG(LogGraphNodeImager, Warning, TEXT("Package filter ignored due to class filter being specified."));
		}
	}
	else 
	{
		// No class(es) specified, so enumerate from packages

		// Must have at least one package specified
		if(PackagesString.IsEmpty())
		{
			UE_LOG(LogGraphNodeImager, Error, TEXT("Either a class filter or package filter must be specified, aborting."));
			return;
		}

		TArray< FString > PackageNames;
		PackagesString.ParseIntoArray(PackageNames, ListDelims, NumDelims);
		ensure(PackageNames.Num());

		BatchProcessFunc = [PackageNames](FObjectProcessFunc& ObjectProcessFunc) -> int32
		{
			int32 Result = 0;
			for(auto const& PkgName : PackageNames)
			{
				// Attempt to find the package
				auto Package = FindPackage(nullptr, *PkgName);
				if(Package == nullptr)
				{
					// If it is not in memory, try to load it.
					Package = LoadPackage(nullptr, *PkgName, LOAD_None);
				}
				if(Package == nullptr)
				{
					UE_LOG(LogGraphNodeImager, Warning, TEXT("Failed to find specified package '%s', skipping."), *PkgName);
					continue;
				}

				// Make sure it's fully loaded
				Package->FullyLoad();

				// @TODO: Work out why the below enumeration is called twice for every class in the package (it's called on the exact same uclass instance)
				TSet< UObject* > Processed;

				// Functor to invoke on every object found in the package
				TFunction< void(UObject*) > ObjectEnumFtr = [&](UObject* Obj)
				{
					// The BP action database appears to be keyed either on native UClass objects, or, in the
					// case of blueprints, on the Blueprint object itself, as opposed to the generated class.

					UObject* ObjectToProcess = nullptr;

					// Native class?
					if(auto Class = Cast< UClass >(Obj))
					{
						if(Class->HasAllClassFlags(CLASS_Native) && !Class->HasAnyFlags(RF_ClassDefaultObject))
						{
							ObjectToProcess = Class;
						}
					}
					else if(auto Blueprint = Cast< UBlueprint >(Obj))
					{
						ObjectToProcess = Blueprint;
					}

					if(ObjectToProcess && !Processed.Contains(ObjectToProcess))
					{
						UE_LOG(LogGraphNodeImager, Log, TEXT("Enumerating object '%s' in package '%s'"), *ObjectToProcess->GetName(), *PkgName);

						// Process this class
						Result += ObjectProcessFunc(ObjectToProcess);
						
						Processed.Add(ObjectToProcess);
					}
				};

				// Enumerate all objects in the package
				ForEachObjectWithOuter(Package, ObjectEnumFtr, true /* Include nested */);
			}

			return Result;
		};
	}

	check((bool)BatchProcessFunc);

	// Initialize the doc generator
	auto Gen = MakeUnique< FNodeDocsGenerator >();
	if(Gen->Init(DocTitle))
	{
		FObjectProcessFunc ClassFunc = [&](UObject* Object) -> int32
		{
			return Gen->ProcessSourceObject(Object, IntermediateDir);
		};

		bool const bCleanIntermediate = true;
		if(bCleanIntermediate)
		{
			IFileManager::Get().DeleteDirectory(*IntermediateDir, false, true);
		}

		// Invoke the batch processor, which will call the generator for every class enumerated.
		int32 Count = BatchProcessFunc(ClassFunc);

		Gen->Finalize(IntermediateDir);

		// TEMP!!!!
		//Gen.Release();
		//

		// Destroy the generator, which will also kill the host window.
		Gen.Reset();

		if(Count > 0)
		{
			UE_LOG(LogGraphNodeImager, Log, TEXT("Intermediate docs generated for %i nodes."), Count);

			ProcessIntermediateDocs(IntermediateDir, OutputDir, DocTitle, bCleanOutput);
		}
		else
		{
			UE_LOG(LogGraphNodeImager, Warning, TEXT("No nodes documented!"));
		}
	}
	else
	{
		UE_LOG(LogGraphNodeImager, Error, TEXT("Failed to initialize doc generator, aborting."));
	}
}

void FGraphNodeImagerModule::ProcessIntermediateDocs(FString const& IntermediateDir, FString const& OutputDir, FString const& DocTitle, bool bCleanOutput)
{
	const FString DotNETBinariesDir = FPaths::EngineDir() / TEXT("Binaries/DotNET");
	const FString DocGenExeName = TEXT("KantanDocGen.exe");
	const FString DocGenPath = DotNETBinariesDir / DocGenExeName;

	// Create a read and write pipe for the child process
	void* PipeRead = nullptr;
	void* PipeWrite = nullptr;
	verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));

	FString Args =
		FString(TEXT("-outputdir=")) + TEXT("\"") + OutputDir + TEXT("\"")
		+ TEXT(" -fromintermediate -intermediatedir=") + TEXT("\"") + IntermediateDir + TEXT("\"")
		+ TEXT(" -name=") + DocTitle
		+ (bCleanOutput ? TEXT(" -cleanoutput") : TEXT(""))
		;
	FProcHandle Proc = FPlatformProcess::CreateProc(
		*DocGenPath,
		*Args,
		true,
		false,
		false,
		nullptr,
		0,
		nullptr,
		PipeWrite
	);

	if(Proc.IsValid())
	{
		int32 ReturnCode = 0;
		FString BufferedText;
		for(bool bProcessFinished = false; !bProcessFinished; )
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
			while(BufferedText.FindChar('\n', EndOfLineIdx))
			{
				FString Line = BufferedText.Left(EndOfLineIdx);
				Line.RemoveFromEnd(TEXT("\r"));

				UE_LOG(LogGraphNodeImager, Log, TEXT("[KantanDocGen] %s"), *Line);

				BufferedText = BufferedText.Mid(EndOfLineIdx + 1);
			}

			FPlatformProcess::Sleep(0.1f);
		}

		//FPlatformProcess::WaitForProc(Proc);
		//FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode);
		FPlatformProcess::CloseProc(Proc);
		Proc.Reset();

		if(ReturnCode != 0)
		{
			UE_LOG(LogGraphNodeImager, Error, TEXT("KantanDocGen tool failed, see above output."));
		}
	}

	// Close the pipes
	FPlatformProcess::ClosePipe(0, PipeRead);
	FPlatformProcess::ClosePipe(0, PipeWrite);
}


#undef LOCTEXT_NAMESPACE


