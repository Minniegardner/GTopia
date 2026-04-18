# Parity Audit (Reference vs Current)

Date: 2026-04-17
Scope: Release gate audit for effect/block/achievement parity against ReferensiSrc/GrowtopiaV1.

## 1) Effect Matrix

| Area | Reference (GrowtopiaV1) | Current (after patch) | Status | Notes |
|---|---|---|---|---|
| Magplant suck effect packet type | ITEM_EFFECT packet | ITEM_EFFECT packet | PASS | Switched from SEND_PARTICLE_EFFECT to ITEM_EFFECT in world sucker flow. |
| Magplant suck visual path | Uses source+destination trajectory | Uses source+destination trajectory | PASS | Uses pos + dest fields with client item ID in tileX/tileY. |
| Magplant suck effect ID | ParticleID 0 | field2 set to 0 | PASS | Aligned to reference behavior. |
| Ghost slime touch effect | ParticleAltID 195 | Particle effect 195 in ghost slime path | PASS | Matches current ghost algorithm behavior. |
| Ghost jar capture effect | ParticleAltID 44 | Particle effect 44 in ghost capture path | PASS | Matches reference behavior used for capture feedback. |
| Friend login audio | audio/friend_logon.wav | friend_logon.wav via PlaySFX delay 2000 | PASS | Local + cross-subserver execute path supported. |
| Friend logout audio | audio/friend_logoff.wav | friend_logoff.wav via PlaySFX delay 2000 | PASS | Local + cross-subserver execute path supported. |
| Friend alert message format | `3FRIEND ALERT ... logged on / `! ... logged off | Same format text | PASS | Exact style replicated in GameServer local and cross-server handler. |
| Friend duplicate alert hardening | Debounce repeated state edges in short window | Debounce per friend+event window | PASS | Receiver-side debounce added for both local and cross-subserver paths. |

## 2) Block/System Matrix

| Area | Reference | Current (after patch) | Status | Notes |
|---|---|---|---|---|
| Magplant extra serialization order | itemID, itemCount, magnet, remote, itemLimit | Same order | PASS | Critical crash fix applied in TileExtra_Magplant serialize order. |
| Magplant world enter stability | Requires decode-compatible extra data | Order now reference-compatible | PASS | Remaining risk is legacy corrupted world data from old order saves. |
| GBC Lucky in Love | GetGBCDrop(isPotion) with Lucky in Love check | GetGBCDrop(IsLuckyInLovePotionActive) | PASS | Hardcoded false removed. |
| SGBC Lucky in Love | GetSGBCDrop(isPotion) with Lucky in Love check | GetSGBCDrop(IsLuckyInLovePotionActive) | PASS | Hardcoded false removed. |
| Birth Certificate field name | NewName | NewName in consumable + dialog flow | PASS | Input key aligned to NewName. |
| Birth Certificate 2-step flow | BirthCertificate -> BirthCertificateConfirm with stored pending name | Same two-step with pending name memory | PASS | Confirm now consumes pending name from memory, not retyped field. |
| Neutron Gun ghost interaction | Pull ghost along beam-distance rule | PullGhostsTowardPlayer implemented | PASS | Uses line-distance threshold and 0.6 pull ratio. |
| Friend options checkbox | checkbox_notifications in friend options dialog | Implemented in FriendMenu -> FriendsOptions | PASS | Saved via player stats map. |
| Friend login/logoff local notify | Notify online friends on state change | Implemented on EnterGame and LogOff | PASS | Respects per-player notification option. |
| Friend cross-subserver notify bridge | UpdateToMaster-like global notification | Implemented via TCP_CROSS_ACTION_FRIEND_LOGIN/LOGOUT + Master routing | PASS | Numeric userID query support added in Master resolver. |
| Legacy world migration utility | Admin-run repair for old world files | /repairmagplantworlds command + backup + dry-run | PASS | Rewrites world files through serializer after compatibility decode fallback. |

## 3) Achievement Matrix

| Area | Reference | Current | Status | Notes |
|---|---|---|---|---|
| Achievement catalog breadth | Large static set (~171 shown in reference UI) | Smaller curated classic-focused set | PARTIAL | Functional but not 1:1 full coverage. |
| Achievement trigger plumbing | Multi-system + events + playmods | Stat-based + selected event triggers | PARTIAL | Core works; not full reference parity yet. |
| Achievement broadcast/effects | Reference-specific per achievement/feature set | Basic announcement + existing effects | PARTIAL | Needs full one-by-one mapping before strict parity release. |

## 4) Remaining Gaps Before "Strict 1:1" Release

1. Complete achievement-by-achievement parity import (IDs, thresholds, trigger sources, optional effects/messages).
2. Validate legacy worlds created before magplant serialization fix; re-save/migrate old tiles if needed.
3. Optional: introduce delayed console-message transport helper if exact 2000ms delay semantics are required for friend alerts.

## 5) Recommended Release Verification

1. Enter world containing dense magplant setup and confirm no client crash.
2. Verify magplant suck visual path and packet behavior with multiple item types.
3. Test GBC/SGBC with and without Lucky in Love playmod.
4. Test friend options toggle on/off and local login/logoff notifications.
5. Test cross-subserver friend notifications using two servers and two friend accounts.
6. Test Birth Certificate flow end-to-end (invalid, valid, confirm, consume, cooldown).
7. Test neutron gun behavior against active ghosts in haunted world.
8. Re-run achievement smoke tests for existing implemented classic achievements.
