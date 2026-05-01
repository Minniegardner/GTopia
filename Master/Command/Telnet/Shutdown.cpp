#include "Shutdown.h"
#include "../../Context.h"
#include "../../World/WorldManager.h"
#include "../../Player/PlayerManager.h"

const TelnetCommandInfo& Shutdown::GetInfo()
{
    static TelnetCommandInfo info =
    {
        "/shutdown",
        "Save all data and stop the master server",
        5,
        {
            CompileTimeHashString("shutdown")
        }
    };

    return info;
}

void Shutdown::Execute(TelnetClient* pNetClient, std::vector<string>& args)
{
    if(!pNetClient || !TelnetCommandBase<Shutdown>::CheckPerm(pNetClient)) return;

    pNetClient->SendMessage("Saving players and worlds, then stopping server...", true);
    GetContext()->Stop();
}
