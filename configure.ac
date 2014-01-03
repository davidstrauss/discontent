dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(discontent, 1.0)


AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE()

PKG_CHECK_MODULES(SD_DAEMON, libsystemd-daemon >= 204)
PKG_CHECK_MODULES(SD_JOURNAL, libsystemd-journal >= 204)
PKG_CHECK_MODULES(MICROHTTPD, libmicrohttpd >= 0.9.27)
AC_SEARCH_LIBS(markdown, markdown, [], [
        echo "Error! You need to have libmarkdown installed."
        exit -1
        ])

AC_PROG_CC
AC_PROG_CC_C99

AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT
