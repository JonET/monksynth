# Cross-platform patch script for FetchContent PATCH_COMMAND.
# Applies every *.patch in this directory if not already applied.
# A failed apply is a warning, not a hard error, because the Linux static-link
# patch is guarded by MONKSYNTH_STATIC_LINUX_DEPS. CMakeLists.txt checks
# separately that vstgui-wine-gdi-paint.patch landed on Windows.

get_filename_component(PATCH_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
file(GLOB PATCH_FILES "${PATCH_DIR}/*.patch")
list(SORT PATCH_FILES)

foreach(PATCH_FILE IN LISTS PATCH_FILES)
    get_filename_component(PATCH_NAME "${PATCH_FILE}" NAME)

    execute_process(
        COMMAND git apply --reverse --check "${PATCH_FILE}"
        RESULT_VARIABLE ALREADY_APPLIED
        OUTPUT_QUIET ERROR_QUIET
    )

    if(ALREADY_APPLIED EQUAL 0)
        message(STATUS "${PATCH_NAME} already applied, skipping")
        continue()
    endif()

    execute_process(
        COMMAND git apply "${PATCH_FILE}"
        RESULT_VARIABLE PATCH_RESULT
        OUTPUT_VARIABLE PATCH_OUTPUT
        ERROR_VARIABLE PATCH_ERROR
    )

    if(PATCH_RESULT EQUAL 0)
        message(STATUS "${PATCH_NAME} applied successfully")
    else()
        message(WARNING "${PATCH_NAME} could not be applied: ${PATCH_ERROR}")
    endif()
endforeach()
