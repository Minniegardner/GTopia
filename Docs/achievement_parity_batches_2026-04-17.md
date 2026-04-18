# Achievement Parity Import Plan (Batch-by-Batch)

Date: 2026-04-17
Goal: Reach 1:1 achievement parity against ReferensiSrc/GrowtopiaV1 with explicit verification checklist per batch.

## Batch Status

- [x] Batch 0: Catalog foundation imported (all reference achievement keys registered in server catalog).
- [x] Batch 1: Core classic/stat thresholds aligned with reference checks already present.
- [x] Batch 2: Mid-tier progression achievements (Builder/Farmer/Demolition ladders and economy ladders).
- [ ] Batch 3: Event-driven and seasonal achievements.
- [ ] Batch 4: Specialty systems (fishing, ghost, surgery, anomalyzer, star captain, guild).
- [ ] Batch 5: Final message/effect parity audit and duplicate trigger hardening.

## Batch 1 Checklist (Implemented)

- [x] Builder (Classic) threshold
- [x] Farmer (Classic) threshold
- [x] Demolition (Classic) threshold
- [x] Packrat (Classic) gem threshold
- [x] Ding! (Classic) level threshold
- [x] Big Spender (Classic)
- [x] Trashman (Classic)
- [x] Paint The Town Blue (Classic)
- [x] Mine, All Mine (Classic)
- [x] Milkin' It (Classic)
- [x] Bubble Bubble (Classic)
- [x] Surgeon (Classic)
- [x] Radiation Hunter (Classic)
- [x] Deadly Vacuum (Classic)
- [x] Resurrection (Classic)
- [x] Who Ya Gonna Call? (Classic)
- [x] Mastermind (Classic)
- [x] Big Breaker (Classic)
- [x] Hammer Time (Classic)
- [x] Reaper King (Classic)
- [x] Bone Breaker (Classic)
- [x] Rod Snapper (Classic)
- [x] Trowel Troubles (Classic)
- [x] Cultivate This! (Classic)
- [x] Scrapped Scanners! (Classic)
- [x] Cooking Conundrum! (Classic)
- [x] Social Butterfly (Classic)
- [x] Sparkly Dust (Classic)
- [x] Embiggened (Classic)

## Batch 2 Checklist (Implemented)

- [x] ProBuilder (`BLOCKS_PLACED >= 1000`)
- [x] FarmerInTheDell (`TREES_HARVESTED >= 1000`)
- [x] WreckingCrew (`BLOCKS_DESTROYED >= 1000`)
- [x] Hoarder (`Gems >= 10000`)
- [x] OlMoneybags (`Gems >= 100000`)
- [x] BiggerBreaker (`BROKEN_ANOMALIZERS >= 100`)
- [x] ExpertBuilder (`BLOCKS_PLACED >= 10000`)
- [x] FarmasaurusRex (`TREES_HARVESTED >= 10000`)
- [x] Annihilator (`BLOCKS_DESTROYED >= 10000`)
- [x] FilthyRich (`Gems >= 500000`)
- [x] OneMeeelion (`Gems >= 1000000`)
- [x] BiggestBreaker (`BROKEN_ANOMALIZERS >= 500`)

Notes:
- Multi-tier stat unlock logic now evaluates all matching rules for the same stat name (no first-match early return).
- Breaker-tier achievements depend on `BROKEN_ANOMALIZERS` stat producers in gameplay paths.

## Batch 3 Checklist (Event/Seasonal)

- [ ] Heartbreaker
- [ ] StupidCupid
- [ ] FourLeaves
- [ ] LittleGreenMan
- [ ] GotLuckyCharms
- [ ] SixteenDozen
- [ ] BouncingBabyBunny
- [ ] ADeadRabbit
- [ ] EggHunter
- [ ] BashCinco
- [ ] LaVidaDeLaFiesta
- [ ] Gorro
- [ ] Champeon
- [ ] TooManyPineapples
- [ ] FreshAir
- [ ] TheLastCelebration
- [ ] SummerGrillin
- [ ] BrightFuture

## Verification Protocol (each batch)

1. Verify unlock condition reached in controlled test world/account.
2. Verify no early unlock before threshold.
3. Verify no duplicate unlock spam.
4. Verify persistence survives relog.
5. Verify message/effect text parity with reference format.
6. Record result in release audit and mark checklist item.
