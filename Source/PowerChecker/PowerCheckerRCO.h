#pragma once

#include "FGRemoteCallObject.h"
#include "PowerCheckerBuilding.h"
#include "PowerCheckerRCO.generated.h"

UCLASS(Blueprintable)
class POWERCHECKER_API UPowerCheckerRCO : public UFGRemoteCallObject
{
	GENERATED_BODY()
public:
	static UPowerCheckerRCO* getRCO(UWorld* world);

	UFUNCTION(BlueprintCallable, Category="PowerCheckerRCO", DisplayName="Get Power Checker RCO")
	static UPowerCheckerRCO*
	getRCO(AActor* actor)
	{
		return getRCO(actor->GetWorld());
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="PowerCheckerRCO", DisplayName="SetIncludePaused")
	virtual void SetIncludePaused(class APowerCheckerBuilding* powerChecker, bool includePaused = true);

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="PowerCheckerRCO", DisplayName="SetIncludeOutOfFuel")
	virtual void SetIncludeOutOfFuel(class APowerCheckerBuilding* powerChecker, bool includeOutOfFuel = true);

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="PowerCheckerRCO", DisplayName="SetCalculatedMaximumPotential")
	virtual void TriggerUpdateValues
	(
		class APowerCheckerBuilding* powerChecker,
		bool updateMaximumPotential = false,
		bool withDetails = false,
		PowerCheckerFilterType filterType = PowerCheckerFilterType::Any
	);

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="PowerCheckerRCO")
	virtual void SetProductionPaused(class AFGBuildableFactory* factory, bool isProductionPaused);

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="PowerCheckerRCO")
	virtual void SetPendingPotential(class AFGBuildableFactory* factory, float pendingPotential);

	UPROPERTY(Replicated)
	bool dummy = true;
};
