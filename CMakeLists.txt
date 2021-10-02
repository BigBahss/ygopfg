cmake_minimum_required(VERSION 3.15)

project("25th-bot")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(
    "include"
)

set(CMAKE_AUTOMOC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 COMPONENTS Core REQUIRED)
# find_package(Qt5 COMPONENTS Widgets REQUIRED)
# find_package(Qt5 COMPONENTS Gui REQUIRED)

set(QT5_LIBRARIES Qt5::Core)
# set(QT5_LIBRARIES Qt5::Core Qt5::Widgets Qt5::Gui)

file(GLOB HEADERS RELATIVE ${CMAKE_SOURCE_DIR}
    "include/*.h"
)
qt_wrap_cpp(MOC_GENERATED_HEADERS ${HEADERS})

file(GLOB SOURCES RELATIVE ${CMAKE_SOURCE_DIR}
    "src/*.cpp"
)

add_executable(${PROJECT_NAME} ${SOURCES} ${MOC_GENERATED_HEADERS} ${RCC_GENERATED_RESOURCES} ${UI_GENERATED_HEADERS})

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)

set(LIBSQLITE_NAME sqlite3)
find_library(LIBSQLITE ${LIBSQLITE_NAME})
target_link_libraries(${PROJECT_NAME} ${LPTHREAD} ${WIN32_STATIC_LINK} ${QT5_LIBRARIES} ${LIBSQLITE} ${ADDITIONAL_LIBS})
