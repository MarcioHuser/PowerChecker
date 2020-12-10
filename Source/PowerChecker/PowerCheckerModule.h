#pragma once

#include "DateTime.h"
#include "ModuleManager.h"

#include <map>

#include "FGPowerCircuit.h"

class FPowerCheckerModule: public FDefaultGameModuleImpl {
public:
    virtual void StartupModule() override;

    virtual bool IsGameModule() const override { return true; }

    static void onPowerCircuitChangedHook(UFGPowerCircuit* powerCircuit);

    inline static FString
    getTimeStamp()
    {
        const auto now = FDateTime::Now();

        return FString::Printf(TEXT("%02d:%02d:%02d"), now.GetHour(), now.GetMinute(), now.GetSecond());
    }

    static bool logInfoEnabled;
    static float maximumPlayerDistance;
    static float spareLimit;
    static float overflowBlinkCycle;
    static std::map<FString, float> powerConsumptionMap;
};
