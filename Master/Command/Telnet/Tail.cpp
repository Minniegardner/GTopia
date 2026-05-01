#include "Tail.h"
#include "../../Context.h"

const TelnetCommandInfo& Tail::GetInfo()
{
    static TelnetCommandInfo info =
    {
        "/tail",
        "Toggle tailing telnet logs for your session",
        2,
        {
            CompileTimeHashString("tail")
        }
    };

    return info;
}

void Tail::Execute(TelnetClient* pNetClient, std::vector<string>& args)
{
    if(!pNetClient || args.empty() || !TelnetCommandBase<Tail>::CheckPerm(pNetClient)) return;

    bool enabled = pNetClient->IsTailEnabled();
    pNetClient->SetTailEnabled(!enabled);

    pNetClient->SendMessage(string("Telnet tail ") + (enabled ? "disabled" : "enabled"), true);
}
