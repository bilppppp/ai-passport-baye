set(BAYE_CORE_SRCS
    core/FgtCount.c
    core/FgtPkAi.c
    core/Fight.c
    core/FightSub.c
    core/PublicFun.c
    core/Size.c
    core/citycmd.c
    core/citycmdb.c
    core/citycmdc.c
    core/citycmdd.c
    core/citycmde.c
    core/cityedit.c
    core/comIn.c
    core/comOut.c
    core/datman.c
    core/debug.c
    core/gamEng.c
    core/infdeal.c
    core/itoa.c
    core/paccount.c
    core/pconst.c
    core/pstring.c
    core/showface.c
    core/strcnt.c
    core/tactic.c
    core/varma.c
    core/config.c
    core/touch.c
    core/data-bind.c
    core/bind-objects.c
)

set(BAYE_PLATFORM_SRCS
    platform/passport_fsys.c
    platform/passport_display.c
    platform/passport_gui.c
    platform/passport_timer.c
    platform/passport_input.c
    platform/passport_bios.c
    platform/passport_sys.c
    platform/passport_battery.c
    platform/passport_adpcm.c
    platform/passport_audio.c
    platform/passport_main.c
)

idf_component_register(
    SRCS ${BAYE_CORE_SRCS} ${BAYE_PLATFORM_SRCS}
    INCLUDE_DIRS "include" "core" "core/baye" "core/inc" "core/data" "platform"
    REQUIRES bsp esp_lcd esp_timer nvs_flash
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error
    -Wno-error=maybe-uninitialized
    -Wno-maybe-uninitialized
    -Wno-error=uninitialized
    -Wno-uninitialized
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-unused-label
    -Wno-unused-but-set-variable
    -Wno-parentheses
    -Wno-implicit-fallthrough
    -Wno-pointer-sign
    -Wno-format
    -Wno-char-subscripts
    -Wno-int-conversion
    -Wno-incompatible-pointer-types
)

target_add_binary_data(${COMPONENT_LIB} "assets/dat.lib" BINARY)
target_add_binary_data(${COMPONENT_LIB} "assets/font.bin" BINARY)
target_add_binary_data(${COMPONENT_LIB} "assets/baye_title_16k.adpcm" BINARY)
target_add_binary_data(${COMPONENT_LIB} "assets/baye_strategy_16k.adpcm" BINARY)
target_add_binary_data(${COMPONENT_LIB} "assets/baye_battle_16k.adpcm" BINARY)
target_add_binary_data(${COMPONENT_LIB} "assets/baye_victory_16k.adpcm" BINARY)
target_add_binary_data(${COMPONENT_LIB} "assets/baye_defeat_16k.adpcm" BINARY)

