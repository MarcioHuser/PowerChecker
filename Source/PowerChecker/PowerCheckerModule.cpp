#include "PowerCheckerModule.h"

#include "util/Logging.h"

#include "Util/Optimize.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

bool FPowerCheckerModule::logInfoEnabled = false;
TMap<FString, float> FPowerCheckerModule::powerConsumptionMap;

void FPowerCheckerModule::StartupModule()
{
    SML::Logging::info(*getTimeStamp(), TEXT(" PowerChecker: StartupModule"));

    TSharedRef<FJsonObject> defaultValues(new FJsonObject());

    TSharedPtr<FJsonObject> powerMapping(new FJsonObject());
    powerMapping->SetNumberField(TEXT("/Game/Teleporter/buildable/Build_Teleporteur.Build_Teleporteur_C"), 20);

    defaultValues->SetBoolField(TEXT("logInfoEnabled"), logInfoEnabled);
    defaultValues->SetObjectField(TEXT("powerMapping"), powerMapping);

    defaultValues = SML::ReadModConfig(TEXT("PowerChecker"), defaultValues);

    logInfoEnabled = defaultValues->GetBoolField(TEXT("logInfoEnabled"));
    powerMapping = defaultValues->GetObjectField(TEXT("powerMapping"));

    SML::Logging::info(*getTimeStamp(), TEXT(" PowerChecker: logInfoEnabled = "), logInfoEnabled ? TEXT("true") : TEXT("false"));
}

IMPLEMENT_GAME_MODULE(FPowerCheckerModule, PowerChecker);
