#pragma once
#include "FGBuildableFactory.h"
#include "Logic/PowerCheckerLogic.h"

#include "PowerCheckerBuilding.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FUpdateValues,
	float,
	maximumProduction,
	float,
	currentConsumption,
	float,
	maximumConsumption
	);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FUpdateValuesWithDetails,
	float,
	maximumProduction,
	float,
	currentConsumption,
	float,
	maximumConsumption,
	const TArray<FPowerDetail>&,
	outPowerDetails
	);

UCLASS(Blueprintable)
class POWERCHECKER_API APowerCheckerBuilding : public AFGBuildableFactory
{
	GENERATED_BODY()

	FString _TAG_NAME = TEXT("PowerCheckerBuilding: ");

	inline static FString
	getTimeStamp()
	{
		const auto now = FDateTime::Now();

		return FString::Printf(TEXT("%02d:%02d:%02d"), now.GetHour(), now.GetMinute(), now.GetSecond());
	}

	inline FString
	getTagName() const
	{
		return getTimeStamp() + TEXT(" ") + _TAG_NAME;
	}

public:
	APowerCheckerBuilding();

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="PowerChecker", DisplayName="TriggerUpdateValues")
	virtual void TriggerUpdateValues(bool updateMaximumPotential = false, bool withDetails = false, PowerCheckerFilterType filterType = PowerCheckerFilterType::Any);
	virtual void Server_TriggerUpdateValues(bool updateMaximumPotential = false, bool withDetails = false, PowerCheckerFilterType filterType = PowerCheckerFilterType::Any);

	UFUNCTION(BlueprintCallable, Category="PowerChecker", DisplayName="SetIncludePaused")
	virtual void SetIncludePaused(bool includePaused = true);
	virtual void Server_SetIncludePaused(bool includePaused = true);

	UFUNCTION(BlueprintCallable, Category="PowerChecker", DisplayName="SetIncludeOutOfFuel")
	virtual void SetIncludeOutOfFuel(bool includeOutOfFuel = true);
	virtual void Server_SetIncludeOutOfFuel(bool includeOutOfFuel = true);

	UPROPERTY(BlueprintAssignable, Category = "PowerChecker")
	FUpdateValues OnUpdateValues;

	UFUNCTION(BlueprintCallable, Category = "EfficiencyChecker", NetMulticast, Reliable)
	virtual void UpdateValues
	(
		float maximumProduction,
		float currentConsumption,
		float maximumConsumption
	);

	// UFUNCTION(BlueprintImplementableEvent, Category = "EfficiencyChecker")
	// void UpdateValuesEvent
	// (
	// 	float maximumProduction,
	// 	float currentConsumption,
	// 	float maximumConsumption
	// );

	UPROPERTY(BlueprintAssignable, Category = "PowerChecker")
	FUpdateValuesWithDetails OnUpdateValuesWithDetails;

	UFUNCTION(BlueprintCallable, Category = "EfficiencyChecker", NetMulticast, Reliable)
	virtual void UpdateValuesWithDetails
	(
		float maximumProduction,
		float currentConsumption,
		float maximumConsumption,
		const TArray<FPowerDetail>& powerDetails
	);

	// UFUNCTION(BlueprintImplementableEvent, Category = "EfficiencyChecker")
	// void UpdateValuesWithDetailsEvent
	// (
	// 	float maximumProduction,
	// 	float currentConsumption,
	// 	float maximumConsumption,
	// 	const TArray<FPowerDetail>& powerDetails
	// );

	virtual EProductionStatus GetProductionIndicatorStatus() const override;

	virtual bool ValidateTick(int testCircuitId);
	virtual bool checkPlayerIsNear();
	virtual int getCircuitId();

	UPROPERTY(BlueprintReadWrite, Replicated)
	EProductionStatus productionStatus = EProductionStatus::IS_NONE;

	UPROPERTY(BlueprintReadWrite, Replicated)
	bool isOverflow = false;

	UPROPERTY(BlueprintReadWrite, Replicated)
	int currentCircuitId = 0;

	UPROPERTY(BlueprintReadWrite, Replicated)
	float calculatedMaximumPotential = 0;

	UPROPERTY(BlueprintReadWrite, Replicated, SaveGame)
	bool includePaused = true;

	UPROPERTY(BlueprintReadWrite, Replicated, SaveGame)
	bool includeOutOfFuel = true;

	float lastCheck = 0;
};
