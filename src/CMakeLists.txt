set(target polymorphic_value)

set(_source_files
    main.cpp
)

add_executable(${target} ${_source_files})

target_link_libraries(${target} PRIVATE project_warnings project_options)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

target_link_libraries(
    ${target} PRIVATE
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(${target})
