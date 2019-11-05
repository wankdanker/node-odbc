project (odbc)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(odbc_include_dir "${PROJECT_SOURCE_DIR}/src")
set(odbc_source_dir "${PROJECT_SOURCE_DIR}/src")

set( LINK_LIBS odbc32.lib odbccp32.lib )

# Include N-API wrappers
execute_process(COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_DIR
        )
string(REPLACE "\n" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})
string(REPLACE "\"" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})
include_directories(${CMAKE_JS_INC})

file(GLOB SOURCE_FILES
        "${odbc_include_dir}/dynodbc.h"
        "${odbc_include_dir}/odbc.h"
        "${odbc_include_dir}/odbc_connection.h"
        "${odbc_include_dir}/odbc_statement.h"
        "${odbc_include_dir}/strptime.h"
        "${odbc_source_dir}/dynodbc.cpp"
        "${odbc_source_dir}/odbc.cpp"
        "${odbc_source_dir}/odbc_connection.cpp"
        "${odbc_source_dir}/odbc_statement.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC} )
target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/include
    PRIVATE
        ${NODE_ADDON_API_DIR}
        ${CMAKE_JS_INC}
)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS=1 NAPI_DISABLE_CPP_EXCEPTIONS)
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB} ${LINK_LIBS} )

# Define NAPI_VERSION
add_definitions(-DNAPI_VERSION=3)
