#include "PlayMod.h"

PlayMod::PlayMod()
: m_charFlags(0), m_displayItem(0), m_modType(PLAYMOD_TYPE_NONE), m_durationTime(0),
m_punchDamage(0), m_punchPower(0), m_speed(0), m_buildRange(0)
{
}

PlayMod::~PlayMod()
{
}
