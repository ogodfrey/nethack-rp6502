/* RP6502: functions for the condition macros redirected by rp6502mac.h. */
#define RP6502_MACRO_BODIES
#include "hack.h"

boolean
rpm_Fire_resistance()
{
    return (Fire_resistance) ? TRUE : FALSE;
}

boolean
rpm_Cold_resistance()
{
    return (Cold_resistance) ? TRUE : FALSE;
}

boolean
rpm_Sleep_resistance()
{
    return (Sleep_resistance) ? TRUE : FALSE;
}

boolean
rpm_Disint_resistance()
{
    return (Disint_resistance) ? TRUE : FALSE;
}

boolean
rpm_Shock_resistance()
{
    return (Shock_resistance) ? TRUE : FALSE;
}

boolean
rpm_Poison_resistance()
{
    return (Poison_resistance) ? TRUE : FALSE;
}

boolean
rpm_Drain_resistance()
{
    return (Drain_resistance) ? TRUE : FALSE;
}

boolean
rpm_Antimagic()
{
    return (Antimagic) ? TRUE : FALSE;
}

boolean
rpm_Acid_resistance()
{
    return (Acid_resistance) ? TRUE : FALSE;
}

boolean
rpm_Stone_resistance()
{
    return (Stone_resistance) ? TRUE : FALSE;
}

boolean
rpm_Sick_resistance()
{
    return (Sick_resistance) ? TRUE : FALSE;
}

boolean
rpm_Blindfolded()
{
    return (Blindfolded) ? TRUE : FALSE;
}

boolean
rpm_Blind()
{
    return (Blind) ? TRUE : FALSE;
}

boolean
rpm_Blindfolded_only()
{
    return (Blindfolded_only) ? TRUE : FALSE;
}

boolean
rpm_Halluc_resistance()
{
    return (Halluc_resistance) ? TRUE : FALSE;
}

boolean
rpm_Hallucination()
{
    return (Hallucination) ? TRUE : FALSE;
}

boolean
rpm_Deaf()
{
    return (Deaf) ? TRUE : FALSE;
}

boolean
rpm_Fumbling()
{
    return (Fumbling) ? TRUE : FALSE;
}

boolean
rpm_Wounded_legs()
{
    return (Wounded_legs) ? TRUE : FALSE;
}

boolean
rpm_Sleepy()
{
    return (Sleepy) ? TRUE : FALSE;
}

boolean
rpm_Hunger()
{
    return (Hunger) ? TRUE : FALSE;
}

boolean
rpm_See_invisible()
{
    return (See_invisible) ? TRUE : FALSE;
}

boolean
rpm_Blind_telepat()
{
    return (Blind_telepat) ? TRUE : FALSE;
}

boolean
rpm_Warning()
{
    return (Warning) ? TRUE : FALSE;
}

boolean
rpm_Warn_of_mon()
{
    return (Warn_of_mon) ? TRUE : FALSE;
}

boolean
rpm_Searching()
{
    return (Searching) ? TRUE : FALSE;
}

boolean
rpm_Clairvoyant()
{
    return (Clairvoyant) ? TRUE : FALSE;
}

boolean
rpm_Infravision()
{
    return (Infravision) ? TRUE : FALSE;
}

boolean
rpm_Detect_monsters()
{
    return (Detect_monsters) ? TRUE : FALSE;
}

boolean
rpm_Invis()
{
    return (Invis) ? TRUE : FALSE;
}

boolean
rpm_Invisible()
{
    return (Invisible) ? TRUE : FALSE;
}

boolean
rpm_Stealth()
{
    return (Stealth) ? TRUE : FALSE;
}

boolean
rpm_Aggravate_monster()
{
    return (Aggravate_monster) ? TRUE : FALSE;
}

boolean
rpm_Conflict()
{
    return (Conflict) ? TRUE : FALSE;
}

boolean
rpm_Jumping()
{
    return (Jumping) ? TRUE : FALSE;
}

boolean
rpm_Teleportation()
{
    return (Teleportation) ? TRUE : FALSE;
}

boolean
rpm_Teleport_control()
{
    return (Teleport_control) ? TRUE : FALSE;
}

boolean
rpm_Levitation()
{
    return (Levitation) ? TRUE : FALSE;
}

boolean
rpm_Lev_at_will()
{
    return (Lev_at_will) ? TRUE : FALSE;
}

boolean
rpm_Flying()
{
    return (Flying) ? TRUE : FALSE;
}

boolean
rpm_Wwalking()
{
    return (Wwalking) ? TRUE : FALSE;
}

boolean
rpm_Swimming()
{
    return (Swimming) ? TRUE : FALSE;
}

boolean
rpm_Amphibious()
{
    return (Amphibious) ? TRUE : FALSE;
}

boolean
rpm_Breathless()
{
    return (Breathless) ? TRUE : FALSE;
}

boolean
rpm_Passes_walls()
{
    return (Passes_walls) ? TRUE : FALSE;
}

boolean
rpm_Slow_digestion()
{
    return (Slow_digestion) ? TRUE : FALSE;
}

boolean
rpm_Half_spell_damage()
{
    return (Half_spell_damage) ? TRUE : FALSE;
}

boolean
rpm_Half_physical_damage()
{
    return (Half_physical_damage) ? TRUE : FALSE;
}

boolean
rpm_Regeneration()
{
    return (Regeneration) ? TRUE : FALSE;
}

boolean
rpm_Energy_regeneration()
{
    return (Energy_regeneration) ? TRUE : FALSE;
}

boolean
rpm_Protection()
{
    return (Protection) ? TRUE : FALSE;
}

boolean
rpm_Protection_from_shape_changers()
{
    return (Protection_from_shape_changers) ? TRUE : FALSE;
}

boolean
rpm_Polymorph()
{
    return (Polymorph) ? TRUE : FALSE;
}

boolean
rpm_Polymorph_control()
{
    return (Polymorph_control) ? TRUE : FALSE;
}

boolean
rpm_Unchanging()
{
    return (Unchanging) ? TRUE : FALSE;
}

boolean
rpm_Fast()
{
    return (Fast) ? TRUE : FALSE;
}

boolean
rpm_Very_fast()
{
    return (Very_fast) ? TRUE : FALSE;
}

boolean
rpm_Reflecting()
{
    return (Reflecting) ? TRUE : FALSE;
}

boolean
rpm_Unaware()
{
    return (Unaware) ? TRUE : FALSE;
}

boolean
rpm_Hate_silver()
{
    return (Hate_silver) ? TRUE : FALSE;
}

boolean
rpm_tp_sensemon(mtmp)
struct monst *mtmp;
{
    return (tp_sensemon(mtmp)) ? TRUE : FALSE;
}

boolean
rpm_sensemon(mtmp)
struct monst *mtmp;
{
    return (sensemon(mtmp)) ? TRUE : FALSE;
}

boolean
rpm_mon_warning(mtmp)
struct monst *mtmp;
{
    return (mon_warning(mtmp)) ? TRUE : FALSE;
}

boolean
rpm_mon_visible(mtmp)
struct monst *mtmp;
{
    return (mon_visible(mtmp)) ? TRUE : FALSE;
}

boolean
rpm_see_with_infrared(mtmp)
struct monst *mtmp;
{
    return (see_with_infrared(mtmp)) ? TRUE : FALSE;
}

boolean
rpm_canseemon(mtmp)
struct monst *mtmp;
{
    return (canseemon(mtmp)) ? TRUE : FALSE;
}

boolean
rpm_canspotmon(mtmp)
struct monst *mtmp;
{
    return (canspotmon(mtmp)) ? TRUE : FALSE;
}

boolean
rpm_knowninvisible(mtmp)
struct monst *mtmp;
{
    return (knowninvisible(mtmp)) ? TRUE : FALSE;
}

boolean
rpm_is_safepet(mtmp)
struct monst *mtmp;
{
    return (is_safepet(mtmp)) ? TRUE : FALSE;
}

boolean
rpm_canseeself()
{
    return (canseeself()) ? TRUE : FALSE;
}

boolean
rpm_senseself()
{
    return (senseself()) ? TRUE : FALSE;
}

boolean
rpm_canspotself()
{
    return (canspotself()) ? TRUE : FALSE;
}
