#include "PowerCheckerModule.h"

#include "util/Logging.h"

#include "Util/Optimize.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

bool FPowerCheckerModule::logInfoEnabled = false;
float FPowerCheckerModule::maximumPlayerDistance = 4000;
float FPowerCheckerModule::spareLimit = 100;
float FPowerCheckerModule::overflowBlinkCycle = 1;
std::map<FString, float> FPowerCheckerModule::powerConsumptionMap;

void FPowerCheckerModule::StartupModule()
{
    SML::Logging::info(*getTimeStamp(), TEXT(" PowerChecker: StartupModule"));

    TSharedRef<FJsonObject> defaultValues(new FJsonObject());

    TSharedPtr<FJsonObject> powerMappingJson(new FJsonObject());
    powerMappingJson->SetNumberField(TEXT("/Game/Teleporter/buildable/Build_Teleporteur.Build_Teleporteur_C"), -20);

    defaultValues->SetBoolField(TEXT("logInfoEnabled"), logInfoEnabled);
    defaultValues->SetNumberField(TEXT("maximumPlayerDistance"), maximumPlayerDistance);
    defaultValues->SetNumberField(TEXT("spareLimit"), spareLimit);
    defaultValues->SetNumberField(TEXT("overflowBlinkCycle"), overflowBlinkCycle);
    defaultValues->SetObjectField(TEXT("powerMapping"), powerMappingJson);

    defaultValues = SML::ReadModConfig(TEXT("PowerChecker"), defaultValues);

    logInfoEnabled = defaultValues->GetBoolField(TEXT("logInfoEnabled"));
    maximumPlayerDistance = defaultValues->GetNumberField(TEXT("maximumPlayerDistance"));
    spareLimit = defaultValues->GetNumberField(TEXT("spareLimit"));
    overflowBlinkCycle = defaultValues->GetNumberField(TEXT("overflowBlinkCycle"));
    powerMappingJson = defaultValues->GetObjectField(TEXT("powerMapping"));

    SML::Logging::info(*getTimeStamp(), TEXT(" PowerChecker: logInfoEnabled = "), logInfoEnabled ? TEXT("true") : TEXT("false"));
    SML::Logging::info(*getTimeStamp(), TEXT(" PowerChecker: maximumPlayerDistance = "), maximumPlayerDistance);
    SML::Logging::info(*getTimeStamp(), TEXT(" PowerChecker: spareLimit = "), spareLimit);
    SML::Logging::info(*getTimeStamp(), TEXT(" PowerChecker: powerMapping:"));

    if(powerMappingJson)
    {
        for (auto entry : powerMappingJson->Values)
        {
            auto key = entry.Key;
            auto value = entry.Value->AsNumber();
            
            powerConsumptionMap[key] =  value;

            SML::Logging::info(*getTimeStamp(), TEXT(" PowerChecker:     - "), *key, TEXT(" = "), value);
        }
    }

    SML::Logging::info(*getTimeStamp(), TEXT(" ==="));
}

IMPLEMENT_GAME_MODULE(FPowerCheckerModule, PowerChecker);
