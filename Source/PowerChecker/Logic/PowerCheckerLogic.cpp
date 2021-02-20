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
TSubclassOf<class UFGItemDescriptor> APowerCheckerLogic::dropPodStub = nullptr;

APowerCheckerLogic::APowerCheckerLogic()
{
}

void APowerCheckerLogic::Initialize(TSubclassOf<UFGItemDescriptor> in_dropPodStub)
{
	singleton = this;

	auto subsystem = AFGBuildableSubsystem::Get(this);

	dropPodStub = in_dropPodStub;

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

void APowerCheckerLogic::GetMaximumPotential
(
	UFGPowerConnectionComponent* powerConnection,
	float& totalMaximumPotential,
	bool includePaused,
	bool includeOutOfFuel,
	PowerCheckerFilterType filterType
)
{
	TArray<FPowerDetail> powerDetails;

	GetMaximumPotentialWithDetails(
		powerConnection,
		totalMaximumPotential,
		includePaused,
		includeOutOfFuel,
		false,
		powerDetails,
		filterType
		);
}

void APowerCheckerLogic::GetMaximumPotentialWithDetails
(
	UFGPowerConnectionComponent* powerConnection,
	float& totalMaximumPotential,
	bool includePaused,
	bool includeOutOfFuel,
	bool includePowerDetails,
	TArray<FPowerDetail>& outPowerDetails,
	PowerCheckerFilterType filterType
)
{
	TSet<AActor*> seenActors;
	std::map<TSubclassOf<UFGItemDescriptor>, std::map<float, std::map<int, FPowerDetail>>> buildingDetails;

	auto railroadSubsystem = AFGRailroadSubsystem::Get(powerConnection->GetWorld());
	auto powerCircuit = powerConnection->GetPowerCircuit();

	totalMaximumPotential = 0;

	if (!powerCircuit)
	{
		return;
	}

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

		auto generator = Cast<AFGBuildableGenerator>(nextActor);
		auto factory = Cast<AFGBuildableFactory>(nextActor);

		if (FPowerCheckerModule::logInfoEnabled)
		{
			SML::Logging::info(
				*getTimeStamp(),
				TEXT(" PowerChecker: "),
				*nextActor->GetName(),
				TEXT(" / "),
				*className
				);

			SML::Logging::info(*getTimeStamp(), TEXT("    Base Production: "), powerInfo->GetBaseProduction());
			SML::Logging::info(*getTimeStamp(), TEXT("    Dynamic Production Capacity: "), powerInfo->GetDynamicProductionCapacity());
			SML::Logging::info(*getTimeStamp(), TEXT("    Target Consumption: "), powerInfo->GetTargetConsumption());
		}

		/*if (className == TEXT("/Game/StorageTeleporter/Buildables/Hub/Build_STHub.Build_STHub_C"))
		{
			if (filterType != PowerCheckerFilterType::Any)
			{
				continue;
			}

			totalMaximumPotential += powerInfo->GetTargetConsumption();

			if (FPowerCheckerModule::logInfoEnabled)
			{
				SML::Logging::info(*getTimeStamp(), TEXT("    Item Teleporter Power Comsumption: "), powerInfo->GetTargetConsumption());
			}

			if (buildDescriptor && includePowerDetails)
			{
				buildingDetails[buildDescriptor]
					[-powerInfo->GetTargetConsumption()]
					[100].amount++;
			}
		}
		else*/
		if (auto locomotive = Cast<AFGLocomotive>(nextActor))
		{
			if (filterType != PowerCheckerFilterType::Any)
			{
				continue;
			}

			totalMaximumPotential += locomotive->GetPowerInfo()->GetMaximumTargetConsumption();

			if (includePowerDetails)
			{
				buildingDetails[UFGRecipe::GetProducts(locomotive->GetBuiltWithRecipe())[0].ItemClass]
					[-locomotive->GetPowerInfo()->GetMaximumTargetConsumption()]
					[100].amount++;
			}
		}
		else if (auto dropPod = Cast<AFGDropPod>(nextActor))
		{
			if (filterType != PowerCheckerFilterType::Any)
			{
				continue;
			}

			// dumpUnknownClass(dropPod);
			// dumpUnknownClass(powerInfo);

			if (powerInfo->GetTargetConsumption())
			{
				if (FPowerCheckerModule::logInfoEnabled)
				{
					SML::Logging::info(*getTimeStamp(), TEXT("    Target Consumption: "), powerInfo->GetTargetConsumption());
				}

				totalMaximumPotential += powerInfo->GetTargetConsumption();

				if (includePowerDetails && dropPodStub)
				{
					buildingDetails[dropPodStub]
						[-powerInfo->GetTargetConsumption()]
						[100].amount++;
				}
			}
		}
		else if (generator || powerInfo->GetDynamicProductionCapacity())
		{
			if (FPowerCheckerModule::logInfoEnabled)
			{
				SML::Logging::info(*getTimeStamp(), TEXT("    Can Produce: "), (generator && generator->CanProduce()) ? TEXT("true") : TEXT("false"));
				SML::Logging::info(*getTimeStamp(), TEXT("    Load: "), generator ? generator->GetLoadPercentage() : 0, TEXT("%"));
				SML::Logging::info(*getTimeStamp(), TEXT("    Power Production: "), generator ? generator->GetPowerProductionCapacity() : 0);
				SML::Logging::info(*getTimeStamp(), TEXT("    Default Power Production: "), generator ? generator->GetDefaultPowerProductionCapacity() : 0);
				SML::Logging::info(*getTimeStamp(), TEXT("    Pending Potential: "), generator ? generator->GetPendingPotential() : 0);
				SML::Logging::info(
					*getTimeStamp(),
					TEXT("    Producing Power Production Capacity For Potential: "),
					generator ? generator->CalcPowerProductionCapacityForPotential(generator->GetPendingPotential()) : 0
					);
			}

			auto producedPower = generator ? generator->GetPowerProductionCapacity() : 0;
			if (!producedPower)
			{
				producedPower = powerInfo->GetBaseProduction();
				if (!producedPower)
				{
					//producedPower = generator->CalcPowerProductionCapacityForPotential(generator->GetPendingPotential());
					producedPower = powerInfo->GetDynamicProductionCapacity();
				}
			}

			auto includeDetails = producedPower && buildDescriptor && includePowerDetails;

			if (includeDetails)
			{
				switch (filterType)
				{
				case PowerCheckerFilterType::PausedOnly:
					includeDetails &= generator && generator->IsProductionPaused();
					break;

				case PowerCheckerFilterType::OutOfFuelOnly:
					includeDetails &= generator && !generator->CanStartPowerProduction() && !powerInfo->GetBaseProduction();
					break;

				default:
					includeDetails &= (includePaused || generator && !generator->IsProductionPaused()) &&
						(includeOutOfFuel || generator && generator->CanStartPowerProduction() || powerInfo->GetBaseProduction());
					break;
				}
			}

			if (includeDetails)
			{
				auto& detail = buildingDetails[buildDescriptor]
					[producedPower]
					[int(0.5 + (generator ? generator->GetPendingPotential() : 1) * 100)];

				detail.amount++;

				if (generator)
				{
					detail.factories.Add(generator);
				}
			}
		}
		else if (factory || powerInfo->GetTargetConsumption())
		{
			if (!factory || factory->IsConfigured() && factory->GetProducingPowerConsumption())
			{
				if (FPowerCheckerModule::logInfoEnabled)
				{
					SML::Logging::info(*getTimeStamp(), TEXT("    Power Comsumption: "), factory ? factory->GetProducingPowerConsumption() : 0);
					SML::Logging::info(*getTimeStamp(), TEXT("    Default Power Comsumption: "), factory ? factory->GetDefaultProducingPowerConsumption() : 0);
					SML::Logging::info(*getTimeStamp(), TEXT("    Pending Potential: "), factory ? factory->GetPendingPotential() : 0);
					SML::Logging::info(
						*getTimeStamp(),
						TEXT("    Producing Power Consumption For Potential: "),
						factory ? factory->CalcProducingPowerConsumptionForPotential(factory->GetPendingPotential()) : 0
						);
					SML::Logging::info(*getTimeStamp(), TEXT("    Maximum Target Consumption: "), powerInfo->GetMaximumTargetConsumption());
				}

				auto maxComsumption = FMath::Max(
					factory ? factory->GetProducingPowerConsumption() : 0,
					powerInfo->GetTargetConsumption()
					);

				if (includePaused || !factory || !factory->IsProductionPaused())
				{
					totalMaximumPotential += maxComsumption;
				}

				auto includeDetails = (includePaused || !factory || factory && !factory->IsProductionPaused()) && buildDescriptor && includePowerDetails;

				if (includeDetails)
				{
					switch (filterType)
					{
					case PowerCheckerFilterType::PausedOnly:
						includeDetails &= factory && factory->IsProductionPaused();
						break;

					case PowerCheckerFilterType::OutOfFuelOnly:
						includeDetails = false;
						break;

					default:
						break;
					}
				}

				if (includeDetails)
				{
					auto& detail = buildingDetails[buildDescriptor]
						[-maxComsumption]
						[int(0.5 + (factory ? factory->GetPendingPotential() : 1) * 100)];

					detail.amount++;
					if (factory)
					{
						detail.factories.Add(factory);
					}
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
		auto owner = powerConnection->GetOwner();

		for (auto itBuilding = buildingDetails.begin(); itBuilding != buildingDetails.end(); itBuilding++)
		{
			for (auto itPower = itBuilding->second.begin(); itPower != itBuilding->second.end(); itPower++)
			{
				for (auto itClock = itPower->second.begin(); itClock != itPower->second.end(); itClock++)
				{
					FPowerDetail powerDetail = itClock->second;
					powerDetail.buildingType = itBuilding->first;
					powerDetail.powerPerBuilding = itPower->first;
					powerDetail.potential = itClock->first;

					outPowerDetails.Add(powerDetail);

					powerDetail.factories.Sort(
						[owner](const AFGBuildableFactory& x, const AFGBuildableFactory& y)
						{
							float order = owner->GetDistanceTo(&x) - owner->GetDistanceTo(&y);

							if (order == 0)
							{
								order = x.GetName().Compare(y.GetName());
							}

							return order < 0;
						}
						);
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
