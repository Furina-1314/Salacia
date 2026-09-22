cmake_minimum_required(VERSION 3.16)

project(rov_control LANGUAGES CXX)

include(CTest)
find_package(Threads REQUIRED)

add_library(rov_control STATIC
    src/rov_control.cpp
    src/internal/transport.cpp
    src/internal/posix_tty_transport.cpp
    src/internal/response_parser.cpp
    src/internal/rpmsg_client.cpp
)

target_compile_features(rov_control PUBLIC cxx_std_17)
target_include_directories(rov_control
    PUBLIC include
    PRIVATE src
)
target_link_libraries(rov_control PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(rov_control PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(rov_control PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

add_library(rov_self_test_support STATIC tools/self_test_runner.cpp)
target_compile_features(rov_self_test_support PUBLIC cxx_std_17)
target_include_directories(rov_self_test_support PUBLIC tools)
target_link_libraries(rov_self_test_support PUBLIC rov_control)

if(MSVC)
    target_compile_options(rov_self_test_support PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(rov_self_test_support PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

add_executable(rov_control_tests
    tests/test_main.cpp
    tests/response_parser_test.cpp
    tests/extended_response_parser_test.cpp
    tests/rpmsg_client_test.cpp
    tests/rov_control_test.cpp
    tests/extended_rov_control_test.cpp
    tests/self_test_runner_test.cpp
)
target_compile_features(rov_control_tests PRIVATE cxx_std_17)
target_include_directories(rov_control_tests PRIVATE src tests)
target_link_libraries(rov_control_tests PRIVATE rov_control rov_self_test_support)

if(MSVC)
    target_compile_options(rov_control_tests PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(rov_control_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

add_executable(rov_self_test tools/rov_self_test.cpp)
target_compile_features(rov_self_test PRIVATE cxx_std_17)
target_link_libraries(rov_self_test PRIVATE rov_self_test_support)

if(MSVC)
    target_compile_options(rov_self_test PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(rov_self_test PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

add_test(NAME rov_control_tests COMMAND rov_control_tests)

add_executable(rov_api_smoke examples/rov_api_smoke.cpp)
target_compile_features(rov_api_smoke PRIVATE cxx_std_17)
target_link_libraries(rov_api_smoke PRIVATE rov_control)

if(MSVC)
    target_compile_options(rov_api_smoke PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(rov_api_smoke PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()
