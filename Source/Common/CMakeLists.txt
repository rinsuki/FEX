add_subdirectory(cpp-optparse/)

set(NAME Common)
set(SRCS
  ArgumentLoader.cpp
  EnvironmentLoader.cpp
  StringUtil.cpp)

if (NOT MINGW_BUILD)
  list (APPEND SRCS
    Config.cpp
    FEXServerClient.cpp
    FileFormatCheck.cpp)
endif()

add_library(${NAME} STATIC ${SRCS})
target_link_libraries(${NAME} FEXCore_Base cpp-optparse tiny-json json-maker FEXHeaderUtils)
target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/External/cpp-optparse/)
target_include_directories(${NAME} PRIVATE ${CMAKE_BINARY_DIR}/generated)
