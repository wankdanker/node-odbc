project (odbc)
cmake_minimum_required(VERSION 3.10)

message( STATUS "VOID pointer size = " ${CMAKE_SIZEOF_VOID_P} )
# message( WARNING ${CMAKE_SIZEOF_VOID_P} )

# if(CMAKE_SIZEOF_VOID_P EQUAL 8)
#     # 64 bits
# elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
#     # 32 bits
# endif()

# To set platform specific LIBS
if(LINUX)
  set( LINK_LIBS odbc )
elseif(WIN32)
  set( LINK_LIBS odbc32.lib odbccp32.lib )
else()
  set( LINK_LIBS odbc )
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(module_include_dir "${PROJECT_SOURCE_DIR}/src")
set(module_source_dir "${PROJECT_SOURCE_DIR}/src")

# Include N-API wrappers
execute_process(COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_DIR
        )
string(REPLACE "\n" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})
string(REPLACE "\"" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})
include_directories(${CMAKE_JS_INC})

file(GLOB SOURCE_FILES
        "${module_include_dir}/dynodbc.h"
        "${module_include_dir}/odbc.h"
        "${module_include_dir}/odbc_connection.h"
        "${module_include_dir}/odbc_statement.h"
        "${module_include_dir}/strptime.h"
        "${module_source_dir}/dynodbc.cpp"
        "${module_source_dir}/odbc.cpp"
        "${module_source_dir}/odbc_connection.cpp"
        "${module_source_dir}/odbc_statement.cpp"
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
