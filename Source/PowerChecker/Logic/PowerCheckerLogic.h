#pragma once

#include "FGBuildableFactory.h"
#include "FGBuildDescriptor.h"
#include "FGPowerConnectionComponent.h"

#include "PowerCheckerLogic.generated.h"

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
        float& totalMaximumPotential
    );

    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    static void GetMaximumPotentialWithDetails
    (
        UFGPowerConnectionComponent* powerConnection,
        float& totalMaximumPotential,
        bool includePowerDetails,
        TArray<FPowerDetail>& outPowerDetails
    );

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="PowerCheckerLogic")
    static bool IsLogInfoEnabled();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="PowerCheckerLogic")
    static float GetMaximumPlayerDistance();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="PowerCheckerLogic")
    static float GetSpareLimit();

    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    virtual void Initialize();

    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    virtual void Terminate();

    UFUNCTION(BlueprintCallable, Category="PowerCheckerLogic")
    virtual bool IsValidBuildable(class AFGBuildable* newBuildable);

    static void dumpUnknownClass(AActor* owner);
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

    FCriticalSection eclCritical;

    TSet<class AFGBuildable*> allTeleporters;

    FActorEndPlaySignature::FDelegate removeTeleporterDelegate;

    virtual void addTeleporter(AFGBuildable* actor);

    UFUNCTION()
    virtual void removeTeleporter(AActor* actor, EEndPlayReason::Type reason);
};
