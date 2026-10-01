# VexFactory: The Last Freight

## Goal

Ship a complete, finite factory campaign with a beginning, ten playable chapters, and an ending.
Keep the sandbox, Vecs simulation, CC0 art, and opt-in dependency downloads.
Do not change the Vecs library API.

## Readability first

- Draw the interface at native window resolution. Do not shrink a 1440-pixel interface into a smaller window.
- Enable Retina/high-DPI support. Load a system sans-serif font at the framebuffer density.
- Keep body text at 18 pixels or more. Wrap paragraphs instead of shrinking them.
- Separate world zoom and pan from interface size. Add interface size controls.
- Adapt the tool and objective panels to the available height. Test small windows and large text.

## Campaign

A storm cuts the freight line that supplies the settlements of Lumen.
The player is its last working industrial engineer.
Dispatcher Iona and mechanic Vale guide the restoration from a dark rail yard to the final evacuation convoy.
Mission briefings, completion reports, and the final epilogue tell the story.

1. **A Light in the Yard** — reconnect an ore shipment and learn directional belts.
2. **Shelter Before Dawn** — produce plates for the east ward.
3. **The Stalled Lift** — learn the two-plate gear recipe.
4. **One Furnace, Three Wards** — divide one production line with a splitter under a tight power cap.
5. **A Heart for the Pumps** — feed an assembler with two gears and one plate.
6. **Across the Flood** — route mixed contracts through narrow passages and add generators.
7. **The Night Shift** — build parallel production for a larger relief order.
8. **The Last Convoy** — balance all three products against a dispatch deadline.
9. **Keep the Lights On** — rebuild supply with no existing power.
10. **Homebound** — supply the final train and conclude the campaign.

Every mission has fixed deposits, protected docks, terrain, a starting budget, a power allowance, product quotas, and a time allowance.
Later missions unlock routing tools and generators.
Every mission must have a tested solution without workshop upgrades.

## Planning and production

- Planning does not consume the dispatch timer. Launch production when the layout is ready.
- Miners need finite ore deposits in campaign mode.
- Belts and machines cost credits. Demolition refunds part of the cost, but scraps their contents.
- Only required deliveries earn credits. Surplus cargo cannot create an infinite income loop.
- Machines reserve power. Generators cost money and floor space.
- Campaign recipes: one ore -> one plate; two plates -> one gear; two gears + one plate -> one engine.
- Splitters alternate between forward and left outputs. Sorters route the selected product forward and other products left.
- Docks reject cargo that they do not request. Jams remain visible and diagnosable.
- Contracts give a win state. Missed dispatch or insufficient remaining material gives a failure state with restart.
- Optional time and construction targets award up to three stars.

## Progress and persistence

- Sequential mission unlocks, replay, best stars, and best times.
- A workshop with conveyor, processing, and recovery upgrades. Rewards cannot be farmed by repeating the same result.
- Atomic, versioned, checksum-protected saves in the operating system's user-data directory.
- Preserve an active factory, its buffers, in-flight items, remaining ore, credits, and timer.
- Keep a backup and report corrupt or incompatible saves without replacing a valid running factory.
- Save on explicit request, periodically, on mission completion, and on exit.
- Smoke runs use disposable profiles and do not touch player progress.

## Presentation

- Title menu, campaign map, mission briefing, factory HUD, pause menu, workshop, help, results, and ending.
- Readable contracts, deposit counts, build costs, power, time, refunds, and machine inspection.
- Keyboard and mouse controls, zoom/pan, tool hotkeys, and configurable text size.
- Procedural UI sounds with a mute option. No extra audio download.
- Keep the free-build sandbox available from the title menu.

## Completion gates

1. Existing sandbox simulation tests remain green.
2. Campaign tests cover money, deposits, recipes, routing, power, victory/failure, and rewards.
3. Scripted factory layouts complete all ten missions with no upgrades.
4. Save/resume preserves state and deterministic continuation. Invalid files fail safely.
5. Windows and Linux builds pass. Graphical smoke tests cover menus, gameplay, resizing, large text, and save/resume.
6. Add macOS graphical compilation to CI. Distinguish compilation coverage from a native Retina playtest.
7. Commit each tested milestone using the repository's lowercase imperative message style.
