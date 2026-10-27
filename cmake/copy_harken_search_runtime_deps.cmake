if(NOT DEFINED OUTDIR OR OUTDIR STREQUAL "")
    message(FATAL_ERROR "OUTDIR is required")
endif()
if(NOT DEFINED WEBVIEW2_DLL OR NOT EXISTS "${WEBVIEW2_DLL}")
    message(FATAL_ERROR "required WebView2Loader.dll is missing: ${WEBVIEW2_DLL}")
endif()

if(DEFINED BOOTSTRAP_EXE AND NOT BOOTSTRAP_EXE STREQUAL "")
    if(NOT EXISTS "${BOOTSTRAP_EXE}")
        message(FATAL_ERROR "required HarkenSearchBootstrap.exe is missing: ${BOOTSTRAP_EXE}")
    endif()
    cmake_path(ABSOLUTE_PATH BOOTSTRAP_EXE NORMALIZE
               OUTPUT_VARIABLE bootstrap_source)
    set(bootstrap_destination "${OUTDIR}/HarkenSearchBootstrap.exe")
    cmake_path(ABSOLUTE_PATH bootstrap_destination NORMALIZE
               OUTPUT_VARIABLE bootstrap_destination)
    if(NOT bootstrap_source STREQUAL bootstrap_destination)
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${bootstrap_source}" "${bootstrap_destination}"
            RESULT_VARIABLE bootstrap_copy_result
        )
        if(NOT bootstrap_copy_result EQUAL 0)
            message(FATAL_ERROR "failed to copy required HarkenSearchBootstrap.exe")
        endif()
    endif()
endif()

file(MAKE_DIRECTORY "${OUTDIR}/models")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${WEBVIEW2_DLL}" "${OUTDIR}/WebView2Loader.dll"
    RESULT_VARIABLE required_copy_result
)
if(NOT required_copy_result EQUAL 0)
    message(FATAL_ERROR "failed to copy required WebView2Loader.dll")
endif()

function(copy_optional_runtime source destination)
    if(EXISTS "${source}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${source}" "${OUTDIR}/${destination}"
            RESULT_VARIABLE optional_copy_result
        )
        if(NOT optional_copy_result EQUAL 0)
            message(FATAL_ERROR "failed to copy optional runtime present at ${source}")
        endif()
    endif()
endfunction()

copy_optional_runtime("${RLOTTIE_BIN}/rlottie.dll" "rlottie.dll")
copy_optional_runtime("${RLOTTIE_IMG}/rlottie-image-loader.dll"
                      "rlottie-image-loader.dll")
