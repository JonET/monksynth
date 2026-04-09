# Cross-platform patch script for FetchContent PATCH_COMMAND.
# Applies vstgui-static-linux.patch if not already applied.
# Silently succeeds if the patch is already applied or fails to apply
# (the patched code is guarded by a CMake option, so it's harmless).

get_filename_component(PATCH_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
set(PATCH_FILE "${PATCH_DIR}/vstgui-static-linux.patch")

# Check if the patch has already been applied by looking for our marker
execute_process(
    COMMAND git apply --reverse --check "${PATCH_FILE}"
    RESULT_VARIABLE ALREADY_APPLIED
    OUTPUT_QUIET ERROR_QUIET
)

if(ALREADY_APPLIED EQUAL 0)
    message(STATUS "VSTGUI static-linux patch already applied, skipping")
    return()
endif()

execute_process(
    COMMAND git apply "${PATCH_FILE}"
    RESULT_VARIABLE PATCH_RESULT
    OUTPUT_VARIABLE PATCH_OUTPUT
    ERROR_VARIABLE PATCH_ERROR
)

if(PATCH_RESULT EQUAL 0)
    message(STATUS "VSTGUI static-linux patch applied successfully")
else()
    message(WARNING "VSTGUI static-linux patch could not be applied: ${PATCH_ERROR}")
endif()
