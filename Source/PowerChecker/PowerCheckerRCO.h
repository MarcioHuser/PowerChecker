#pragma once

#include "FGRemoteCallObject.h"
#include "PowerCheckerBuilding.h"
#include "PowerCheckerRCO.generated.h"

UCLASS()
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

	UFUNCTION(BlueprintCallable, Server, WithValidation, Reliable, Category="PowerCheckerRCO",DisplayName="SetCalculatedMaximumPotential")
    virtual void TriggerUpdateValues(class APowerCheckerBuilding* powerChecker, bool updateMaximumPotential = false, bool withDetails = false);
	
	UPROPERTY(Replicated)
	bool dummy = true;
};
