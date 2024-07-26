cmake_minimum_required(VERSION 3.23)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL "no")
endif()

execute_process(
  COMMAND $ENV{SHELL} -c "grep -m1 version poac.toml"
  COMMAND $ENV{SHELL} -c "cut -f 2 -d'\"'"
  RESULT_VARIABLE POAC_VERSION
  OUTPUT_QUIET
)

execute_process(
  COMMAND $ENV{SHELL} -c "git rev-parse HEAD"
  RESULT_VARIABLE POAC_COMMIT_HASH
  OUTPUT_QUIET
)
execute_process(
  COMMAND $ENV{SHELL} -c "git rev-parse --short HEAD"
  RESULT_VARIABLE POAC_COMMIT_SHORT_HASH
  OUTPUT_QUIET
)
execute_process(
  COMMAND $ENV{SHELL} -c "git show -s --date=format-local:'%Y-%m-%d' --format=%cd"
  RESULT_VARIABLE POAC_COMMIT_DATE
  OUTPUT_QUIET
)

project(
  poac
  VERSION ${POAC_VERSION}
  LANGUAGES CXX
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(
  POAC_THIRD_PARTY
  REQUIRED
  libgit2>=1.1.0
  libgit2<1.9.0
  libcurl>=7.79.1
  libcurl<9.0.0
  nlohmann_json>=3.10.5
  nlohmann_json<4.0.0
  tbb>=2021.5.0
  tbb<2022.0.0
  fmt>=8.1.1
  fmt<11.0.0
)

include(FetchContent)
FetchContent_Declare(
  toml11
  GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
  GIT_TAG v4.0.3
)
FetchContent_MakeAvailable(toml11)

file(GLOB_RECURSE POAC_SOURCES src/*.cc)

add_executable(poac ${POAC_SOURCES})

target_compile_definitions(
  poac
  PRIVATE POAC_POAC_PKG_VERSION="${POAC_VERSION}"
  PRIVATE POAC_POAC_COMMIT_HASH="${POAC_COMMIT_HASH}"
  PRIVATE POAC_POAC_COMMIT_SHORT_HASH="${POAC_COMMIT_SHORT_HASH}"
  PRIVATE POAC_POAC_COMMIT_DATE="${POAC_COMMIT_DATE}"
)

set_target_properties(poac PROPERTIES CXX_STANDARD 20)
target_include_directories(poac PUBLIC ${POAC_THIRD_PARTY_INCLUDE_DIRS})
target_link_libraries(poac PUBLIC toml11 ${POAC_THIRD_PARTY_LIBRARIES})

include(GNUInstallDirs)
install(TARGETS poac RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
