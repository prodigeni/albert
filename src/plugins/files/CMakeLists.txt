set(LIB_NAME files)

find_package(Qt5Widgets 5.1 REQUIRED)

include_directories(
    ../../application/ #TODO replace by coreapi
    ../../shared
    ../../application/pluginsystem/extension
)

set(SRC
    metadata.json
    extension.cpp
    configwidget.cpp
    file.cpp
    fileactions.h
    indexer.cpp
    ../../shared/search/search.cpp
    ../../shared/search/search_impl.h
    ../../shared/search/prefixsearch.h
    ../../shared/search/fuzzysearch.h
)

qt5_wrap_ui(UI
    configwidget.ui
)

add_library(${LIB_NAME} SHARED ${SRC} ${UI})
set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(${LIB_NAME} ${Qt5Widgets_LIBRARIES} )

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    # Define where to install the lib
    install(TARGETS ${LIB_NAME} LIBRARY DESTINATION share/${PROJECT_NAME}/plugins)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
