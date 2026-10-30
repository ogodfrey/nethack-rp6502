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
