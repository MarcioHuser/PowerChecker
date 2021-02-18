#include "PowerCheckerBuilding.h"

#include "FGPowerCircuit.h"
#include "FGPowerInfoComponent.h"
#include "FGProductionIndicatorInstanceComponent.h"
#include "GeneratedCodeHelpers.h"
#include "PowerCheckerModule.h"
#include "PowerCheckerRCO.h"

#include "SML/util/Logging.h"

#include "Util/Optimize.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

APowerCheckerBuilding::APowerCheckerBuilding()
{
}

void APowerCheckerBuilding::BeginPlay()
{
	Super::BeginPlay();
}

void APowerCheckerBuilding::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		auto testCircuitId = getCircuitId();

		if (!ValidateTick(testCircuitId))
		{
			return;
		}

		lastCheck = GetWorld()->GetTimeSeconds();

		auto updateMaximumPotential = false;

		if (testCircuitId != currentCircuitId)
		{
			if (FPowerCheckerModule::logInfoEnabled)
			{
				SML::Logging::info(
					*getTagName(),
					TEXT("Recalculating because Circuit ID changed from "),
					currentCircuitId,
					TEXT(" to "),
					testCircuitId
					);
			}

			currentCircuitId = testCircuitId;
			updateMaximumPotential = true;
		}

		Server_TriggerUpdateValues(updateMaximumPotential, false);
	}
}

void APowerCheckerBuilding::SetIncludePaused(bool includePaused)
{
	if (HasAuthority())
	{
		Server_SetIncludePaused(includePaused);
	}
	else
	{
		auto rco = UPowerCheckerRCO::getRCO(GetWorld());
		if (rco)
		{
			if (FPowerCheckerModule::logInfoEnabled)
			{
				SML::Logging::info(*getTagName(), TEXT("Calling SetCustomInjectedInput at server"));
			}

			rco->SetIncludePaused(this, includePaused);
		}
	}
}

void APowerCheckerBuilding::Server_SetIncludePaused(bool includePaused)
{
	this->includePaused = includePaused;
}

void APowerCheckerBuilding::TriggerUpdateValues(bool updateMaximumPotential, bool withDetails)
{
	if (HasAuthority())
	{
		Server_TriggerUpdateValues(updateMaximumPotential, withDetails);
	}
	else
	{
		auto rco = UPowerCheckerRCO::getRCO(GetWorld());
		if (rco)
		{
			if (FPowerCheckerModule::logInfoEnabled)
			{
				SML::Logging::info(*getTagName(), TEXT("Calling SetCustomInjectedInput at server"));
			}

			rco->TriggerUpdateValues(this, updateMaximumPotential, withDetails);
		}
	}
}

void APowerCheckerBuilding::Server_TriggerUpdateValues(bool updateMaximumPotential, bool withDetails)
{
	auto powerConnection = FindComponentByClass<UFGPowerConnectionComponent>();

	auto powerCircuit = powerConnection ? powerConnection->GetPowerCircuit() : nullptr;

	FPowerCircuitStats circuitStats;
	if (powerCircuit)
	{
		powerCircuit->GetStats(circuitStats);
	}

	TArray<FPowerDetail> powerDetails;

	if (updateMaximumPotential)
	{
		APowerCheckerLogic::GetMaximumPotentialWithDetails(powerConnection, calculatedMaximumPotential, includePaused, withDetails, powerDetails);
	}

	isOverflow = circuitStats.PowerProductionCapacity > 0 && circuitStats.PowerProductionCapacity < calculatedMaximumPotential;

	if (circuitStats.PowerProductionCapacity == 0 || circuitStats.PowerProductionCapacity < calculatedMaximumPotential)
	{
		productionStatus = EProductionStatus::IS_ERROR;
	}
	else if (circuitStats.PowerProductionCapacity - FPowerCheckerModule::spareLimit < calculatedMaximumPotential)
	{
		productionStatus = EProductionStatus::IS_STANDBY;
	}
	else
	{
		productionStatus = EProductionStatus::IS_PRODUCING;
	}

	if (withDetails)
	{
		UpdateValuesWithDetails(circuitStats.PowerProductionCapacity, circuitStats.PowerConsumed, calculatedMaximumPotential, powerDetails);
		// UpdateValuesWithDetailsEvent(circuitStats.PowerProductionCapacity, circuitStats.PowerConsumed, calculatedMaximumPotential, powerDetails);
	}
	else
	{
		UpdateValues(circuitStats.PowerProductionCapacity, circuitStats.PowerConsumed, calculatedMaximumPotential);
		// UpdateValuesEvent(circuitStats.PowerProductionCapacity, circuitStats.PowerConsumed, calculatedMaximumPotential);
	}
}

void APowerCheckerBuilding::UpdateValues_Implementation(float maximumProduction, float currentConsumption, float maximumConsumption)
{
	// if (FPowerCheckerModule::logInfoEnabled)
	// {
	// 	SML::Logging::info(
	// 		*getTagName(),
	// 		TEXT("UpdateValues_Implementation: authority = "),
	// 		HasAuthority() ? TEXT("true") : TEXT("false"),
	// 		TEXT(" / maximumProduction = "),
	// 		maximumProduction,
	// 		TEXT(" / currentConsumption = "),
	// 		currentConsumption,
	// 		TEXT(" / maximumConsumption = "),
	// 		maximumConsumption
	// 		);
	// }

	OnUpdateValues.Broadcast(maximumProduction, currentConsumption, maximumConsumption);
}

void APowerCheckerBuilding::UpdateValuesWithDetails_Implementation
(float maximumProduction, float currentConsumption, float maximumConsumption, const TArray<FPowerDetail>& powerDetails)
{
	// if (FPowerCheckerModule::logInfoEnabled)
	// {
	// 	SML::Logging::info(
	// 		*getTagName(),
	// 		TEXT("UpdateValuesWithDetails_Implementation: authority = "),
	// 		HasAuthority() ? TEXT("true") : TEXT("false"),
	// 		TEXT(" / maximumProduction = "),
	// 		maximumProduction,
	// 		TEXT(" / currentConsumption = "),
	// 		currentConsumption,
	// 		TEXT(" / maximumConsumption = "),
	// 		maximumConsumption,
	// 		TEXT(" / powerDetails = "),
	// 		powerDetails.Num(),
	// 		TEXT(" items")
	// 		);
	// }

	OnUpdateValuesWithDetails.Broadcast(maximumProduction, currentConsumption, maximumConsumption, powerDetails);
}

EProductionStatus APowerCheckerBuilding::GetProductionIndicatorStatus() const
{
	// // auto baseStatus = Super::GetProductionIndicatorStatus();
	//
	// auto powerInfo = GetPowerInfo();
	//
	// if (!powerInfo)
	// {
	//     return EProductionStatus::IS_NONE;
	// }
	// else if (powerInfo->IsFuseTriggered())
	// {
	//     return EProductionStatus::IS_ERROR;
	// }

	return productionStatus == EProductionStatus::IS_ERROR &&
	       isOverflow &&
	       (int)(GetWorld()->GetTimeSeconds() / (FPowerCheckerModule::overflowBlinkCycle / 2)) % 2 == 0
		       ? EProductionStatus::IS_NONE
		       : productionStatus;
}

bool APowerCheckerBuilding::ValidateTick(int testCircuitId)
{
	return (lastCheck + 1 <= GetWorld()->GetTimeSeconds() || testCircuitId != currentCircuitId) && checkPlayerIsNear();
}

bool APowerCheckerBuilding::checkPlayerIsNear()
{
	auto playerIt = GetWorld()->GetPlayerControllerIterator();
	for (; playerIt; ++playerIt)
	{
		const auto player = Cast<AFGCharacterPlayer>((*playerIt)->GetPawn());
		if (!player)
		{
			continue;
		}

		auto playerTranslation = player->GetActorLocation();

		auto playerState = player->GetPlayerState();

		auto distance = FVector::Dist(playerTranslation, GetActorLocation());

		if (distance <= FPowerCheckerModule::maximumPlayerDistance)
		{
			// if (FPowerCheckerModule::logInfoEnabled)
			// {
			// 	SML::Logging::info(
			// 		*getTagName(),
			// 		TEXT("Player "),
			// 		*playerState->GetPlayerName(),
			// 		TEXT(" location = "),
			// 		*playerTranslation.ToString(),
			// 		TEXT(" / build location = "),
			// 		*GetActorLocation().ToString(),
			// 		TEXT(" / distance = "),
			// 		distance
			// 		);
			// }

			return true;
		}
	}

	return false;
}

void APowerCheckerBuilding::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APowerCheckerBuilding, productionStatus);
	DOREPLIFETIME(APowerCheckerBuilding, isOverflow);
	DOREPLIFETIME(APowerCheckerBuilding, currentCircuitId);
	DOREPLIFETIME(APowerCheckerBuilding, calculatedMaximumPotential);
	DOREPLIFETIME(APowerCheckerBuilding, includePaused);
}

int APowerCheckerBuilding::getCircuitId()
{
	auto powerConnection = FindComponentByClass<UFGPowerConnectionComponent>();
	if (!powerConnection)
	{
		return 0;
	}

	auto powerCircuit = powerConnection->GetPowerCircuit();
	if (!powerCircuit)
	{
		return 0;
	}

	return powerCircuit->GetCircuitID();
}
