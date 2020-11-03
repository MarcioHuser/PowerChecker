#pragma once

#include "DateTime.h"
#include "ModuleManager.h"

class FPowerCheckerModule: public FDefaultGameModuleImpl {
public:
    virtual void StartupModule() override;

    virtual bool IsGameModule() const override { return true; }

    inline static FString
    getTimeStamp()
    {
        const auto now = FDateTime::Now();

        return FString::Printf(TEXT("%02d:%02d:%02d"), now.GetHour(), now.GetMinute(), now.GetSecond());
    }

    static bool logInfoEnabled;

    static TMap<FString, float> powerConsumptionMap;
};
