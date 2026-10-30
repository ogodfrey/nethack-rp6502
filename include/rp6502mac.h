/* RP6502: out-of-line versions of large condition macros.
 *
 * The property macros of youprop.h test 32-bit fields and the vision
 * macros of display.h expand to long expressions; inlined at hundreds of
 * call sites they cost tens of kilobytes of 6502 code.  For the RP6502
 * they become calls to functions in sys/rp6502/rp6502mac.c, which defines
 * RP6502_MACRO_BODIES to see the original macros.
 * Generated list; all of these are side-effect-free boolean expressions.
 */
#ifndef RP6502MAC_H
#define RP6502MAC_H

#if defined(RP6502) && !defined(RP6502_MACRO_BODIES)

extern boolean NDECL(rpm_Fire_resistance);
#undef Fire_resistance
#define Fire_resistance rpm_Fire_resistance()
extern boolean NDECL(rpm_Cold_resistance);
#undef Cold_resistance
#define Cold_resistance rpm_Cold_resistance()
extern boolean NDECL(rpm_Sleep_resistance);
#undef Sleep_resistance
#define Sleep_resistance rpm_Sleep_resistance()
extern boolean NDECL(rpm_Disint_resistance);
#undef Disint_resistance
#define Disint_resistance rpm_Disint_resistance()
extern boolean NDECL(rpm_Shock_resistance);
#undef Shock_resistance
#define Shock_resistance rpm_Shock_resistance()
extern boolean NDECL(rpm_Poison_resistance);
#undef Poison_resistance
#define Poison_resistance rpm_Poison_resistance()
extern boolean NDECL(rpm_Drain_resistance);
#undef Drain_resistance
#define Drain_resistance rpm_Drain_resistance()
extern boolean NDECL(rpm_Antimagic);
#undef Antimagic
#define Antimagic rpm_Antimagic()
extern boolean NDECL(rpm_Acid_resistance);
#undef Acid_resistance
#define Acid_resistance rpm_Acid_resistance()
extern boolean NDECL(rpm_Stone_resistance);
#undef Stone_resistance
#define Stone_resistance rpm_Stone_resistance()
extern boolean NDECL(rpm_Sick_resistance);
#undef Sick_resistance
#define Sick_resistance rpm_Sick_resistance()
extern boolean NDECL(rpm_Blindfolded);
#undef Blindfolded
#define Blindfolded rpm_Blindfolded()
extern boolean NDECL(rpm_Blind);
#undef Blind
#define Blind rpm_Blind()
extern boolean NDECL(rpm_Blindfolded_only);
#undef Blindfolded_only
#define Blindfolded_only rpm_Blindfolded_only()
extern boolean NDECL(rpm_Halluc_resistance);
#undef Halluc_resistance
#define Halluc_resistance rpm_Halluc_resistance()
extern boolean NDECL(rpm_Hallucination);
#undef Hallucination
#define Hallucination rpm_Hallucination()
extern boolean NDECL(rpm_Deaf);
#undef Deaf
#define Deaf rpm_Deaf()
extern boolean NDECL(rpm_Fumbling);
#undef Fumbling
#define Fumbling rpm_Fumbling()
extern boolean NDECL(rpm_Wounded_legs);
#undef Wounded_legs
#define Wounded_legs rpm_Wounded_legs()
extern boolean NDECL(rpm_Sleepy);
#undef Sleepy
#define Sleepy rpm_Sleepy()
extern boolean NDECL(rpm_Hunger);
#undef Hunger
#define Hunger rpm_Hunger()
extern boolean NDECL(rpm_See_invisible);
#undef See_invisible
#define See_invisible rpm_See_invisible()
extern boolean NDECL(rpm_Blind_telepat);
#undef Blind_telepat
#define Blind_telepat rpm_Blind_telepat()
extern boolean NDECL(rpm_Warning);
#undef Warning
#define Warning rpm_Warning()
extern boolean NDECL(rpm_Warn_of_mon);
#undef Warn_of_mon
#define Warn_of_mon rpm_Warn_of_mon()
extern boolean NDECL(rpm_Searching);
#undef Searching
#define Searching rpm_Searching()
extern boolean NDECL(rpm_Clairvoyant);
#undef Clairvoyant
#define Clairvoyant rpm_Clairvoyant()
extern boolean NDECL(rpm_Infravision);
#undef Infravision
#define Infravision rpm_Infravision()
extern boolean NDECL(rpm_Detect_monsters);
#undef Detect_monsters
#define Detect_monsters rpm_Detect_monsters()
extern boolean NDECL(rpm_Invis);
#undef Invis
#define Invis rpm_Invis()
extern boolean NDECL(rpm_Invisible);
#undef Invisible
#define Invisible rpm_Invisible()
extern boolean NDECL(rpm_Stealth);
#undef Stealth
#define Stealth rpm_Stealth()
extern boolean NDECL(rpm_Aggravate_monster);
#undef Aggravate_monster
#define Aggravate_monster rpm_Aggravate_monster()
extern boolean NDECL(rpm_Conflict);
#undef Conflict
#define Conflict rpm_Conflict()
extern boolean NDECL(rpm_Jumping);
#undef Jumping
#define Jumping rpm_Jumping()
extern boolean NDECL(rpm_Teleportation);
#undef Teleportation
#define Teleportation rpm_Teleportation()
extern boolean NDECL(rpm_Teleport_control);
#undef Teleport_control
#define Teleport_control rpm_Teleport_control()
extern boolean NDECL(rpm_Levitation);
#undef Levitation
#define Levitation rpm_Levitation()
extern boolean NDECL(rpm_Lev_at_will);
#undef Lev_at_will
#define Lev_at_will rpm_Lev_at_will()
extern boolean NDECL(rpm_Flying);
#undef Flying
#define Flying rpm_Flying()
extern boolean NDECL(rpm_Wwalking);
#undef Wwalking
#define Wwalking rpm_Wwalking()
extern boolean NDECL(rpm_Swimming);
#undef Swimming
#define Swimming rpm_Swimming()
extern boolean NDECL(rpm_Amphibious);
#undef Amphibious
#define Amphibious rpm_Amphibious()
extern boolean NDECL(rpm_Breathless);
#undef Breathless
#define Breathless rpm_Breathless()
extern boolean NDECL(rpm_Passes_walls);
#undef Passes_walls
#define Passes_walls rpm_Passes_walls()
extern boolean NDECL(rpm_Slow_digestion);
#undef Slow_digestion
#define Slow_digestion rpm_Slow_digestion()
extern boolean NDECL(rpm_Half_spell_damage);
#undef Half_spell_damage
#define Half_spell_damage rpm_Half_spell_damage()
extern boolean NDECL(rpm_Half_physical_damage);
#undef Half_physical_damage
#define Half_physical_damage rpm_Half_physical_damage()
extern boolean NDECL(rpm_Regeneration);
#undef Regeneration
#define Regeneration rpm_Regeneration()
extern boolean NDECL(rpm_Energy_regeneration);
#undef Energy_regeneration
#define Energy_regeneration rpm_Energy_regeneration()
extern boolean NDECL(rpm_Protection);
#undef Protection
#define Protection rpm_Protection()
extern boolean NDECL(rpm_Protection_from_shape_changers);
#undef Protection_from_shape_changers
#define Protection_from_shape_changers rpm_Protection_from_shape_changers()
extern boolean NDECL(rpm_Polymorph);
#undef Polymorph
#define Polymorph rpm_Polymorph()
extern boolean NDECL(rpm_Polymorph_control);
#undef Polymorph_control
#define Polymorph_control rpm_Polymorph_control()
extern boolean NDECL(rpm_Unchanging);
#undef Unchanging
#define Unchanging rpm_Unchanging()
extern boolean NDECL(rpm_Fast);
#undef Fast
#define Fast rpm_Fast()
extern boolean NDECL(rpm_Very_fast);
#undef Very_fast
#define Very_fast rpm_Very_fast()
extern boolean NDECL(rpm_Reflecting);
#undef Reflecting
#define Reflecting rpm_Reflecting()
extern boolean NDECL(rpm_Unaware);
#undef Unaware
#define Unaware rpm_Unaware()
extern boolean NDECL(rpm_Hate_silver);
#undef Hate_silver
#define Hate_silver rpm_Hate_silver()
extern boolean FDECL(rpm_tp_sensemon, (struct monst *));
#undef tp_sensemon
#define tp_sensemon(mon) rpm_tp_sensemon(mon)
extern boolean FDECL(rpm_sensemon, (struct monst *));
#undef sensemon
#define sensemon(mon) rpm_sensemon(mon)
extern boolean FDECL(rpm_mon_warning, (struct monst *));
#undef mon_warning
#define mon_warning(mon) rpm_mon_warning(mon)
extern boolean FDECL(rpm_mon_visible, (struct monst *));
#undef mon_visible
#define mon_visible(mon) rpm_mon_visible(mon)
extern boolean FDECL(rpm_see_with_infrared, (struct monst *));
#undef see_with_infrared
#define see_with_infrared(mon) rpm_see_with_infrared(mon)
extern boolean FDECL(rpm_canseemon, (struct monst *));
#undef canseemon
#define canseemon(mon) rpm_canseemon(mon)
extern boolean FDECL(rpm_canspotmon, (struct monst *));
#undef canspotmon
#define canspotmon(mon) rpm_canspotmon(mon)
extern boolean FDECL(rpm_knowninvisible, (struct monst *));
#undef knowninvisible
#define knowninvisible(mon) rpm_knowninvisible(mon)
extern boolean FDECL(rpm_is_safepet, (struct monst *));
#undef is_safepet
#define is_safepet(mon) rpm_is_safepet(mon)
extern boolean NDECL(rpm_canseeself);
#undef canseeself
#define canseeself() rpm_canseeself()
extern boolean NDECL(rpm_senseself);
#undef senseself
#define senseself() rpm_senseself()
extern boolean NDECL(rpm_canspotself);
#undef canspotself
#define canspotself() rpm_canspotself()

#endif /* RP6502 && !RP6502_MACRO_BODIES */
#endif /* RP6502MAC_H */
