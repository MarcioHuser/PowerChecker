﻿#pragma once

#include "FGBuildableFactory.h"
#include "FGBuildDescriptor.h"
#include "FGPowerConnectionComponent.h"

#include "PowerCheckerLogic.generated.h"

UENUM(BlueprintType)
enum class PowerCheckerFilterType : uint8
{
    Any,
    PausedOnly,
    OutOfFuelOnly
};

USTRUCT(Blueprintable)
struct POWERCHECKER_API FPowerDetail
{
    GENERATED_USTRUCT_BODY()
public:

    UPROPERTY(EditAnywhere,BlueprintReadWrite, Category="PowerCheckerLogic|PowerDetail")
    TSubclassOf<UFGItemDescriptor> buildingType;

    UPROPERTY(EditAnywhere,BlueprintReadWrite, Category="PowerCheckerLogic|PowerDetail")
    float powerPerBuilding = 0;

    UPROPERTY(EditAnywhere,BlueprintReadWrite, Category="PowerCheckerLogic|PowerDetail")
    int potential = 100;

    UPROPERTY(EditAnywhere,BlueprintReadWrite, Category="PowerCheckerLogic|PowerDetail")
    int amount = 0;

    UPROPERTY(EditAnywhere,BlueprintReadWrite, Category="PowerCheckerLogic|PowerDetail")
    TArray<class AFGBuildableFactory*> factories;

public:
    FORCEINLINE ~FPowerDetail() = default;
};

UCLASS(Blueprintable)
class POWERCHECKER_API APowerCheckerLogic : public AActor
{
    GENERATED_BODY()
public:
    APowerCheckerLogic();

    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    static void GetMaximumPotential
    (
        UFGPowerConnectionComponent* powerConnection,
        float& totalMaximumPotential,
        bool includePaused,
        bool includeOutOfFuel,
        PowerCheckerFilterType filterType
    );

    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    static void GetMaximumPotentialWithDetails
    (
        UFGPowerConnectionComponent* powerConnection,
        float& totalMaximumPotential,
        bool includePaused,
        bool includeOutOfFuel,
        bool includePowerDetails,
        TArray<FPowerDetail>& outPowerDetails,
        PowerCheckerFilterType filterType
    );

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="PowerCheckerLogic")
    static bool IsLogInfoEnabled();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="PowerCheckerLogic")
    static float GetMaximumPlayerDistance();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="PowerCheckerLogic")
    static float GetSpareLimit();

    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    virtual void Initialize(TSubclassOf<class UFGItemDescriptor> in_dropPodStub);

    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    virtual void Terminate();

    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    virtual bool IsValidBuildable(class AFGBuildable* newBuildable);

    static void dumpUnknownClass(UObject* obj);
    static bool inheritsFromClass(AActor* owner, const FString& className);

    inline static FString
    getTimeStamp()
    {
        const auto now = FDateTime::Now();

        return FString::Printf(TEXT("%02d:%02d:%02d"), now.GetHour(), now.GetMinute(), now.GetSecond());
    }

    UFUNCTION()
    virtual void OnFGBuildableSubsystemBuildableConstructed(AFGBuildable* buildable);

    static APowerCheckerLogic* singleton;

    static TSubclassOf<class UFGItemDescriptor> dropPodStub;

    FCriticalSection eclCritical;

    TSet<class AFGBuildable*> allTeleporters;
    TSet<class APowerCheckerBuilding*> allPowerCheckers;

    FActorEndPlaySignature::FDelegate removeTeleporterDelegate;
    FActorEndPlaySignature::FDelegate removePowerCheckerDelegate;

    virtual void addTeleporter(class AFGBuildable* actor);
    virtual void addPowerChecker(class APowerCheckerBuilding* actor);

    UFUNCTION()
    virtual void removeTeleporter(AActor* actor, EEndPlayReason::Type reason);
    
    UFUNCTION()
    virtual void removePowerChecker(AActor* actor, EEndPlayReason::Type reason);
};
