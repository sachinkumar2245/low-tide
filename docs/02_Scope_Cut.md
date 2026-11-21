# FILE: /docs/02_Scope_Cut.md

**Project:** LOW TIDE | **Phase:** 1 | **Doc status:** v0.2, reconciled against the Story Bible

Definitions:
- **Keep** = ships in v1.0.
- **Defer** = not in v1.0; hooks remain; stretch or post-launch candidate.
- **Cut** = removed; the architecture will not support it.
- **Merge** = story beat preserved, delivered inside another mission or as a short hub scene.

## 2.1 Map Size: Full Vision vs. Shippable

**Authoring-throughput derivation (tunable starting values, re-measured at the end of P4 using the real Old Mercy slice):**

| Input | Value | Basis |
|---|---|---|
| World-building window in P5 | ~30 dev-weeks (50% of a ~40-week P5 plus ~10 weeks borrowed from polish) | ASSUMPTION: solo dev |
| Dense urban throughput (modular kit, trim sheets) | 0.10 km² / week | ASSUMPTION; validated by MVS authoring |
| Industrial/waterfront throughput | 0.15 km² / week | Large repeated volumes (containers, warehouses) |
| Rural throughput (terrain + procedural scatter + roads) | 0.50 km² / week | Mostly terrain and scatter rules |
| Non-playable skyline backdrop (AD-13) | ~0.5 week total | Far-tier boxes plus emissive window atlas |

| District | Full-vision area (km²) | Full cost (wks) | Decision | Ship area (km²) | Ship cost (wks) | Bible content kept / reason |
|---|---|---|---|---|---|---|
| Old Mercy | 1.0 | 10.0 | **Keep** | 1.0 | 10.0 | "Emotional heart". Home, Mack's workshop, radio station, gyms, Lanterns. Pro, M2, M3, M10, M11, M13, Lena + Mack chains, Ending A, Ending D grave |
| The Cut | 1.5 | 10.0 | **Keep (reduced)** | 1.2 | 8.0 | Docks/shipyard (M8), Meridian construction site + flooded industrial streets (M14), ferry pier (M16), marina + boat launch (M4 scene, M17 approach) |
| Glass Mile | 1.5 | 15.0 | **Keep edge strip only** | 0.4 | 4.0 | The Saint (M1), casino back rooms (M6), King's Table scene, Rafe's penthouse cell (Ending B). Calder influence needs a playable footprint (§10). The high-rise core becomes skyline (AD-13) |
| Briar County | 4.0 | 8.0 | **Keep (reduced)** | 2.2 | 4.4 | Airfield race venue + backroads (M9), Ash Dogs hangout, motel, swamp edge, highway (Ending C) |
| Pelican Keys | 2.5 | 10.0 | **Defer** | 0 | 0 | No main-mission dependency. The vessel is anchored offshore of The Cut instead |
| Northbank | 1.5 | 10.0 | **Cut (skyline only)** | 0 | 0.5 | No mission is set there. Meridian's power stays off-screen ("they make phone calls"). Visible as skyline |
| **Total** | **12.0** | **63.0** | | **4.8** | **26.9** | **Area cut = 60.0%.** Cost cut = 57% |

26.9 of ~30 weeks leaves ~10% margin, which is thin. **First contingency:** shrink Briar from 2.2 to 1.5 km² (−1.4 wks). The full vision (63 weeks) is infeasible for a solo dev.
ASSUMPTION: Full-vision bounding extents are ~4 km × 4 km including sea. The shippable playable bounding box is ~3 km × 2.5 km plus the offshore vessel chunk. Both feed float precision analysis in Part 3 §3.1.

## 2.2 Mission Scope

Cost points: S = 1, M = 3, L = 8, intermediate values allowed. These are relative effort units used only to size the cut.

### Main path (Prologue + M1–M17 + act-ending scenes)

| Mission | District | Decision | Kept scope | Full | Kept |
|---|---|---|---|---|---|
| Prologue: Last Ferry | Vignette → Old Mercy | **Keep** | Full beat list (MVS) | 5 | 5 |
| M1 The Saint | Glass Mile edge | **Keep (reduced)** | 3 approaches: negotiate / intimidate / betray. "Another way" merged into negotiate | 3 | 3 |
| M2 Old Friends, New Enemies | Old Mercy | **Keep** | MVS | 3 | 3 |
| M3 The Paper Trail | Old Mercy (archive) | **Keep** | Investigation intro. Footage via AD-14 | 3 | 3 |
| M4 Welcome Back, Brother | The Cut marina | **Merge → scene** | Restaurant dialogue scene; the 3-way choice is kept | 3 | 1 |
| M5 No Easy Money | Hub | **Merge → scene** | Contact-choice scene; first faction rep write. The work happens in M8–M10 | 3 | 1 |
| M6 Cash Never Sleeps | Glass Mile casino rooms | **Keep (reduced)** | 2 approaches (stealth, direct). Social and inside-contact deferred | 8 | 4 |
| M7 Velvet Season | Glass Mile | **Defer** | Hooks only (01 §1.4). Info rewards re-routed through Inez in M1 | 3 | 0 |
| M8 The Last Honest Dock | The Cut | **Keep (reduced)** | Investigation + shipyard standoff | 5 | 3 |
| M9 Blacktop Saints | Briar | **Keep (reduced)** | 1 race + raid escape; 3 outcomes kept | 5 | 4 |
| M10 The Choir Is Listening | Old Mercy | **Keep (reduced)** | Dialogue decision + one fight; Lanterns outcome kept | 3 | 2 |
| M11 Dead Air | Old Mercy | **Keep** | Low combat; cheap | 2 | 2 |
| M12 Brother's Keeper | The Cut | **Keep** | 4 choices kept; the "work with Sofia" branch is the main source of Sofia trust | 3 | 3 |
| A1E The Map / A2E The Storm Files | Hub | **Keep** | Scenes | 2 | 2 |
| M13 Evacuation Day | Old Mercy | **Keep** | Set piece | 5 | 5 |
| M14 The Other Side of the Wall | The Cut | **Keep (reduced)** | Infiltration + flooded pursuit; single route | 5 | 4 |
| M15 The King's Table | Glass Mile | **Merge → scene** | Faction-alignment negotiation scene. Outcomes set M17 support. Elimination branch cut | 8 | 2 |
| M16 The Last Ferry | The Cut + vignette | **Keep (reduced)** | Pier walk + flashback vignette (AD-12) | 5 | 3 |
| M17 Low Tide | Offshore vessel | **Keep (reduced)** | Boat approach; 2 entry approaches (stealth, violent) + social confrontation as dialogue; Atlas decision | 8 | 5 |
| **Main subtotal** | | | | **82** | **55** |

### Endings

All 4 are kept as **1 shared finale (M17) + 4 epilogue variants** (full: 4 distinct finales = 20 pts; kept = 8). Every epilogue location is inside the kept scope:
- A: outside Mack's workshop (Old Mercy).
- B: Rafe's penthouse (Glass Mile airlock cell).
- C: night highway (Briar).
- D: Eli's grave (Old Mercy cemetery).

**Gate change:** the Sofia chain is deferred, so Ending D's Sofia-trust threshold is retuned to main-path sources (M3, M12, M13, M14) plus Atlas count.

### Character chains (28 missions)

| Chain | Missions | Decision | Reason |
|---|---|---|---|
| Lena: A Voice for the Floodplain | 5 | **Keep** | Ending A's broadcast channel. Old Mercy reuse |
| Mack: The Last Shift | 4 | **Keep** | Central mystery (Mack's silence). Workshop hub. Old Mercy + The Cut reuse |
| Inez: House Rules | 4 | Defer | The Saint's status is still set by M1 |
| Tam: Blacktop Saints | 5 | Defer | Ash Dogs outcome still set by M9 |
| Dime: The Lantern Test | 5 | Defer | Lanterns outcome still set by M10 |
| Sofia: The Unfinished Case | 5 | Defer | Trust is sourced from the main path (Ending D gate change) |
| **Chains subtotal** | 28 × 2 = **56** | 9 × 2 = **18** | Kept chains reuse kept districts at the highest rate |

## 2.3 Feature Scope Cut (complete)

| Feature / Area | Decision | Kept scope | Full | Kept | Reason |
|---|---|---|---|---|---|
| Main path | Keep (reduced) | §2.2 | 82 | 55 | — |
| Character chains | Keep 2 / Defer 4 | §2.2 | 56 | 18 | — |
| Endings | Keep (restructured) | §2.2 | 20 | 8 | — |
| Ground vehicles (26 assumed) | Keep 8 | Lena's hatchback, Mack's muscle car, transit coach, police cruiser, ambulance (Act III), pickup, box van, sports coupe (Rafe's offer) | 52 | 16 | Covers every handling and story archetype |
| Motorcycles (4) | Keep 1 | Ash Dogs bike, arcade-stabilized (no fall-over sim; rider ragdolls off) | 12 | 3 | Ash Dogs identity (§4). Full bike physics is the expensive part |
| Boats (4) | Keep 1 | Speedboat/tender: M17 approach + free roam | 12 | 3 | M17 requires it |
| Aircraft (2) | **Cut** | — | 16 | 0 | AD-04 |
| Weapons (18 assumed) | Keep 6 | Fists, bat, pistol, SMG, shotgun, rifle | 18 | 6 | One per combat role |
| Throwables | **Cut** | — | 2 | 0 | — |
| Racing (§9 street + Briar) | Keep 3 courses | Airfield drag/circuit + 2 backroad courses | 16 | 6 | Closed courses allow preload corridors |
| Boxing | Keep 3 bouts | Old Mercy gym; reuses melee | 6 | 3 | — |
| Gambling (casino, poker, betting) | Keep 1 | Poker | 12 | 3 | UI-only |
| Fishing | Defer | — | 6 | 0 | — |
| Hunting | **Cut** | — | 8 | 0 | Wildlife AI |
| Everyday venues (restaurants, pool halls, gyms, shopping) | Keep shops + gym | Weapon shop, Mack's (repair/paint), gym | 12 | 3 | — |
| Property / criminal business management | Keep minimal | 2 safehouses + The Saint as hub (M1 outcome) | 20 | 3 | Business sim = balancing cost |
| Vehicle customization | Keep reduced | Paint + 2 perf tiers at Mack's | 10 | 3 | Part swaps multiply vehicle art |
| Clothing | Defer | — | 8 | 0 | — |
| Nightlife (changing events, VIP, parties, music) | Keep minimal | The Saint, 1 static event state | 12 | 3 | — |
| Adult venues (Velvet Room, strip clubs) | Defer | Hooks (01 §1.4) | 12 | 0 | Content-sensitive |
| Romance / intimacy | Defer | Trust/dialogue only | 8 | 0 | 01 §1.4 |
| Photography | Defer | — | 4 | 0 | — |
| Investigation / case board | Keep (simplified) | Inspect + audio + linear case board | 8 | 6 | Core loop (§5) |
| Atlas pieces | Keep 40 | ~15 mission-bound + ~25 world-placed | 6 | 3 | Ending gates need count |
| Dynamic world events (30 types assumed) | Keep 8 | Faction turf fight, police gambling raid, business offer, business takeover, venue closure, storm damage, NPC remembers, roadside encounter | 30 | 8 | §9 examples |
| Criminal side activities (theft, protection, territory, smuggling, security contracts) | Keep 4 types | Vehicle theft, protection/retaliation, territory dispute, roadside encounter | 20 | 8 | — |
| Repeatable casino robberies | **Cut** | M6 only | 8 | 0 | — |
| Police / heat | Keep 3 tiers, ground only | — | 10 | 5 | AD-04, AD-10 |
| Traffic + pedestrians | Keep (reduced) | Fewer archetypes | 12 | 8 | — |
| NPC memory | Keep named/notable; Cut ambient persistence | — | 12 | 7 | 01 §1.2 |
| Personality + world variables (§2, §10) | **Keep all variables** | Consumers limited to dialogue, endings, AD-09 variants | 8 | 6 | Data is cheap; it is the game's spine |
| Visible world reactivity (§10) | Keep bounded | ≤12 variant sites per kept district (AD-09) | 12 | 4 | — |
| Weather | Keep scripted (Clear/Rain/Storm) | — | 10 | 4 | — |
| Time of day | Keep discrete (AD-07) | — | 6 | 3 | — |
| Swimming / diving | Keep surface; **Cut** diving | — | 8 | 3 | — |
| Destruction | Keep props; **Cut** structural | — | 8 | 2 | — |
| Economy (shops, laundering, investment) | Keep shops | — | 10 | 3 | — |
| Phone | Keep mission comms | — | 6 | 2 | — |
| Stealth | Keep crouch, LOS, takedown | — | 8 | 4 | — |
| Act III crisis state (closures, schedules, emergency vehicles) | Keep | Road-edge closures + variant groups + 1 emergency archetype | 10 | 6 | Bible: "not just a weather effect" |
| Act III flood | Keep (AD-08) | — | 8 | 8 | — |
| Casino vessel | Keep (anchored, static) | — | 8 | 5 | — |
| **Total** | | | **612** | **228** | **Feature cut = 62.7%** |

## 2.4 Minimum Viable Slice (MVS)

Delivered at the end of P4. P2 and P3 build its systems in graybox.

### Neighbourhood: Old Mercy core (~0.6 km², ~800 m × 750 m)

The Bible confirms Old Mercy. It contains Tyler's home, Mack's workshop and the bus terminal (ASSUMPTION: the terminal sits on the Old Mercy edge), so both MVS missions run entirely inside it. It is also the densest authoring type, which makes it the most informative throughput sample.
Challenge considered: The Cut would exercise water and boats, but water is P4+ risk, and The Cut has no Prologue or M2 content.
Limitation: narrow streets can't reach 55 m/s. Top speed is validated on the testbed stress highway (05). The MVS includes one arterial with a ≥600 m straight (~35–40 m/s).

### Vehicles (3, all story-sourced)

| Name (fictional) | Archetype | Mass / top speed (tunable) | What it tests |
|---|---|---|---|
| **Coastline Transit coach** | Heavy bus, NPC-driven | ~12 t / 25 m/s | Mass extreme, large collision hull, narrow-street clearance, spline drive, passenger camera (Pro) |
| **Lena's Ardent hatchback** | Light FWD compact | ~1.2 t / 42 m/s | Baseline handling, in-car dialogue under streaming (V39), most common traffic archetype later |
| **Mack's Sable muscle car** | Heavy RWD classic | ~1.6 t / 55 m/s | Top-speed streaming stress, high-torque/oversteer range, CCD, soft-cap hook, interior→exterior drive-out (workshop portal, AD-01) |

### Core Mission Loop: Prologue + M2

| Step | Beat | Verbs | Systems touched |
|---|---|---|---|
| 1 | Prison gate at dawn (Twilight) | V01, V57 | Vignette cell (AD-12), TOD state |
| 2 | Bus ride, skyline reveal | V58, V57 | Spline vehicle, vignette→world transition, **preload list**, skyline HLOD (AD-13) |
| 3 | Terminal reunion with Lena | V35 | Dialogue runtime, Lena trust write |
| 4 | Drive Lena's car home with in-car dialogue | V10, V11, V39, V54 | Vehicle physics, VO-vs-chunk I/O priority, waypoint |
| 5 | Eviction notice; enter home | V29, V07 | Inspect mode, seamless interior cell, portal culling |
| 6 | Fade → Night. Men arrive; melee, one draws a pistol (ASSUMPTION), disarm and shoot | V21, V04, V09, V23, V24, V41 | Melee, cover, weapon, basic AI (P3), Ruthlessness/Empathy write (spare or finish), witness heat |
| 7 | Find and play Eli's cassette → Atlas #1 | V29, V30, V32 | Inspect audio, Atlas bitset |
| 8 | Title; world opens; save at home | V51 | Save: global + chunk deltas |
| 9 | M2: go to Mack's workshop | V01/V11, V07 | Seamless vehicle-sized interior |
| 10 | Talk Last Ferry with Mack | V35, V63 | Mack trust write |
| 11 | Examine records → Eli's identity used at the municipal archive | V29 | Inspect mode |
| 12 | Drive Mack's car out; watcher gives chase, escape at heat 1 (ASSUMPTION) | V10, V11, V13, V17 | Interior→exterior vehicle exit, streaming at speed, damage, pursuer bound to the player bubble |
| 13 | Free roam: steal a parked car | V12 | Vehicle registry, witness heat |
| 14 | Return home, save, quit, reload | V51 | Full save/load round-trip, incl. door/prop deltas |

### MVS "Done" Criteria

Hardware: provisional **Min Spec** (6C/12T 2019-class CPU, 16 GB RAM, 6 GB VRAM GTX 1660 Super / RX 5600 XT class, SATA SSD). Final derivation in 03/04. All thresholds are tunable starting values, validated in P2/P3.

| # | Criterion | Threshold | Measured by |
|---|---|---|---|
| D1 | Frame time, scripted MVS route, 1080p, Min Spec | p50 ≤ 13.0 ms, **p99 ≤ 16.6 ms** | Benchmark harness (05) on the MVS route |
| D2 | Hitches | **0 frames > 33.3 ms**; ≤ 3 frames > 20 ms per 10 min | Same harness |
| D3 | Gameplay-critical pop-in | 0 cases of collidable geometry missing within the stream-in radius (set in Part 3) | Debug assert: player/vehicle never enters a non-Active chunk |
| D4 | Memory ceiling | RAM ≤ 6 GB, VRAM ≤ 4 GB peak | Profiler capture |
| D5 | Memory soak | Slope < 1 MB/min over 30 min, after a 5 min warm-up | Looping free-roam soak |
| D6 | Chunk arena hygiene | 100% of chunk arenas return to zero on unload | Allocation tagging (Part 3 §3.5) |
| D7 | Transitions | Vignette→world and interior airlock ≤ 1.5 s, with no visible hitch | Harness timestamps |
| D8 | Save/load | All §1.2 state used by the MVS round-trips; deltas restored after an unload/reload cycle | Integration test |
| D9 | Cold load to control | ≤ 30 s from launch (Min Spec SATA SSD) | Log timestamps |
| D10 | Stability | 2 h session, 0 crashes, 0 asserts in Profile build | Soak run |
| D11 | Content | 20–25 min first-play (Prologue + M2) + free roam over ~0.6 km² | Playtest timing |
