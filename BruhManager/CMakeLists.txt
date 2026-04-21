add_library(BruhManager_incLib INTERFACE)
target_include_directories(BruhManager_incLib INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
file(GLOB_RECURSE BruhManager_incLib_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
)
target_sources(BruhManager_incLib INTERFACE ${BruhManager_incLib_SOURCES})
add_executable(BruhManager main.cpp)
target_link_libraries(BruhManager PUBLIC BruhManager_incLib)