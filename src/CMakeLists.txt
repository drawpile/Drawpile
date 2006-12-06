project (DrawPile)

configure_file ( config.h.cmake config.h )

include ( CheckIncludeFiles )
check_include_files ( stdint.h HAVE_STDINT_H )

subdirs ( shared/ server/ client/ )
