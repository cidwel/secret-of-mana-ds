# Native ring-menu audio packets

Eleven bodies cover 105 candidate bytes: D0CEDF/25 is the shared genuine-wrapper tail, and ten eight-byte presets CEF8/CF00/CF08/CF10/CF18/CF20/CF28/CF30/CF38/CF40 send IDs 0C/0D/0E/29/2A/2B/2C/4B/28/A3. Presets preserve A/P through their original PHP/PHA frame, publish the original four-byte packet and execute the complete frozen audio request/ACK chain. CEDF is not claimed as a standalone callable entry without its producer frame.

Smoke2 and first capture20 passed: all ten presets at both accumulator widths, with saved full A and flags. All original native audio callees execute; full WRAM, CPU/context and peripheral/ACK state match.

Audit: **61/61 instruction starts**, every immediate width and all body boundaries reproduce source exactly. All 211 dependency hashes match. The outer seven-phase dispatcher and interactive C0666F/C066B5 menu remain open. No hooks or ROM/assets are persisted.

Integration spec: `event_menu_ring_audio_check:EventMenuRingAudio:event-menu-ring-audio:event_menu_ring_audio`.

## Frozen primary hashes

- `tools/event_menu_ring_audio.py`: `4f98d8f5e066260eb694169325bfa7f4979eac80d4fae89ee563703e3de6f72f`
- `tools/event_menu_ring_audio_check.py`: `7d44b4764031c5d136a271a91dd05e1a683b7267e4596d7f074324d43203ea27`
- `data/event_menu_ring_audio.json`: `37eaed09812d84d565e919f8d23a6da2bdcbfe007a5d83a83dbc9616a69fe5cd`
- `data/event_menu_ring_audio.coverage.json`: `936550e6e7f06f4edb3a1482c6416f44198a9334cd7f37637bc2d33b1a2e3c18`
- `data/event_menu_ring_audio.source_audit.json`: `1c39caeda56e7ac4a7ba733c8609f27a6f156c766b2d58759f1d5f45af5c9cf7`
