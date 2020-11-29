#include "PowerCheckerRCO.h"
#include "PowerCheckerBuilding.h"

#include "FGPlayerController.h"

#include "UnrealNetwork.h"

#include "Util/Optimize.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

void UPowerCheckerRCO::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPowerCheckerRCO, dummy);
}

UPowerCheckerRCO* UPowerCheckerRCO::getRCO(UWorld* world)
{
	return Cast<UPowerCheckerRCO>(
		Cast<AFGPlayerController>(world->GetFirstPlayerController())->GetRemoteCallObjectOfClass(UPowerCheckerRCO::StaticClass())
		);
}

void UPowerCheckerRCO::TriggerUpdateValues_Implementation(APowerCheckerBuilding* powerChecker, bool updateMaximumPotential, bool withDetails)
{
	if (powerChecker->HasAuthority())
	{
		powerChecker->Server_TriggerUpdateValues(updateMaximumPotential, withDetails);
	}
}

bool UPowerCheckerRCO::TriggerUpdateValues_Validate(APowerCheckerBuilding* powerChecker, bool updateMaximumPotential, bool withDetails)
{
	return true;
}
