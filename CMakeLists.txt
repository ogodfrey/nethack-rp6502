cmake_minimum_required(VERSION 3.28)

# NetHack for the Picocomputer RP6502. Port files live in sys/rp6502.

include(${CMAKE_CURRENT_LIST_DIR}/tools/rp6502.cmake)

project(NETHACK-RP6502 C ASM)

# Build the game with mons[] and objects[] in XRAM (see include/rp6502xram.h).
# The game does not link yet; this compiles the modules with the accessors.
option(RP6502_XRAM_TABLES "Compile the game with mons[] and objects[] in XRAM" OFF)

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
    # hostgen.sh is a POSIX script with no reliable execute bit on Windows, so
    # run it through sh there (MSYS2 or Git for Windows), with its tools (mkdir,
    # cc) on PATH.
    if(CMAKE_HOST_WIN32)
        find_program(NH_SH NAMES sh
            PATHS C:/msys64/usr/bin "$ENV{ProgramFiles}/Git/usr/bin"
            REQUIRED)
        get_filename_component(NH_SH_BIN "${NH_SH}" DIRECTORY)
        get_filename_component(NH_MSYS "${NH_SH_BIN}/../.." ABSOLUTE)
        set(NH_HOSTGEN_PATH "${NH_SH_BIN};${NH_MSYS}/ucrt64/bin;$ENV{PATH}")
    else()
        set(NH_SH sh)
        set(NH_HOSTGEN_PATH "$ENV{PATH}")
    endif()
    add_custom_command(
        OUTPUT ${NH_GEN}/include/pm.h ${NH_GEN}/include/onames.h
               ${NH_GEN}/include/date.h
        COMMAND ${CMAKE_COMMAND} -E env "PATH=${NH_HOSTGEN_PATH}"
                ${NH_SH} ${CMAKE_SOURCE_DIR}/sys/rp6502/tools/hostgen.sh
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
    if(RP6502_XRAM_TABLES)
        # The two table sources define their arrays under RAM names, which
        # only the image tools (xramtest) use, and the rest of the game sees
        # the XRAM views.
        set_source_files_properties(src/monst.c PROPERTIES COMPILE_DEFINITIONS
            "RP6502_MONS_RAM;mons=rp6502_ram_mons")
        set_source_files_properties(src/objects.c PROPERTIES COMPILE_DEFINITIONS
            "RP6502_MONS_RAM;objects=rp6502_ram_objects")
        target_compile_definitions(nethack_objs PRIVATE RP6502_MONS_XRAM)
    endif()

    # Prototype: mons[] copied to XRAM and read back through the fetch cache.
    # monst.c is built with mons renamed, so the test has a RAM copy to load.
    add_library(xram_monst OBJECT src/monst.c)
    add_dependencies(xram_monst nethack_gen)
    target_include_directories(xram_monst PRIVATE ${NH_GEN}/include include)
    target_compile_definitions(xram_monst PRIVATE RP6502 RP6502_MONS_RAM
        mons=rp6502_ram_mons)
    target_compile_options(xram_monst PRIVATE -Oz -fno-inline -fno-lto
        -ffunction-sections -fdata-sections)

    # Same for objects[]: objects.c is built with the table renamed to a RAM
    # copy for the image. The game itself never has this copy.
    add_library(xram_objs OBJECT src/objects.c)
    add_dependencies(xram_objs nethack_gen)
    target_include_directories(xram_objs PRIVATE ${NH_GEN}/include include)
    target_compile_definitions(xram_objs PRIVATE RP6502 RP6502_MONS_RAM
        objects=rp6502_ram_objects)
    target_compile_options(xram_objs PRIVATE -Oz -fno-inline -fno-lto
        -ffunction-sections -fdata-sections)

    # The names are not part of either image: mons_names[] is ordinary RAM.
    add_library(xram_names OBJECT src/monnames.c)
    add_dependencies(xram_names nethack_gen)
    target_include_directories(xram_names PRIVATE ${NH_GEN}/include include)
    target_compile_definitions(xram_names PRIVATE RP6502)
    target_compile_options(xram_names PRIVATE -Oz -fno-inline -fno-lto
        -ffunction-sections -fdata-sections)

    add_executable(xramtest)
    # The XRAM images of the tables are byproducts of the link: after the
    # link, elfsym.py copies each table's initial data from the output, so the
    # pointers in the image match this program. The addresses come from
    # include/xram.h.
    file(STRINGS include/xram.h MONS_XRAM_LINE REGEX "^#define MONS_XRAM_ADDR")
    string(REGEX MATCH "0x[0-9a-fA-F]+" MONS_XRAM_ADDR "${MONS_XRAM_LINE}")
    file(STRINGS include/xram.h OBJS_XRAM_LINE REGEX "^#define OBJS_XRAM_ADDR")
    string(REGEX MATCH "0x[0-9a-fA-F]+" OBJS_XRAM_ADDR "${OBJS_XRAM_LINE}")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_custom_command(TARGET xramtest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:xramtest>
        COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/sys/rp6502/tools/elfsym.py
                $<TARGET_FILE:xramtest>.elf rp6502_ram_mons
                $<TARGET_FILE_DIR:xramtest>/mons.bin
        COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/sys/rp6502/tools/elfsym.py
                $<TARGET_FILE:xramtest>.elf rp6502_ram_objects
                $<TARGET_FILE_DIR:xramtest>/objects.bin
        VERBATIM)
    set_property(TARGET xramtest APPEND PROPERTY LINK_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/sys/rp6502/tools/elfsym.py)
    rp6502_byproduct(xramtest XRAM(${MONS_XRAM_ADDR}) mons.bin)
    rp6502_byproduct(xramtest XRAM(${OBJS_XRAM_ADDR}) objects.bin)
    rp6502_executable(xramtest DATA default RESET default)
    target_sources(xramtest PRIVATE
        sys/rp6502/xramtest.c
        sys/rp6502/rp6502xram.c
        $<TARGET_OBJECTS:xram_monst>
        $<TARGET_OBJECTS:xram_objs>
        $<TARGET_OBJECTS:xram_names>
    )
    # objects[] is not linked into the program; its name is given the XRAM
    # address, so &objects[i] is the address in the image.
    target_link_options(xramtest PRIVATE -Wl,--defsym,objects=${OBJS_XRAM_ADDR})
    add_dependencies(xramtest nethack_gen)
    target_include_directories(xramtest PRIVATE ${NH_GEN}/include include)
    target_compile_definitions(xramtest PRIVATE RP6502 RP6502_MONS_XRAM)
    target_compile_options(xramtest PRIVATE -Oz -fno-inline -fno-lto
        -ffunction-sections -fdata-sections)

    # The whole game as one program, to measure what the link needs. There are
    # no data assets yet, so it cannot run; the link reports what does not fit.
    add_executable(nethack EXCLUDE_FROM_ALL)
    add_dependencies(nethack nethack_objs)
    target_sources(nethack PRIVATE $<TARGET_OBJECTS:nethack_objs>)
    target_include_directories(nethack PRIVATE ${NH_GEN}/include include)
    target_compile_definitions(nethack PRIVATE RP6502)
    target_compile_options(nethack PRIVATE -Oz -fno-inline -fno-lto
        -ffunction-sections -fdata-sections)
    rp6502_executable(nethack DATA default RESET default)
    if(RP6502_XRAM_TABLES)
        target_sources(nethack PRIVATE sys/rp6502/rp6502xram.c)
        target_compile_definitions(nethack PRIVATE RP6502_MONS_XRAM)
        target_link_options(nethack PRIVATE -Wl,--defsym,objects=${OBJS_XRAM_ADDR})
    endif()
endif()
