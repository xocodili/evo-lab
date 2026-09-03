# Marketing & communications design

**Status:** Living guide (2026-08-28)  
**Audience:** Public (itch.io, social, talks) + contributors who need consistent visuals

---

## 1. Visual language

### Reference asset

**Primary lifecycle diagram:** [`assets/docs/hgt-camp-nom-lifecycle.png`](../assets/docs/hgt-camp-nom-lifecycle.png)

Style: **1950s science brochure / mid-century modern print** — flat vector, halftone texture, cream paper, bold panel headers, optimistic techno-biology tone.

### Palette (canonical)

| Role | Colour | Use |
|------|--------|-----|
| Paper | Cream / off-white | Background |
| Teal | `#2a9d8f` approx | Headers, A motor, water accents |
| Coral | `#e76f51` approx | Stage II drama, mouth, alerts |
| Mustard | `#f4a261` approx | C hub, highlights |
| Charcoal | Near-black | Line art, labels |
| White | Pure | Node fills, contrast |

### Typography tone

- **Headers:** Bold sans-serif, ALL CAPS, 1950s advertising (Futura / Helvetica feel)
- **Captions:** Short, punchy, one mechanical fact per line
- **Slogans:** Triadic rhythm — e.g. *CONNECT · TRANSFER · EVOLVE*

### Diagram grammar

| Element | Meaning |
|---------|---------|
| Y-star with central **C HUB** | CAMP camper “flux-capacitor” topology |
| **P SENSE** (eye) | Perceptor |
| **M MOUTH** | Ingestion / feedbag |
| **A MOTOR** (flagella) | Actuator / crawl |
| Solid axon bundles | Live capped edges |
| **Dashed stubs** | Partial topology / dangling uncapped ends |
| **X / ghost node** | Dead module |
| **DOCK!** starburst | Entropic INSERTION event |
| Panel arrows 1→2→3 | Death cascade → death feast → HGT |

Future assets should reuse this grammar before inventing new icons.

---

## 2. Public terminology

| Term | Say | Avoid |
|------|-----|-------|
| camper | Friendly collective for wet-layer life (organisms in the wet layer) | “Agent”, “creature blob” |
| CAMP camper | Standard four-module archetype | “Default organism” |
| Energon | Food substrate (bytes in the field) | “Energy points” |
| Death feast | Scavenging at corpse + structural dock | “Combat loot” |
| INSERTION | Horizontal acquisition of an axon edge | “Mutation”, “breeding” |
| Partial topology | Broken wiring after module death | “Damage state” |
| Flux-capacitor hub | C-centred Y-star (informal, fun) | Over-use in technical docs |

Technical docs: [HGT-INSERTION.md](HGT-INSERTION.md), [PARTHENOGENESIS.md](PARTHENOGENESIS.md), [EVOLUTION.md](EVOLUTION.md).

---

## 3. Voice & tone

- **Curious, not cute-for-cute’s-sake** — real mechanisms behind the metaphor
- **Evidence-anchored** when claiming biology (cite in devlogs, not on every poster)
- **Entropy is honest** — HGT is rare; reproduction is not shipped yet
- **1950s optimism, 2020s humility** — “THE FUTURE IS AXONOMIC!” with a wink

### Devlog structure (itch.io)

1. **What you can see** (screenshot / diagram)
2. **What changed in the sim** (one mechanism)
3. **Why it matters evolutionarily** (one paragraph, plain language)
4. **What’s next** (single sentence)

---

## 4. Asset inventory

| Asset | Path | Use |
|-------|------|-----|
| HGT lifecycle (3-panel) | `assets/docs/hgt-camp-nom-lifecycle.png` | Devlogs, talks, README hero |
| CAMP neural architecture + stem | `assets/docs/camp-neural-architecture-stem.png` | Anatomy devlog, itch.io |
| Energon regulation (Black Queen hub) | `assets/docs/camp-energon-regulation.png` | Metabolism / stable-pop devlog |
| Wet tank environment | `assets/docs/camp-environment-setup.png` | World / tide / sunfall devlog |
| Lexend Deca font | `assets/fonts/LexendDeca-Regular.ttf` | In-app HUD |

### Planned (not yet created)

- Tide / refugia map for geography rollout

---

## 5. Channels

| Channel | Content fit |
|---------|-------------|
| **itch.io devlog** | Diagram + 2 paragraphs + build note |
| **GitHub README** | Accurate snapshot + link here |
| **Talks / streams** | Stage I–III diagram as narrative spine |

---

## 6. Checklist before publishing visuals

- [ ] Matches palette and panel layout of reference asset
- [ ] CAMP labels (P, M, C, A) consistent with in-game inspector
- [ ] Distinguishes **chemical HGT** (mouth bytes) vs **structural INSERTION** (dock)
- [ ] Does not imply mating/reproduction if not shipped
- [ ] File stored under `assets/docs/` with descriptive name

---

## 7. itch.io release packaging

**Do not upload the exe alone.** Players need the full folder layout from a **Release build**:

1. Configure and build: `cmake --build build --config Release`
2. Zip **contents of** `build\src\` (not the repo root): `evo-lab.exe`, `assets\`, `resources\`
3. Upload zip to itch; set Windows executable to `evo-lab.exe`

Missing `assets/fonts/` or `resources/sprites/` → silent or broken UI. See root `README.md` **Play (itch zip)**.

**Scheduled 2026-09-03:** Fresh deploy with torpedo MPCA + perception retune — requires tonight’s full build zip.
