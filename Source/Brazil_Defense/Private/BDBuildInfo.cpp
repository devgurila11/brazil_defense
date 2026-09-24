// Brazil Defense. Which build is running, said the same way everywhere.

#include "BDBuildInfo.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"

const FString& BDBuildInfo::GetLabel()
{
	static const FString Label = []()
	{
		// The file that holds this code: the module DLL when modules are separate (the
		// editor), the executable when everything is linked into one (a packaged game).
#if IS_MONOLITHIC
		const FString Binary = FPlatformProcess::ExecutablePath();
#else
		const FString Binary = FModuleManager::Get().GetModuleFilename(TEXT("Brazil_Defense"));
#endif
		const FDateTime Utc = IFileManager::Get().GetTimeStamp(*Binary);
		const TCHAR* Configuration = LexToString(FApp::GetBuildConfiguration());
		if (Utc == FDateTime::MinValue())
		{
			return FString::Printf(TEXT("build ? %s"), Configuration);
		}

		// The file system keeps UTC; the tester reads a local clock.
		const FDateTime Local = Utc + (FDateTime::Now() - FDateTime::UtcNow());
		return FString::Printf(TEXT("build %s %s"), *Local.ToString(TEXT("%Y.%m.%d-%H%M")), Configuration);
	}();
	return Label;
}
