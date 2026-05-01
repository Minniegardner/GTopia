#include "Load.h"
#include "../../Context.h"
#include "../../Player/RoleManager.h"

const TelnetCommandInfo& Load::GetInfo()
{
    static TelnetCommandInfo info =
    {
        "/load",
        "Reload basic config files (roles, config, telnet)",
        4,
        {
            CompileTimeHashString("load")
        }
    };

    return info;
}

void Load::Execute(TelnetClient* pNetClient, std::vector<string>& args)
{
    if(!pNetClient || !TelnetCommandBase<Load>::CheckPerm(pNetClient)) return;

    string base = GetProgramPath();
    bool ok = true;

    if(!GetContext()->GetGameConfig()->LoadConfig(base + "/config.txt")) {
        pNetClient->SendMessage("Failed to reload config.txt", true);
        ok = false;
    }

    if(!GetRoleManager()->Load(base + "/roles.txt")) {
        pNetClient->SendMessage("Failed to reload roles.txt", true);
        ok = false;
    }

    if(!GetTelnetServer()->LoadTelnetConfigFromFile(base + "/telnet_config.txt")) {
        pNetClient->SendMessage("Failed to reload telnet_config.txt", true);
        ok = false;
    }

    if(ok) pNetClient->SendMessage("Reload completed.", true);
}
