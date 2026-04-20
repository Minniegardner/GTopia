#include "GhostComponent.h"

#include "Algorithm/GhostAlgorithm.h"
#include "../World/World.h"
#include "../World/WorldManager.h"

namespace {
constexpr uint64 kGhostTickIntervalMS = 300;  // Interval managed by algorithm internally

bool s_enabled = true;
}

void GhostComponent::SetEnabled(bool enabled)
{
    s_enabled = enabled;
}

void GhostComponent::OnTick()
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

        GhostAlgorithm::UpdateWorldGhosts(pWorld);
    });
}
