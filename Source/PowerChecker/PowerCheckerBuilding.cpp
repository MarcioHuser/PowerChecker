#include "PowerCheckerBuilding.h"
#include "FGPowerInfoComponent.h"

#include "Util/Optimize.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

APowerCheckerBuilding::APowerCheckerBuilding()
{
}

EProductionStatus APowerCheckerBuilding::GetProductionIndicatorStatus() const
{
    // auto baseStatus = Super::GetProductionIndicatorStatus();

    auto powerInfo = GetPowerInfo();

    if(!powerInfo || powerInfo->IsFuseTriggered())
    {
        return EProductionStatus::IS_NONE;
    }

    return productionStatus;
}
