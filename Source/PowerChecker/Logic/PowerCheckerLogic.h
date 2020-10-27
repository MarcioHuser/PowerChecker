#pragma once

#include "FGBuildableFactory.h"
#include "FGPowerConnectionComponent.h"

#include "PowerCheckerLogic.generated.h"

UCLASS(Blueprintable)
class POWERCHECKER_API APowerCheckerLogic : public AActor
{
    GENERATED_BODY()
public:
    APowerCheckerLogic();

    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    static float GetMaximumPotential(UFGCircuitConnectionComponent* powerConnection);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="PowerCheckerLogic")
        static bool IsLogInfoEnabled();

    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    virtual void Initialize();
    
    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    virtual void Terminate();

    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    virtual bool IsValidBuildable(class AFGBuildable* newBuildable);

    static void dumpUnknownClass(AActor* owner);
    
    inline static FString
    getTimeStamp()
    {
        const auto now = FDateTime::Now();

        return FString::Printf(TEXT("%02d:%02d:%02d"), now.GetHour(), now.GetMinute(), now.GetSecond());
    }

    UFUNCTION()
    virtual void OnFGBuildableSubsystemBuildableConstructed(AFGBuildable* buildable);
    
    static APowerCheckerLogic* singleton;

    FCriticalSection eclCritical;

    TSet<class AFGBuildable*> allTeleporters;
    
    FActorEndPlaySignature::FDelegate removeTeleporterDelegate;

    virtual void addTeleporter(AFGBuildable* actor);

    UFUNCTION()
    virtual void removeTeleporter(AActor* actor, EEndPlayReason::Type reason);
};
