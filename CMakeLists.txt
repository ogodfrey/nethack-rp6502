cmake_minimum_required(VERSION 3.28)

# NetHack for the Picocomputer RP6502. Port files live in sys/rp6502.

include(${CMAKE_CURRENT_LIST_DIR}/tools/rp6502.cmake)

project(NETHACK-RP6502 C ASM)

# Milestone 1: console/DECgraphics/keyboard test ROM.
add_executable(dectest)
rp6502_asset(dectest help sys/rp6502/dectest.txt)
rp6502_executable(dectest DATA default RESET default)
target_sources(dectest PRIVATE
    sys/rp6502/dectest.c
)

# Milestone 2: the game itself.  cc65 cannot compile it (stack frames
# over 256 bytes; see sys/rp6502/FEASIBILITY.md), so it needs llvm-mos.
if(DEFINED LLVM_MOS_PLATFORM)
    # Headers that makedefs generates (pm.h, onames.h, date.h) come from
    # host-built utilities.
    set(NH_GEN ${CMAKE_BINARY_DIR}/gen)
    add_custom_command(
        OUTPUT ${NH_GEN}/include/pm.h ${NH_GEN}/include/onames.h
               ${NH_GEN}/include/date.h
        COMMAND ${CMAKE_SOURCE_DIR}/sys/rp6502/tools/hostgen.sh
                ${CMAKE_SOURCE_DIR} ${NH_GEN}
        DEPENDS sys/rp6502/tools/hostgen.sh util/makedefs.c src/monst.c
                src/objects.c include/monst.h include/objclass.h
                include/permonst.h include/rp6502conf.h
        COMMENT "Generating NetHack headers with host makedefs"
        VERBATIM
    )
    add_custom_target(nethack_gen DEPENDS ${NH_GEN}/include/pm.h)

    file(STRINGS sys/rp6502/sources.txt NH_SOURCES REGEX "^[^#]")

    # The full game does not fit in 64K without the overlay system (milestone
    # 3), so for now the modules are compiled as an object library only.
    add_library(nethack_objs OBJECT ${NH_SOURCES} sys/share/random.c)
    add_dependencies(nethack_objs nethack_gen)
    target_include_directories(nethack_objs PRIVATE ${NH_GEN}/include include)
    target_compile_definitions(nethack_objs PRIVATE RP6502)
    # -Oz -fno-inline is ~4% smaller than -Os and keeps single-caller statics
    # out of line, so no function is too big for an overlay slot.
    target_compile_options(nethack_objs PRIVATE -Oz -fno-inline -fno-lto
        -ffunction-sections -fdata-sections)
endif()
