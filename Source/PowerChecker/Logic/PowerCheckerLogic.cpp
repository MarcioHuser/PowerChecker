#include "PowerCheckerLogic.h"
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


#include "FGBuildablePowerPole.h"
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
		removePowerCheckerDelegate.BindDynamic(this, &APowerCheckerLogic::removePowerChecker);

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

void APowerCheckerLogic::GetMaximumPotentialWithDetails(UFGPowerConnectionComponent* powerConnection, float& totalMaximumPotential, bool includePowerDetails, TArray<FPowerDetail>& outPowerDetails)
{
	TSet<AActor*> seenActors;
	std::map<TSubclassOf<UFGItemDescriptor>, std::map<float, std::map<int, int>>> buildingDetails;

	auto railroadSubsystem = AFGRailroadSubsystem::Get(powerConnection->GetWorld());
	auto powerCircuit = powerConnection->GetPowerCircuit();

	totalMaximumPotential = 0;

	if (!powerCircuit)
	{
		return;
	}

	bool teleporterFound = false;

	for (auto powerInfo : powerCircuit->mPowerInfos)
	{
		auto nextActor = powerInfo->GetOwner();
		if (!nextActor || seenActors.Contains(nextActor) || Cast<AFGBuildablePowerPole>(nextActor) || Cast<APowerCheckerBuilding>(nextActor))
		{
			continue;
		}

		seenActors.Add(nextActor);

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

		auto powerIt = FPowerCheckerModule::powerConsumptionMap.find(className);

		if (powerIt != FPowerCheckerModule::powerConsumptionMap.end())
		{
			if (FPowerCheckerModule::logInfoEnabled)
			{
				SML::Logging::info(*getTimeStamp(), TEXT("    Default Power Comsumption: "), powerIt->second);
			}

			if (powerIt->second < 0)
			{
				totalMaximumPotential -= powerIt->second;
			}

			if (buildDescriptor && includePowerDetails)
			{
				auto factory = Cast<AFGBuildableFactory>(nextActor);

				buildingDetails[buildDescriptor]
					[powerIt->second]
					[factory ? factory->GetPendingPotential() : 100]++;
			}
		}
			/*else if (auto trainStation = Cast<AFGBuildableRailroadStation>(nextActor))
			{
				totalMaximumPotential += powerInfo->GetMaximumTargetConsumption();
	
				if (includePowerDetails)
				{
					buildingDetails[trainStation->GetBuiltWithDescriptor()]
						[-powerInfo->GetMaximumTargetConsumption()]
						[100]++;
				}
	
				// Add all connected cargo platforms
	
				for (auto d = 0; d < 2; d++)
				{
					for (auto nextPlatform = trainStation->GetConnectedPlatformInDirectionOf(d);
					     nextPlatform;
					     nextPlatform = nextPlatform->GetConnectedPlatformInDirectionOf(d))
					{
						auto cargoPlatform = Cast<AFGBuildableTrainPlatformCargo>(nextPlatform);
						if (!cargoPlatform || seenActors.Contains(cargoPlatform))
						{
							continue;
						}
	
						seenActors.Add(cargoPlatform);
	
						totalMaximumPotential += cargoPlatform->GetPowerInfo()->GetMaximumTargetConsumption();
	
						if (includePowerDetails)
						{
							buildingDetails[cargoPlatform->GetBuiltWithDescriptor()]
								[-cargoPlatform->GetPowerInfo()->GetMaximumTargetConsumption()]
								[100]++;
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
	
						totalMaximumPotential += locomotive->GetPowerInfo()->GetMaximumTargetConsumption();
	
						if (includePowerDetails)
						{
							buildingDetails[UFGRecipe::GetProducts(locomotive->GetBuiltWithRecipe())[0].ItemClass]
								[-locomotive->GetPowerInfo()->GetMaximumTargetConsumption()]
								[100]++;
						}
					}
				}
			}*/
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
		else if (auto locomotive = Cast<AFGLocomotive>(nextActor))
		{
			totalMaximumPotential += locomotive->GetPowerInfo()->GetMaximumTargetConsumption();

			if (includePowerDetails)
			{
				buildingDetails[UFGRecipe::GetProducts(locomotive->GetBuiltWithRecipe())[0].ItemClass]
					[-locomotive->GetPowerInfo()->GetMaximumTargetConsumption()]
					[100]++;
			}
		}
		else if (auto dropPod = Cast<AFGDropPod>(nextActor))
		{
			// dumpUnknownClass(dropPod);
			// dumpUnknownClass(powerInfo);

			if (powerInfo->GetTargetConsumption())
			{
				totalMaximumPotential += powerInfo->GetTargetConsumption();
			}
		}
		else if (auto generator = Cast<AFGBuildableGenerator>(nextActor))
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
				SML::Logging::info(*getTimeStamp(), TEXT("    Base Production: "), powerInfo->GetBaseProduction());
				SML::Logging::info(*getTimeStamp(), TEXT("    Bynamic Production Capacity: "), powerInfo->GetDynamicProductionCapacity());
			}

			auto producedPower = powerInfo->GetBaseProduction();
			if (!producedPower)
			{
				//producedPower = generator->CalcPowerProductionCapacityForPotential(generator->GetPendingPotential());
				producedPower = powerInfo->GetDynamicProductionCapacity();
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
		else if (auto factory = Cast<AFGBuildableFactory>(nextActor))
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
					SML::Logging::info(*getTimeStamp(), TEXT("    Maximum Target Consumption: "), powerInfo->GetMaximumTargetConsumption());
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
		else
		{
			if (FPowerCheckerModule::logInfoEnabled)
			{
				SML::Logging::info(
                    *getTimeStamp(),
                    TEXT(" PowerChecker: Unknown "),
                    *className
                    );
			}
			
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

bool APowerCheckerLogic::IsValidBuildable(AFGBuildable* newBuildable)
{
	if (!newBuildable)
	{
		return false;
	}

	if (newBuildable->GetClass()->GetPathName() == TEXT("/Game/StorageTeleporter/Buildables/ItemTeleporter/ItemTeleporter_Build.ItemTeleporter_Build_C"))
	{
		addTeleporter(newBuildable);
	}
	else if (auto powerChecker = Cast<APowerCheckerBuilding>(newBuildable))
	{
		addPowerChecker(powerChecker);
	}
	else
	{
		return false;
	}

	return true;
}

void APowerCheckerLogic::addTeleporter(AFGBuildable* teleporter)
{
	FScopeLock ScopeLock(&eclCritical);
	allTeleporters.Add(teleporter);

	teleporter->OnEndPlay.Add(removeTeleporterDelegate);
}

void APowerCheckerLogic::removeTeleporter(AActor* actor, EEndPlayReason::Type reason)
{
	FScopeLock ScopeLock(&eclCritical);
	allTeleporters.Remove(Cast<AFGBuildable>(actor));

	actor->OnEndPlay.Remove(removeTeleporterDelegate);
}

void APowerCheckerLogic::addPowerChecker(APowerCheckerBuilding* powerChecker)
{
	FScopeLock ScopeLock(&eclCritical);
	allPowerCheckers.Add(powerChecker);

	powerChecker->OnEndPlay.Add(removePowerCheckerDelegate);
}

void APowerCheckerLogic::removePowerChecker(AActor* actor, EEndPlayReason::Type reason)
{
	FScopeLock ScopeLock(&eclCritical);
	allPowerCheckers.Remove(Cast<APowerCheckerBuilding>(actor));

	actor->OnEndPlay.Remove(removePowerCheckerDelegate);
}
