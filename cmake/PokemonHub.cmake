# Shared by the application and ROM-free National Dex checks.
get_filename_component(FULLDEX_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PKSMCORE_DIR "${FULLDEX_SOURCE_DIR}/third_party/PKSM-Core")
file(GLOB_RECURSE PKSMCORE_SOURCES CONFIGURE_DEPENDS "${PKSMCORE_DIR}/source/*.cpp" "${PKSMCORE_DIR}/memecrypto/*.c")
file(GLOB_RECURSE PKSMCORE_DATA CONFIGURE_DEPENDS "${PKSMCORE_DIR}/personals/*" "${PKSMCORE_DIR}/strings/*.txt")
file(GLOB_RECURSE _pksm_include_items LIST_DIRECTORIES true "${PKSMCORE_DIR}/include/*")
set(PKSMCORE_INCLUDE_DIRS "${PKSMCORE_DIR}/include")
foreach(_item ${_pksm_include_items})
    if(IS_DIRECTORY "${_item}")
        list(APPEND PKSMCORE_INCLUDE_DIRS "${_item}")
    endif()
endforeach()
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pksmcore_data_builtin.cpp"
    COMMAND "${Python3_EXECUTABLE}" "${FULLDEX_SOURCE_DIR}/tools/pksm/embed_data.py" "${PKSMCORE_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}/pksmcore_data_builtin.cpp"
    DEPENDS "${FULLDEX_SOURCE_DIR}/tools/pksm/embed_data.py" ${PKSMCORE_DATA} VERBATIM)
add_library(pokemonhub STATIC ${PKSMCORE_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/pksmcore_data_builtin.cpp"
    "${FULLDEX_SOURCE_DIR}/frontend/pokemon_hub.cpp")
set_target_properties(pokemonhub PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(pokemonhub PRIVATE "${FULLDEX_SOURCE_DIR}/third_party/pksmcore-config"
    ${PKSMCORE_INCLUDE_DIRS} "${PKSMCORE_DIR}/memecrypto" "${PKSMCORE_DIR}/pcg-cpp/include")
