﻿#include "PowerCheckerLogic.h"
#include "PowerCheckerModule.h"
#include "PowerCheckerRCO.h"

#include "FGBuildableGenerator.h"
#include "FGBuildableRailroadStation.h"
#include "FGBuildableTrainPlatformCargo.h"
#include "FGCircuitSubsystem.h"
#include "FGFactoryConnectionComponent.h"
#include "FGGameMode.h"
#include "FGItemDescriptor.h"
#include "FGPowerCircuit.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerInfoComponent.h"
#include "FGRailroadSubsystem.h"
#include "FGTrain.h"
#include "FGRailroadVehicle.h"
#include "FGTrainStationIdentifier.h"

#include "SML/util/ReflectionHelper.h"
#include "SML/util/Logging.h"

#include "Util/Optimize.h"

#include <map>


#include "FGDropPod.h"
#include "FGLocomotive.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

APowerCheckerLogic* APowerCheckerLogic::singleton = nullptr;

APowerCheckerLogic::APowerCheckerLogic()
{
}

void APowerCheckerLogic::Initialize()
{
	singleton = this;

	auto subsystem = AFGBuildableSubsystem::Get(this);

	if (subsystem)
	{
		FOnBuildableConstructedGlobal::FDelegate constructedDelegate;

		constructedDelegate.BindDynamic(this, &APowerCheckerLogic::OnFGBuildableSubsystemBuildableConstructed);

		subsystem->BuildableConstructedGlobalDelegate.Add(constructedDelegate);

		TArray<AActor*> allBuildables;
		UGameplayStatics::GetAllActorsOfClass(subsystem->GetWorld(), AFGBuildable::StaticClass(), allBuildables);

		removeTeleporterDelegate.BindDynamic(this, &APowerCheckerLogic::removeTeleporter);

		for (auto buildableActor : allBuildables)
		{
			IsValidBuildable(Cast<AFGBuildable>(buildableActor));
		}

		auto gameMode = Cast<AFGGameMode>(UGameplayStatics::GetGameMode(subsystem->GetWorld()));
		if (gameMode)
		{
			gameMode->RegisterRemoteCallObjectClass(UPowerCheckerRCO::StaticClass());
		}
	}
}

void APowerCheckerLogic::OnFGBuildableSubsystemBuildableConstructed(AFGBuildable* buildable)
{
	IsValidBuildable(buildable);
}

void APowerCheckerLogic::Terminate()
{
	FScopeLock ScopeLock(&eclCritical);
	allTeleporters.Empty();

	singleton = nullptr;
}

void APowerCheckerLogic::GetMaximumPotential(UFGPowerConnectionComponent* powerConnection, float& totalMaximumPotential)
{
	TArray<FPowerDetail> powerDetails;

	GetMaximumPotentialWithDetails(powerConnection, totalMaximumPotential, false, powerDetails);
}

void APowerCheckerLogic::GetMaximumPotentialWithDetails
(UFGPowerConnectionComponent* powerConnection, float& totalMaximumPotential, bool includePowerDetails, TArray<FPowerDetail>& outPowerDetails)
{
	TSet<AActor*> seenActors;
	TSet<UFGPowerConnectionComponent*> pendingConnections;

	std::map<TSubclassOf<UFGItemDescriptor>, std::map<float, std::map<int, int>>> buildingDetails;

	auto nextActor = powerConnection->GetOwner();

	if (nextActor)
	{
		pendingConnections.Add(powerConnection);
	}

	auto railroadSubsystem = AFGRailroadSubsystem::Get(nextActor->GetWorld());
	//auto circuitSubsystem = AFGCircuitSubsystem::Get(nextBuilding->GetWorld());

	//auto powerCircuit = circuitSubsystem->FindCircuit<UFGPowerCircuit>(powerConnection->GetCircuitID());

	totalMaximumPotential = 0;

	bool teleporterFound = false;

	while (pendingConnections.Num())
	{
		powerConnection = *pendingConnections.begin();
		pendingConnections.Remove(powerConnection);

		nextActor = powerConnection->GetOwner();
		if (!nextActor)
		{
			continue;
		}

		seenActors.Add(nextActor);

		TArray<UFGPowerConnectionComponent*> powerConnections;
		nextActor->GetComponents<UFGPowerConnectionComponent>(powerConnections);

		TArray<UFGCircuitConnectionComponent*> connections;

		for (auto currentPowerConnection : powerConnections)
		{
			currentPowerConnection->GetConnections(connections);
			currentPowerConnection->GetHiddenConnections(connections);
		}

		auto trainStation = Cast<AFGBuildableRailroadStation>(nextActor);

		// Find power connections from all other stations in the circuit
		if (trainStation)
		{
			TArray<AFGTrainStationIdentifier*> stations;
			railroadSubsystem->GetTrainStations(trainStation->GetTrackGraphID(), stations);

			for (auto station : stations)
			{
				if (!station || !station->GetStation() || seenActors.Contains(station->GetStation()))
				{
					continue;
				}

				station->GetStation()->GetComponents<UFGPowerConnectionComponent>(powerConnections);

				connections.Append(powerConnections);

				if (includePowerDetails)
				{
					// Add all connected cargo platforms

					for (auto d = 0; d < 2; d++)
					{
						for (auto nextPlatform = station->GetStation()->GetConnectedPlatformInDirectionOf(d);
						     nextPlatform;
						     nextPlatform = nextPlatform->GetConnectedPlatformInDirectionOf(d))
						{
							auto cargoPlatform = Cast<AFGBuildableTrainPlatformCargo>(nextPlatform);

							if (!cargoPlatform || seenActors.Contains(cargoPlatform))
							{
								continue;
							}

							seenActors.Add(cargoPlatform);

							buildingDetails[cargoPlatform->GetBuiltWithDescriptor()]
								[-cargoPlatform->CalcProducingPowerConsumptionForPotential(cargoPlatform->GetPendingPotential())]
								[int(0.5 + cargoPlatform->GetPendingPotential() * 100)]++;
						}
					}
				}
			}

			TArray<AFGTrain*> trains;
			railroadSubsystem->GetTrains(trainStation->GetTrackGraphID(), trains);

			for (auto train : trains)
			{
				for (auto trainIt = AFGRailroadSubsystem::TTrainIterator(train->GetFirstVehicle()); trainIt; ++trainIt)
				{
					// dumpUnknownClass(vehicle);

					auto locomotive = Cast<AFGLocomotive>(*trainIt);

					if (!locomotive || seenActors.Contains(locomotive))
					{
						continue;
					}

					seenActors.Add(locomotive);

					auto producedPower = locomotive->GetPowerInfo()->GetMaximumTargetConsumption();

					buildingDetails[UFGRecipe::GetProducts(locomotive->GetBuiltWithRecipe())[0].ItemClass]
						[-producedPower]
						[100]++;
				}
			}
		}

		auto className = nextActor->GetClass()->GetPathName();
		auto buildable = Cast<AFGBuildable>(nextActor);
		auto buildDescriptor = buildable ? buildable->GetBuiltWithDescriptor() : nullptr;

		if (FPowerCheckerModule::logInfoEnabled)
		{
			SML::Logging::info(
				*getTimeStamp(),
				TEXT(" PowerChecker: "),
				*nextActor->GetName(),
				TEXT(" / "),
				*className
				);
		}

		for (auto connection : connections)
		{
			auto testPowerConnection = Cast<UFGPowerConnectionComponent>(connection);
			if (!testPowerConnection)
			{
				continue;
			}

			auto connectedBuilding = testPowerConnection->GetOwner();
			if (!connectedBuilding || seenActors.Contains(connectedBuilding) || pendingConnections.Contains(testPowerConnection))
			{
				continue;
			}

			pendingConnections.Add(testPowerConnection);
		}

		auto generator = Cast<AFGBuildableGenerator>(nextActor);
		auto factory = Cast<AFGBuildableFactory>(nextActor);
		auto dropPod = Cast<AFGDropPod>(nextActor);

		// if (className == TEXT("/Game/Teleporter/buildable/Build_Teleporteur.Build_Teleporteur_C"))
		// {
		//     if (FPowerCheckerModule::logInfoEnabled)
		//     {
		//         SML::Logging::info(*getTimeStamp(), TEXT("    Default Power Comsumption: 20"));
		//     }
		//
		//     totalMaximumPotential += 20;
		//
		//     if (buildDescriptor && includePowerDetails)
		//     {
		//         buildingDetails[buildDescriptor]
		//             [-20]
		//             [100]++;
		//     }
		// }

		auto powerIt = FPowerCheckerModule::powerConsumptionMap.find(className);

		if (powerIt != FPowerCheckerModule::powerConsumptionMap.end())
		{
			if (FPowerCheckerModule::logInfoEnabled)
			{
				SML::Logging::info(*getTimeStamp(), TEXT("    Default Power Comsumption: "), powerIt->second);
			}

			totalMaximumPotential += powerIt->second;

			if (buildDescriptor && includePowerDetails)
			{
				buildingDetails[buildDescriptor]
					[powerIt->second]
					[factory ? factory->GetPendingPotential() : 100]++;
			}
		}
		else if (className == TEXT("/Game/StorageTeleporter/Buildables/Hub/Build_STHub.Build_STHub_C"))
		{
			if (!teleporterFound)
			{
				teleporterFound = true;

				FScopeLock ScopeLock(&singleton->eclCritical);

				float itemTeleporterPower = 1;

				if (FPowerCheckerModule::logInfoEnabled)
				{
					SML::Logging::info(*getTimeStamp(), TEXT("    Hub Power Comsumption: 1"));
				}

				for (auto teleporter : singleton->allTeleporters)
				{
					TArray<UFGFactoryConnectionComponent*> teleportConnections;
					teleporter->GetComponents(teleportConnections);

					for (auto connection : teleportConnections)
					{
						if (connection->IsConnected() && connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
						{
							if (FPowerCheckerModule::logInfoEnabled)
							{
								SML::Logging::info(*getTimeStamp(), TEXT("    Teleporter "), *teleporter->GetPathName(), TEXT(" has output connection. Consumption: 10"));
							}

							itemTeleporterPower += 10;

							if (buildDescriptor && includePowerDetails)
							{
								buildingDetails[teleporter->GetBuiltWithDescriptor()]
									[-10]
									[100]++;
							}
						}
					}
				}

				totalMaximumPotential += itemTeleporterPower;

				if (buildDescriptor && includePowerDetails)
				{
					buildingDetails[buildDescriptor]
						[-1]
						[100]++;
				}
			}
		}
		else if (generator)
		{
			if (FPowerCheckerModule::logInfoEnabled)
			{
				SML::Logging::info(*getTimeStamp(), TEXT("    Can Produce: "), generator->CanProduce() ? TEXT("true") : TEXT("false"));
				SML::Logging::info(*getTimeStamp(), TEXT("    Load: "), generator->GetLoadPercentage(), TEXT("%"));
				SML::Logging::info(*getTimeStamp(), TEXT("    Power Production: "), generator->GetPowerProductionCapacity());
				SML::Logging::info(*getTimeStamp(), TEXT("    Default Power Production: "), generator->GetDefaultPowerProductionCapacity());
				SML::Logging::info(*getTimeStamp(), TEXT("    Pending Potential: "), generator->GetPendingPotential());
				SML::Logging::info(
					*getTimeStamp(),
					TEXT("    Producing Power Production Capacity For Potential: "),
					generator->CalcPowerProductionCapacityForPotential(generator->GetPendingPotential())
					);
				SML::Logging::info(*getTimeStamp(), TEXT("    Base Production: "), powerConnection->GetPowerInfo()->GetBaseProduction());
				SML::Logging::info(*getTimeStamp(), TEXT("    Bynamic Production Capacity: "), powerConnection->GetPowerInfo()->GetDynamicProductionCapacity());
			}

			auto producedPower = powerConnection->GetPowerInfo()->GetBaseProduction();
			if (!producedPower)
			{
				//producedPower = generator->CalcPowerProductionCapacityForPotential(generator->GetPendingPotential());
				producedPower = powerConnection->GetPowerInfo()->GetDynamicProductionCapacity();
			}

			if (!generator->IsProductionPaused() && producedPower && buildDescriptor && includePowerDetails)
			{
				// if (generator && inheritsFromClass(nextBuilding, TEXT("/Script/RefinedPower.RPMPPlatform")))
				// {
				// 	auto generatorPlacement = FReflectionHelper::GetObjectPropertyValue<URPMPPlacementComponent>(nextBuilding, TEXT("GeneratorPlacement"));
				// 	if (!generatorPlacement)
				// 	{
				// 		continue;
				// 	}
				// 	
				// 	auto attachedBuilding = Cast<AFGBuildable>(generatorPlacement->mAttachedBuilding);
				// 	if (!generatorPlacement->mAttachedBuilding)
				// 	{
				// 		continue;
				// 	}
				// 	
				// 	auto maxPowerOutput = FReflectionHelper::GetPropertyValue<UFloatProperty>(attachedBuilding, TEXT("mMaxPowerOutput"));
				// 	
				// 	buildingDetails[attachedBuilding->GetBuiltWithDescriptor()]
				// 		[maxPowerOutput]
				// 		[100]++;
				//
				// 	dumpUnknownClass(nextBuilding);
				// } else {

				buildingDetails[buildDescriptor]
					[producedPower]
					[int(0.5 + generator->GetPendingPotential() * 100)]++;

				// }
			}
		}
		else if (factory)
		{
			if (!factory->IsProductionPaused() && factory->IsConfigured() && factory->GetDefaultProducingPowerConsumption())
			{
				if (FPowerCheckerModule::logInfoEnabled)
				{
					SML::Logging::info(*getTimeStamp(), TEXT("    Power Comsumption: "), factory->GetProducingPowerConsumption());
					SML::Logging::info(*getTimeStamp(), TEXT("    Default Power Comsumption: "), factory->GetDefaultProducingPowerConsumption());
					SML::Logging::info(*getTimeStamp(), TEXT("    Pending Potential: "), factory->GetPendingPotential());
					SML::Logging::info(
						*getTimeStamp(),
						TEXT("    Producing Power Consumption For Potential: "),
						factory->CalcProducingPowerConsumptionForPotential(factory->GetPendingPotential())
						);
					SML::Logging::info(*getTimeStamp(), TEXT("    Maximum Target Consumption: "), powerConnection->GetPowerInfo()->GetMaximumTargetConsumption());
				}

				totalMaximumPotential += factory->CalcProducingPowerConsumptionForPotential(factory->GetPendingPotential());

				if (buildDescriptor && includePowerDetails)
				{
					buildingDetails[buildDescriptor]
						[-factory->CalcProducingPowerConsumptionForPotential(factory->GetPendingPotential())]
						[int(0.5 + factory->GetPendingPotential() * 100)]++;
				}
			}
		}
		else if (dropPod)
		{
			// dumpUnknownClass(dropPod);
			// dumpUnknownClass(powerConnection);
			// dumpUnknownClass(powerConnection->GetPowerInfo());
			
			if (powerConnection->GetPowerInfo()->GetTargetConsumption())
			{
				totalMaximumPotential += powerConnection->GetPowerInfo()->GetTargetConsumption();
			}
		}
		else
		{
			// dumpUnknownClass(nextActor);
		}
	}

	if (includePowerDetails)
	{
		for (auto itBuilding = buildingDetails.begin(); itBuilding != buildingDetails.end(); itBuilding++)
		{
			for (auto itPower = itBuilding->second.begin(); itPower != itBuilding->second.end(); itPower++)
			{
				for (auto itClock = itPower->second.begin(); itClock != itPower->second.end(); itClock++)
				{
					FPowerDetail powerDetail;
					powerDetail.buildingType = itBuilding->first;
					powerDetail.powerPerBuilding = itPower->first;
					powerDetail.potential = itClock->first;
					powerDetail.amount = itClock->second;

					outPowerDetails.Add(powerDetail);
				}
			}
		}

		outPowerDetails.Sort(
			[](const FPowerDetail& x, const FPowerDetail& y)
			{
				float order = (x.powerPerBuilding > 0 ? 0 : 1) - (y.powerPerBuilding > 0 ? 0 : 1);

				if (order == 0)
				{
					order = abs(x.powerPerBuilding) - abs(y.powerPerBuilding);
				}

				if (order == 0)
				{
					order = UFGItemDescriptor::GetItemName(x.buildingType).CompareTo(UFGItemDescriptor::GetItemName(y.buildingType));
				}

				if (order == 0)
				{
					order = x.potential - y.potential;
				}

				return order < 0;
			}
			);
	}
}

bool APowerCheckerLogic::IsLogInfoEnabled()
{
	return FPowerCheckerModule::logInfoEnabled;
}

float APowerCheckerLogic::GetMaximumPlayerDistance()
{
	return FPowerCheckerModule::maximumPlayerDistance;
}

float APowerCheckerLogic::GetSpareLimit()
{
	return FPowerCheckerModule::spareLimit;
}

bool APowerCheckerLogic::inheritsFromClass(AActor* owner, const FString& className)
{
	for (auto cls = owner->GetClass()->GetSuperClass(); cls && cls != AActor::StaticClass(); cls = cls->GetSuperClass())
	{
		if (cls->GetPathName() == className)
		{
			return true;
		}
	}

	return false;
}

void APowerCheckerLogic::dumpUnknownClass(UObject* obj)
{
	if (FPowerCheckerModule::logInfoEnabled)
	{
		SML::Logging::info(*getTimeStamp(), TEXT("Unknown Class "), *obj->GetClass()->GetPathName());

		for (auto cls = obj->GetClass()->GetSuperClass(); cls && cls != AActor::StaticClass(); cls = cls->GetSuperClass())
		{
			SML::Logging::info(*getTimeStamp(), TEXT("    - Super: "), *cls->GetPathName());
		}

		for (TFieldIterator<UProperty> property(obj->GetClass()); property; ++property)
		{
			SML::Logging::info(
				*getTimeStamp(),
				TEXT("    - "),
				*property->GetName(),
				TEXT(" ("),
				*property->GetCPPType(),
				TEXT(" / "),
				*property->GetClass()->GetPathName(),
				TEXT(")")
				);

			auto floatProperty = Cast<UFloatProperty>(*property);
			if (floatProperty)
			{
				SML::Logging::info(*getTimeStamp(), TEXT("        = "), floatProperty->GetPropertyValue_InContainer(obj));
			}

			auto intProperty = Cast<UIntProperty>(*property);
			if (intProperty)
			{
				SML::Logging::info(*getTimeStamp(), TEXT("        = "), intProperty->GetPropertyValue_InContainer(obj));
			}

			auto boolProperty = Cast<UBoolProperty>(*property);
			if (boolProperty)
			{
				SML::Logging::info(*getTimeStamp(), TEXT("        = "), boolProperty->GetPropertyValue_InContainer(obj) ? TEXT("true") : TEXT("false"));
			}

			auto structProperty = Cast<UStructProperty>(*property);
			if (structProperty && property->GetName() == TEXT("mFactoryTickFunction"))
			{
				auto factoryTick = structProperty->ContainerPtrToValuePtr<FFactoryTickFunction>(obj);
				if (factoryTick)
				{
					SML::Logging::info(*getTimeStamp(), TEXT("    - Tick Interval = "), factoryTick->TickInterval);
				}
			}

			auto strProperty = Cast<UStrProperty>(*property);
			if (strProperty)
			{
				SML::Logging::info(*getTimeStamp(), TEXT("        = "), *strProperty->GetPropertyValue_InContainer(obj));
			}
		}
	}
}

void APowerCheckerLogic::removeTeleporter(AActor* actor, EEndPlayReason::Type reason)
{
	FScopeLock ScopeLock(&eclCritical);
	allTeleporters.Remove(Cast<AFGBuildable>(actor));

	actor->OnEndPlay.Remove(removeTeleporterDelegate);
}

bool APowerCheckerLogic::IsValidBuildable(AFGBuildable* newBuildable)
{
	if (!newBuildable)
	{
		return false;
	}

	if (newBuildable->GetClass()->GetPathName() == TEXT("/Game/StorageTeleporter/Buildables/ItemTeleporter/ItemTeleporter_Build.ItemTeleporter_Build_C"))
	{
		addTeleporter(newBuildable);

		return true;
	}

	return false;
}

void APowerCheckerLogic::addTeleporter(AFGBuildable* teleporter)
{
	FScopeLock ScopeLock(&eclCritical);
	allTeleporters.Add(teleporter);

	teleporter->OnEndPlay.Add(removeTeleporterDelegate);
}
