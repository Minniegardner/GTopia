#include "ChemsynthComponent.h"

#include "Algorithm/ChemsynthAlgorithm.h"
#include "../World/World.h"
#include "../World/WorldManager.h"

namespace {
constexpr uint64 kChemsynthTickIntervalMS = 1500;  // Interval managed by algorithm internally

bool s_enabled = true;
}

void ChemsynthComponent::SetEnabled(bool enabled)
{
    s_enabled = enabled;
}

void ChemsynthComponent::OnTick()
{
    if(!s_enabled) {
        return;
    }

    WorldManager* pWorldManager = GetWorldManager();
    if(!pWorldManager) {
        return;
    }

    pWorldManager->ForEachWorld([](World* pWorld) {
        if(!pWorld || pWorld->IsWaitingForClose() || pWorld->GetPlayerCount() < 1) {
            return;
        }

        ChemsynthAlgorithm::UpdateWorldChemsynth(pWorld);
    });
}
