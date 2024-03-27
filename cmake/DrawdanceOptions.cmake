# SPDX-License-Identifier: MIT

if(CLIENT OR SERVER)
    set(IMAGE_IMPL QT)
    set(FILE_IO_IMPL QT)
    if(QT_VERSION_MAJOR EQUAL 5)
        set(ZIP_IMPL KARCHIVE)
    else()
        set(ZIP_IMPL LIBZIP)
    endif()
else()
    set(IMAGE_IMPL LIBS CACHE STRING "PNG and JPEG implementation (LIBS, QT)")
    string(TOUPPER IMAGE_IMPL ${IMAGE_IMPL})

    set(FILE_IO_IMPL STDIO CACHE STRING "Default file I/O (STDIO, QT)")
    string(TOUPPER FILE_IO_IMPL ${FILE_IO_IMPL})

    set(ZIP_IMPL LIBZIP CACHE STRING "ZIP folder implementation (LIBZIP, KARCHIVE)")
    string(TOUPPER ZIP_IMPL ${ZIP_IMPL})
endif()

add_feature_info("Image library implementation (IMAGE_IMPL)" ON ${IMAGE_IMPL})
add_feature_info("File I/O implementation (FILE_IO_IMPL)" ON ${FILE_IO_IMPL})
add_feature_info("ZIP implementation (ZIP_IMPL)" ON ${ZIP_IMPL})
