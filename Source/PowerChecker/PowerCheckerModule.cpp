#include "PowerCheckerModule.h"

#include "util/Logging.h"

#include "Util/Optimize.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

bool FPowerCheckerModule::logInfoEnabled = false;

void FPowerCheckerModule::StartupModule()
{
    SML::Logging::info(*getTimeStamp(), TEXT(" PowerChecker: StartupModule"));

    TSharedRef<FJsonObject> defaultValues(new FJsonObject());

    defaultValues->SetBoolField(TEXT("logInfoEnabled"), logInfoEnabled);

    defaultValues = SML::ReadModConfig(TEXT("PowerChecker"), defaultValues);

    logInfoEnabled = defaultValues->GetBoolField(TEXT("logInfoEnabled"));

    SML::Logging::info(*getTimeStamp(), TEXT(" PowerChecker: logInfoEnabled = "), logInfoEnabled ? TEXT("true") : TEXT("false"));
}

IMPLEMENT_GAME_MODULE(FPowerCheckerModule, PowerChecker);
