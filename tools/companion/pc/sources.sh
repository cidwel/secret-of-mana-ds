# The sources and headers of som-pc, shared by the development wrapper (som-pc), the release script and the CI recipes.
# Needs HERE (this directory), C (tools/companion) and REPO (repository root) set by the caller.
SRC="$HERE/som_pc.c $HERE/pc_logic.c $HERE/pc_sdl.c $HERE/pc_store.c $HERE/pc_gameplay.c $HERE/pc_states.c $HERE/pc_menu.c $HERE/pc_shader.c $HERE/pc_runs.c $HERE/pc_gen.c $HERE/pc_gpc.c \
$C/launcher/som_credits.c $C/ux/som_ui_draw.c $C/ux/som_ui_state.c $C/ux/som_ui_sel.c $C/ux/som_ui_zone.c $C/som_companion.c \
$C/direct/som_direct.c $C/som_hotkey.c $C/som_bind.c $C/launcher/som_launcher.c $C/launcher/som_launcher_catalog.c $C/launcher/som_launcher_draw.c $C/launcher/somrando_menu.c $C/gameplay_c/som_gameplay.c"
HDR="$HERE/pc_logic.h $HERE/pc_sdl.h $HERE/pc_store.h $HERE/pc_gameplay.h $HERE/pc_states.h $HERE/pc_menu.h $HERE/pc_shader.h $HERE/pc_runs.h $HERE/pc_gen.h $HERE/pc_gpc.h $HERE/pc_compat.h \
$C/launcher/som_credits.h $REPO/data/credits.json $C/ux/som_ui_draw.h $C/ux/som_ui_priv.h $C/som_companion.h $C/direct/som_direct.h $C/launcher/som_launcher.h \
$C/launcher/somrando_api.h $C/launcher/somrando_menu.h $C/gameplay_c/som_gameplay.h $REPO/tools/gameplay/gameplay_menu.json"
