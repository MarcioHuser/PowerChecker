#include "PowerCheckerLogic.h"


#include "FGBuildableGenerator.h"
#include "FGBuildableRailroadStation.h"
#include "FGCircuitSubsystem.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPowerCircuit.h"
#include "FGRailroadSubsystem.h"
#include "FGTrainStationIdentifier.h"
#include "PowerCheckerModule.h"

#include "util/Logging.h"

#include "Util/Optimize.h"

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

float APowerCheckerLogic::GetMaximumPotential(UFGCircuitConnectionComponent* powerConnection)
{
    TSet<AFGBuildable*> seenActors;
    TSet<UFGCircuitConnectionComponent*> pendingConnections;

    auto nextBuilding = Cast<AFGBuildable>(powerConnection->GetOwner());

    if (nextBuilding)
    {
        pendingConnections.Add(powerConnection);
    }

    auto railroadSubsystem = AFGRailroadSubsystem::Get(nextBuilding->GetWorld());
    //auto circuitSubsystem = AFGCircuitSubsystem::Get(nextBuilding->GetWorld());

    //auto powerCircuit = circuitSubsystem->FindCircuit<UFGPowerCircuit>(powerConnection->GetCircuitID());

    float totalMaximumPotential = 0;

    bool teleporterFound = false;

    while (pendingConnections.Num())
    {
        powerConnection = *pendingConnections.begin();
        pendingConnections.Remove(powerConnection);

        nextBuilding = Cast<AFGBuildable>(powerConnection->GetOwner());
        if (!nextBuilding)
        {
            continue;
        }

        seenActors.Add(nextBuilding);

        TArray<UFGPowerConnectionComponent*> powerConnections;
        nextBuilding->GetComponents<UFGPowerConnectionComponent>(powerConnections);

        TArray<UFGCircuitConnectionComponent*> connections;

        for (auto currentPowerConnection : powerConnections)
        {
            currentPowerConnection->GetConnections(connections);
            currentPowerConnection->GetHiddenConnections(connections);
        }

        auto trainStation = Cast<AFGBuildableRailroadStation>(nextBuilding);

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
            }
        }

        SML::Logging::info(
            *getTimeStamp(),
            TEXT(" PowerChecker: "),
            *nextBuilding->GetName(),
            TEXT(" / "),
            *nextBuilding->GetClass()->GetPathName()
            );

        for (auto connection : connections)
        {
            auto connectedBuilding = Cast<AFGBuildable>(connection->GetOwner());
            if (!connectedBuilding || seenActors.Contains(connectedBuilding) || pendingConnections.Contains(connection))
            {
                continue;
            }

            pendingConnections.Add(connection);
        }

        auto generator = Cast<AFGBuildableGenerator>(nextBuilding);
        auto factory = Cast<AFGBuildableFactory>(nextBuilding);

        if (nextBuilding->GetClass()->GetPathName() == TEXT("/Game/Teleporter/buildable/Build_Teleporteur.Build_Teleporteur_C"))
        {
            dumpUnknownClass(nextBuilding);
        }
        else if (nextBuilding->GetClass()->GetPathName() == TEXT("/Game/StorageTeleporter/Buildables/Hub/Build_STHub.Build_STHub_C"))
        {
            if (!teleporterFound)
            {
                teleporterFound = true;

                FScopeLock ScopeLock(&singleton->eclCritical);

                totalMaximumPotential += 1;

                for (auto teleporter : singleton->allTeleporters)
                {
                    TArray<UFGFactoryConnectionComponent*> teleportConnections;
                    teleporter->GetComponents(teleportConnections);

                    for (auto connection : teleportConnections)
                    {
                        if (connection->IsConnected() && connection->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
                        {
                            SML::Logging::info(*getTimeStamp(), TEXT("    Teleporter "), *teleporter->GetPathName(), TEXT(" has output connection"));

                            totalMaximumPotential += 10;
                        }
                    }
                }
            }
            // dumpUnknownClass(nextBuilding);
        }
        else if (generator)
        {
        }
        else if (factory)
        {
            if (!factory->IsProductionPaused() && factory->IsConfigured())
            {
                SML::Logging::info(*getTimeStamp(), TEXT("    Default Power Comsumption: "), factory->GetDefaultProducingPowerConsumption());
                SML::Logging::info(*getTimeStamp(), TEXT("    Pending Potential: "), factory->GetPendingPotential());
                SML::Logging::info(
                    *getTimeStamp(),
                    TEXT("    Producing Power Consumption For Potential: "),
                    factory->CalcProducingPowerConsumptionForPotential(factory->GetPendingPotential())
                    );

                totalMaximumPotential += factory->CalcProducingPowerConsumptionForPotential(factory->GetPendingPotential());
            }
        }
        else
        {
            dumpUnknownClass(nextBuilding);
        }
    }

    return totalMaximumPotential;
}

bool APowerCheckerLogic::IsLogInfoEnabled()
{
    return FPowerCheckerModule::logInfoEnabled;
}

void APowerCheckerLogic::dumpUnknownClass(AActor* owner)
{
    if (FPowerCheckerModule::logInfoEnabled)
    {
        SML::Logging::info(*getTimeStamp(), TEXT("Unknown Class "), *owner->GetClass()->GetPathName());

        for (auto cls = owner->GetClass()->GetSuperClass(); cls && cls != AActor::StaticClass(); cls = cls->GetSuperClass())
        {
            SML::Logging::info(*getTimeStamp(), TEXT("    - Super: "), *cls->GetPathName());
        }

        for (TFieldIterator<UProperty> property(owner->GetClass()); property; ++property)
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
                SML::Logging::info(*getTimeStamp(), TEXT("        = "), floatProperty->GetPropertyValue_InContainer(owner));
            }

            auto intProperty = Cast<UIntProperty>(*property);
            if (intProperty)
            {
                SML::Logging::info(*getTimeStamp(), TEXT("        = "), intProperty->GetPropertyValue_InContainer(owner));
            }

            auto boolProperty = Cast<UBoolProperty>(*property);
            if (boolProperty)
            {
                SML::Logging::info(*getTimeStamp(), TEXT("        = "), boolProperty->GetPropertyValue_InContainer(owner) ? TEXT("true") : TEXT("false"));
            }

            auto structProperty = Cast<UStructProperty>(*property);
            if (structProperty && property->GetName() == TEXT("mFactoryTickFunction"))
            {
                auto factoryTick = structProperty->ContainerPtrToValuePtr<FFactoryTickFunction>(owner);
                if (factoryTick)
                {
                    SML::Logging::info(*getTimeStamp(), TEXT("    - Tick Interval = "), factoryTick->TickInterval);
                }
            }

            auto strProperty = Cast<UStrProperty>(*property);
            if (strProperty)
            {
                SML::Logging::info(*getTimeStamp(), TEXT("        = "), *strProperty->GetPropertyValue_InContainer(owner));
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
