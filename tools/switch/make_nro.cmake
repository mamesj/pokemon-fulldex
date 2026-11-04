# Packs the Switch executable into an .nro for the Homebrew Menu. Run at build time by the
# ssrecomp_nro target (CMakeLists.txt), so that whether there is an icon of your own is
# decided when building, not when configuring.
#
# In: NACPTOOL, ELF2NRO, ELF, OUT, NACP, TITLE, AUTHOR, VERSION, ICON (may be missing), DEFAULT_ICON,
# ROMFS (a folder for the .nro's romfs, or empty).

if(NOT DEFINED AUTHOR OR AUTHOR STREQUAL "")
    set(AUTHOR "IamCarron")
endif()

execute_process(COMMAND "${NACPTOOL}" --create "${TITLE}" "${AUTHOR}" "${VERSION}" "${NACP}"
    RESULT_VARIABLE rc)
if(rc)
    message(FATAL_ERROR "nacptool failed (${rc})")
endif()

set(args "${ELF}" "${OUT}" "--nacp=${NACP}")
if(EXISTS "${ICON}")
    message(STATUS "Icon: ${ICON}")
    list(APPEND args "--icon=${ICON}")
elseif(EXISTS "${DEFAULT_ICON}")
    list(APPEND args "--icon=${DEFAULT_ICON}")
endif()
if(ROMFS)
    if(NOT IS_DIRECTORY "${ROMFS}")
        message(FATAL_ERROR "SSR_SWITCH_ROMFS is not a folder: ${ROMFS}")
    endif()
    message(STATUS "romfs: ${ROMFS}")
    list(APPEND args "--romfsdir=${ROMFS}")
endif()
execute_process(COMMAND "${ELF2NRO}" ${args} RESULT_VARIABLE rc)
if(rc)
    message(FATAL_ERROR "elf2nro failed (${rc})")
endif()
