#include "GhostComponent.h"

#include "Algorithm/GhostAlgorithm.h"
#include "Utils/Timer.h"
#include "../World/World.h"
#include "../World/WorldManager.h"

namespace {
constexpr uint64 kGhostTickIntervalMS = 300;

bool s_enabled = true;
Timer s_lastTick;
}

void GhostComponent::SetEnabled(bool enabled)
{
    s_enabled = enabled;
    if(enabled) {
        s_lastTick.Reset();
    }
}

void GhostComponent::OnTick()
{
    if(!s_enabled || s_lastTick.GetElapsedTime() < kGhostTickIntervalMS) {
        return;
    }

    s_lastTick.Reset();

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
