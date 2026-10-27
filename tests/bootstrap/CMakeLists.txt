cmake_minimum_required(VERSION 3.20)

project(aris_bootstrap_core_tests LANGUAGES C CXX)

enable_testing()
find_package(Python3 COMPONENTS Interpreter QUIET)

add_executable(aris_bootstrap_admission_tests
    test_aris_bootstrap_admission.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapAdmission.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapJournal.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapPlan.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/ed25519/tweetnacl.c"
)

set_target_properties(aris_bootstrap_admission_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(aris_bootstrap_admission_tests PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../src"
    "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty"
)
target_compile_definitions(aris_bootstrap_admission_tests PRIVATE
    ARIS_BOOTSTRAP_TEST_CAPABILITY_ISSUER=1
    ARIS_BOOTSTRAP_TEST_RAW_INVOCATION_ADMISSION=1)

if(MSVC)
    target_compile_options(aris_bootstrap_admission_tests PRIVATE /W4 /WX /EHsc /permissive- /utf-8)
else()
    target_compile_options(aris_bootstrap_admission_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME aris_bootstrap_admission_tests COMMAND aris_bootstrap_admission_tests)

add_executable(aris_bootstrap_runtime_tests
    test_aris_bootstrap_runtime.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/platform/update_bootstrap_authority_base.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapAdmission.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapRuntime.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapRuntimeWin32.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapJournal.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapFilesystem.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapFilesystemWin32.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapPlan.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/ed25519/tweetnacl.c"
)
set_target_properties(aris_bootstrap_runtime_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_include_directories(aris_bootstrap_runtime_tests PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../src"
    "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty"
)
if(WIN32)
    target_compile_definitions(aris_bootstrap_runtime_tests PRIVATE
        ARIS_BOOTSTRAP_NO_MAIN=1)
endif()
if(MSVC)
    target_compile_options(aris_bootstrap_runtime_tests PRIVATE /W4 /WX /EHsc /permissive- /utf-8)
else()
    target_compile_options(aris_bootstrap_runtime_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME aris_bootstrap_runtime_tests COMMAND aris_bootstrap_runtime_tests)
if(WIN32)
    target_link_libraries(aris_bootstrap_runtime_tests PRIVATE advapi32 crypt32 shell32 ole32 wintrust)
endif()

if(WIN32)
    add_executable(aris_bootstrap_health_fixture bootstrap_health_fixture.cpp)
    add_executable(aris_bootstrap_sleep_fixture bootstrap_sleep_fixture.cpp)
    target_include_directories(aris_bootstrap_health_fixture PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty")
    add_executable(aris_bootstrap_native_tests
        test_aris_bootstrap_native.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapAdmission.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapRuntime.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapRuntimeWin32.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapJournal.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapFilesystem.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapFilesystemWin32.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapPlan.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/ed25519/tweetnacl.c")
    target_include_directories(aris_bootstrap_native_tests PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../src"
        "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty")
    target_compile_definitions(aris_bootstrap_native_tests PRIVATE
        ARIS_BOOTSTRAP_NO_MAIN=1
        ARIS_BOOTSTRAP_TEST_PROCESS_CUTS=1)
    target_link_libraries(aris_bootstrap_native_tests PRIVATE advapi32 crypt32 shell32 ole32 wintrust)
    add_dependencies(aris_bootstrap_native_tests
        aris_bootstrap_health_fixture
        aris_bootstrap_sleep_fixture)
    foreach(target aris_bootstrap_health_fixture aris_bootstrap_sleep_fixture
                  aris_bootstrap_native_tests)
        set_target_properties(${target} PROPERTIES
            CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
        target_compile_options(${target} PRIVATE /W4 /WX /EHsc /permissive- /utf-8)
    endforeach()
    add_test(NAME aris_bootstrap_native_tests COMMAND aris_bootstrap_native_tests)

    add_executable(aris_bootstrap_launch_broker_tests
        test_aris_bootstrap_launch_broker.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapAdmission.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapRuntime.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapRuntimeWin32.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapJournal.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapFilesystem.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapFilesystemWin32.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapPlan.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/ed25519/tweetnacl.c")
    target_include_directories(aris_bootstrap_launch_broker_tests PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../src"
        "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty")
    target_compile_definitions(aris_bootstrap_launch_broker_tests PRIVATE
        ARIS_BOOTSTRAP_NO_MAIN=1
        ARIS_BOOTSTRAP_TEST_LAUNCH_BROKER=1)
    target_link_libraries(aris_bootstrap_launch_broker_tests PRIVATE
        advapi32 crypt32 shell32 ole32 wintrust)
    add_dependencies(aris_bootstrap_launch_broker_tests
        aris_bootstrap_sleep_fixture)
    set_target_properties(aris_bootstrap_launch_broker_tests PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    target_compile_options(aris_bootstrap_launch_broker_tests PRIVATE
        /W4 /WX /EHsc /permissive- /utf-8)
    add_test(NAME aris_bootstrap_launch_broker_tests
        COMMAND aris_bootstrap_launch_broker_tests)

    add_executable(aris_bootstrap_entrypoint_tests
        test_aris_bootstrap_entrypoint.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/platform/update_bootstrap_authority_base.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapAdmission.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapRuntime.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapPlan.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapFilesystem.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapFilesystemWin32.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapJournal.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapRuntimeWin32.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/ed25519/tweetnacl.c")
    target_include_directories(aris_bootstrap_entrypoint_tests PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../src"
        "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty")
    target_compile_definitions(aris_bootstrap_entrypoint_tests PRIVATE
        ARIS_BOOTSTRAP_NO_MAIN=1
        ARIS_BOOTSTRAP_TEST_CAPABILITY_ISSUER=1
        ARIS_BOOTSTRAP_TEST_RAW_INVOCATION_ADMISSION=1)
    target_link_libraries(aris_bootstrap_entrypoint_tests PRIVATE advapi32 crypt32 shell32 ole32 wintrust)
    set_target_properties(aris_bootstrap_entrypoint_tests PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    target_compile_options(aris_bootstrap_entrypoint_tests PRIVATE
        /W4 /WX /EHsc /permissive- /utf-8)
    add_test(NAME aris_bootstrap_entrypoint_tests
        COMMAND aris_bootstrap_entrypoint_tests)

    # Explicit fixture-key composition used only by the separate-process
    # black-box test.  The production HarkenSearchBootstrap target never receives a
    # default key; this target is intentionally isolated in the test project.
    add_executable(aris_bootstrap_blackbox_bootstrap WIN32
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapAdmission.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapRuntime.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapPlan.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapFilesystem.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapFilesystemWin32.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapJournal.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapRuntimeWin32.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/ed25519/tweetnacl.c")
    target_include_directories(aris_bootstrap_blackbox_bootstrap PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../src"
        "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty")
    target_compile_definitions(aris_bootstrap_blackbox_bootstrap PRIVATE
        "ARIS_BOOTSTRAP_PACKAGE_KEY_ID=\"fixture-2026\""
        "ARIS_BOOTSTRAP_PACKAGE_PUBLIC_KEY_HEX=\"d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a\""
        ARIS_BOOTSTRAP_TEST_DIAGNOSTICS=1
        ARIS_BOOTSTRAP_TEST_FAIL_CHILD_LAUNCH=1
        ARIS_BOOTSTRAP_TEST_LAUNCH_BROKER=1
        ARIS_BOOTSTRAP_TEST_TRUSTED_ROOTS=1
        ARIS_BOOTSTRAP_TEST_CRASH_AFTER_CHILD_CHECKPOINT=1
        ARIS_BOOTSTRAP_TEST_CRASH_AFTER_PREVIOUS_RELEASE=1
        ARIS_BOOTSTRAP_TEST_CRASH_AFTER_COMMIT=1
        ARIS_BOOTSTRAP_TEST_CRASH_AFTER_COMMITTED_CLEANUP=1
        ARIS_BOOTSTRAP_TEST_CRASH_AFTER_COMMITTED_RELEASE=1
        ARIS_BOOTSTRAP_TEST_TIMEOUTS=1)
    target_link_libraries(aris_bootstrap_blackbox_bootstrap PRIVATE advapi32 crypt32 shell32 ole32 wintrust)
    set_target_properties(aris_bootstrap_blackbox_bootstrap PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
        OUTPUT_NAME "HarkenSearchBootstrap")
    target_compile_options(aris_bootstrap_blackbox_bootstrap PRIVATE
        /W4 /WX /EHsc /permissive- /utf-8)

    add_executable(aris_bootstrap_handoff_fixture
        bootstrap_handoff_fixture.cpp
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapAdmission.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapJournal.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapPlan.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../src/ed25519/tweetnacl.c")
    target_include_directories(aris_bootstrap_handoff_fixture PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../src"
        "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty")
    target_link_libraries(aris_bootstrap_handoff_fixture PRIVATE advapi32)
    set_target_properties(aris_bootstrap_handoff_fixture PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    target_compile_options(aris_bootstrap_handoff_fixture PRIVATE
        /W4 /WX /EHsc /permissive- /utf-8)

    if(Python3_Interpreter_FOUND)
        add_test(NAME aris_bootstrap_blackbox_tests
            COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_LIST_DIR}/test_aris_bootstrap_blackbox.py"
                "$<TARGET_FILE:aris_bootstrap_blackbox_bootstrap>"
                "$<TARGET_FILE:aris_bootstrap_handoff_fixture>"
                "$<TARGET_FILE:aris_bootstrap_health_fixture>"
                "$<TARGET_FILE:aris_bootstrap_sleep_fixture>")
        set_tests_properties(aris_bootstrap_blackbox_tests PROPERTIES TIMEOUT 120)
    endif()
endif()

# This target intentionally stays outside the product root CMake graph.
add_executable(aris_bootstrap_core_tests
    test_aris_bootstrap_plan.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapPlan.cpp"
)

set_target_properties(aris_bootstrap_core_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(aris_bootstrap_core_tests PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../src"
)

if(MSVC)
    target_compile_options(aris_bootstrap_core_tests PRIVATE /W4 /WX /EHsc /permissive-)
else()
    target_compile_options(aris_bootstrap_core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME aris_bootstrap_core_tests COMMAND aris_bootstrap_core_tests)

add_executable(aris_bootstrap_deployment_identity_tests
    test_update_deployment_identity.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/platform/update_deployment_identity_win32.cpp"
)
set_target_properties(aris_bootstrap_deployment_identity_tests PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_include_directories(aris_bootstrap_deployment_identity_tests PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../src")
if(MSVC)
    target_compile_options(aris_bootstrap_deployment_identity_tests PRIVATE /W4 /WX /EHsc /permissive- /utf-8)
else()
    target_compile_options(aris_bootstrap_deployment_identity_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME aris_bootstrap_deployment_identity_tests
    COMMAND aris_bootstrap_deployment_identity_tests)

add_executable(aris_bootstrap_filesystem_tests
    test_aris_bootstrap_filesystem.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapAdmission.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/HarkenSearchBootstrapPlan.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapJournal.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapFilesystem.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/bootstrap/WindowsBootstrapFilesystemWin32.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../src/ed25519/tweetnacl.c"
)

set_target_properties(aris_bootstrap_filesystem_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(aris_bootstrap_filesystem_tests PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../src"
    "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty"
)

if(WIN32)
    target_link_libraries(aris_bootstrap_filesystem_tests PRIVATE advapi32)
endif()

if(MSVC)
    target_compile_options(aris_bootstrap_filesystem_tests PRIVATE /W4 /WX /EHsc /permissive- /utf-8)
else()
    target_compile_options(aris_bootstrap_filesystem_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME aris_bootstrap_filesystem_tests COMMAND aris_bootstrap_filesystem_tests)

if(WIN32)
    foreach(target aris_bootstrap_runtime_tests aris_bootstrap_native_tests
                   aris_bootstrap_launch_broker_tests aris_bootstrap_entrypoint_tests
                   aris_bootstrap_blackbox_bootstrap)
        target_sources(${target} PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/../../src/platform/update_bootstrap_authority_base.cpp")
    endforeach()
    foreach(target aris_bootstrap_entrypoint_tests aris_bootstrap_blackbox_bootstrap)
        target_sources(${target} PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/bootstrap_test_diagnostics.cpp")
    endforeach()
endif()

set(ARIS_BOOTSTRAP_TEST_WORKDIR
    "${CMAKE_BINARY_DIR}/system-temp")
file(MAKE_DIRECTORY "${ARIS_BOOTSTRAP_TEST_WORKDIR}")
set_tests_properties(
    aris_bootstrap_admission_tests
    aris_bootstrap_runtime_tests
    aris_bootstrap_native_tests
    aris_bootstrap_launch_broker_tests
    aris_bootstrap_entrypoint_tests
    aris_bootstrap_core_tests
    aris_bootstrap_filesystem_tests
    PROPERTIES WORKING_DIRECTORY "${ARIS_BOOTSTRAP_TEST_WORKDIR}")
if(TEST aris_bootstrap_blackbox_tests)
    set_tests_properties(aris_bootstrap_blackbox_tests PROPERTIES
        WORKING_DIRECTORY "${ARIS_BOOTSTRAP_TEST_WORKDIR}")
endif()
