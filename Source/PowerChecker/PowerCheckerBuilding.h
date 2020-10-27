#pragma once
#include "FGBuildableFactory.h"
#include "PowerCheckerBuilding.generated.h"

UCLASS(Blueprintable)
class POWERCHECKER_API APowerCheckerBuilding : public AFGBuildableFactory
{
    GENERATED_BODY()

public:
    APowerCheckerBuilding();

    virtual EProductionStatus GetProductionIndicatorStatus() const override;

    UPROPERTY(BlueprintReadWrite)
    EProductionStatus productionStatus = EProductionStatus::IS_NONE; 
};
