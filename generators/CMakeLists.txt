# SPDX-License-Identifier: MIT

if(USE_GENERATORS)
    add_executable(generate_conversions generate_conversions.c)

    if(TARGET Qt${QT_VERSION}::Gui)
        add_executable(qt_image_resize)
        target_sources(qt_image_resize PRIVATE qt_image_resize.cpp)
        target_link_libraries(qt_image_resize "Qt${QT_VERSION}::Gui")

        if("${QT_VERSION}" EQUAL 5)
            add_executable(qt_premul_factors)
            target_sources(qt_premul_factors PRIVATE qt_premul_factors.cpp)
            target_link_libraries(qt_premul_factors Qt5::Gui)
        else()
            message(STATUS "Not building qt_premul_factors generator since it "
                           "only works with Qt5")
        endif()
    else()
        message(STATUS "Qt${QT_VERSION} not found, not building associated "
                       "generators")
    endif()
endif()
