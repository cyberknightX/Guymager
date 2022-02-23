# ****************************************************************************
#  Project:        GUYMAGER
# ****************************************************************************
#  Programmer:     Guy Voncken
#                  Police Grand-Ducale
#                  Service de Police Judiciaire
#                  Section Nouvelles Technologies
# ****************************************************************************
#  Qt project file
# ****************************************************************************

# Copyright 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 
# 2018, 2019, 2020
# Guy Voncken
#
# This file is part of Guymager.
#
# Guymager is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# Guymager is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Guymager. If not, see <http://www.gnu.org/licenses/>.


# Command line configuration options
# ----------------------------------
# These options should behave like the classical ./configure options, i.e. they
# allow for using different paths etc. Currently, there are the following
# pre-compiler definitions:
#    SPLASH_DIR      Defines where to look for the splash image (splash.png)
#                    Example:
#                       qmake DEFINES+="SPLASH_DIR=\'\\\"/usr/share/guymager\\\"\'"
#                    Remark: The backslash-tick-quote-nightmare comes from the fact,
#                    that bash eats some part of, qmake some more and finally again
#                    bash when Makefile calls g++.
#
#    LANGUAGE_DIR    Tells Guymager where to look for the language files. Usually the
#                    same than SPLASH_DIR.
#
#    LANGUAGE_DIR_QT The location of Qt's language files (i.e. the files qhere Qt
#                    stores language dependent texts fot its dialogs, messages, etc.)
#                    On a Debian system, this usually is /usr/share/qt4/translations.
#                    qmake DEFINES+="LANGUAGE_DIR_QT=\'\\\"/usr/share/qt4/translations\\\"\'"
#
#    ENABLE_LIBEWF   Enable Guymager for usage of libewf. Enabled by default. Use
#                       qmake DEFINES*="ENABLE_LIBEWF=0"
#                    to remove all libewf function calls and other references. This option
#                    has been added as Guymager anyway has its own EWF functions (configured
#                    by default in guymager.cfg, see parameter EwfFormat) and libewf's
#                    frequent API changes might cause compile problems.


lessThan(QT_MAJOR_VERSION, 5){
   CONFIG  = qt warn_on
   CONFIG += qdbus
} else {
   CONFIG += warn_on
   QT     += core gui widgets dbus
}
CONFIG += c++14

TARGET   = guymager
TEMPLATE = app
MOC_DIR  = ./moc

INCLUDEPATH += /usr/include/libguytools2

# Add compile time information
# ----------------------------
# Compile time information is written to compileinfo.cpp by means of the date command. The command in QMAKE_PRE_LINK ensures that
# compileinfo.cpp is rewritten and compiled whenever a new executable needs to be created. In order not to get a qmake error because
# of the missing compileinfo.o (the very first time when it is compiled), the line starting with 'DummyResult' creates a dummy
# compileinfo.o whenever qmake is called.

#QMAKE_PRE_LINK  = echo \'// Automatically generated file. See project file for further information.\' > compileinfo.cpp
#QMAKE_PRE_LINK += && date \'+const char *pCompileInfoTimestamp = \"%Y-%m-%d-%H.%M.%S\";\' >> compileinfo.cpp

QMAKE_PRE_LINK  = ./compileinfo.sh > compileinfo.cpp
QMAKE_PRE_LINK += && $(CXX) -c $(CXXFLAGS) compileinfo.cpp

DummyResult = $$system(echo DummyCompileInfoPunktO > compileinfo.o)
OBJECTS += compileinfo.o

SOURCES += aaff.cpp
SOURCES += aewf.cpp
SOURCES += config.cpp
SOURCES += device.cpp
SOURCES += dlgabort.cpp
SOURCES += dlgacquire.cpp
SOURCES += dlgdirsel.cpp
SOURCES += dlgautoexit.cpp
SOURCES += dlgmessage.cpp
SOURCES += dlgwait.cpp
SOURCES += error.cpp
SOURCES += fifo.cpp
SOURCES += file.cpp
SOURCES += hash.cpp
SOURCES += info.cpp
SOURCES += infofield.cpp
SOURCES += itemdelegate.cpp
SOURCES += main.cpp
SOURCES += mainwindow.cpp
SOURCES += md5.cpp
SOURCES += media.cpp
SOURCES += qtutil.cpp
SOURCES += runstats.cpp
SOURCES += sha1.cpp
SOURCES += sha256.cpp
SOURCES += table.cpp
SOURCES += thread.cpp
SOURCES += threadcompress.cpp
SOURCES += threadhash.cpp
SOURCES += threadread.cpp
SOURCES += threadscan.cpp
SOURCES += threadwrite.cpp
SOURCES += util.cpp

HEADERS += config.h
HEADERS += devicelistmodel.h
HEADERS += dlgabort.h
HEADERS += dlgacquire.h
HEADERS += dlgautoexit.h
HEADERS += dlgacquire_private.h
HEADERS += dlgdirsel.h
HEADERS += dlgdirsel_private.h
HEADERS += dlgmessage.h
HEADERS += dlgwait.h
HEADERS += info.h
HEADERS += infofield.h
HEADERS += itemdelegate.h
HEADERS += main_private.h
HEADERS += mainwindow.h
HEADERS += media.h
HEADERS += table.h
HEADERS += thread.h
HEADERS += threadcompress.h
HEADERS += threadhash.h
HEADERS += threadread.h
HEADERS += threadscan.h
HEADERS += threadwrite.h
HEADERS += util.h

QMAKE_CXXFLAGS *= $$system(dpkg-buildflags --get CXXFLAGS)
QMAKE_LFLAGS   *= $$system(dpkg-buildflags --get LDFLAGS)

QMAKE_CXXFLAGS         += -fmessage-length=0     # Tell g++ not to split messages into different lines
QMAKE_CXXFLAGS         += -fno-strict-aliasing   # Avoid strange error messages when using QVarLengthArray

QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -O3 -ggdb

QMAKE_LFLAGS           *= -ggdb -rdynamic    # -rdynamic is necessary in order to have the backtrace handler in toolsignal show all information
QMAKE_LFLAGS_RELEASE   -= -Wl,-O1
QMAKE_LFLAGS_RELEASE   += -Wl,-O3

# Enable Link time optimisation
# -----------------------------
QMAKE_CXXFLAGS         += -flto
QMAKE_LFLAGS           += -Wl,-flto

LIBS += -lz
LIBS += -ldl


!contains( DEFINES, "ENABLE_LIBEWF=0") {   # Corresponds to logic in common.h: ENABLE_LIBEWF must be set explicetly to 0 or else it is 1 (i.e. if it's not set or set to something different than 0)
   USE_LIBEWF=1
} else {
   USE_LIBEWF=0
}


STATIC = no
equals(STATIC, "yes") {
   exists ("/usr/local/lib/libguytools.a") {
      message ("Using /usr/local/lib/libguytools.a")
      LIBS += /usr/local/lib/libguytools.a
   }
   else {
      exists ("/usr/lib/libguytools.a") {
         message ("Using /usr/lib/libguytools.a")
         LIBS += /usr/lib/libguytools.a
      }
      else {
         error ("libguytools.a not found")
      }
   }  

   contains (USE_LIBEWF, 1) {
      exists("/usr/local/lib/libewf.a") {
         message ("Using /usr/local/lib/libewf.a")
         LIBS += /usr/local/lib/libewf.a
      }
      else {
         equals(QMAKE_HOST.arch, "i686") {
            message ("Using i386 32 bit libs")
            LIBS += /usr/lib/i386-linux-gnu/libewf.a
            LIBS += /usr/lib/i386-linux-gnu/libbfio.a
         }
         equals(QMAKE_HOST.arch, "x86_64") {
            message ("Using x86_64 64 bit libs")
            LIBS += /usr/lib/x86_64-linux-gnu/libewf.a
            LIBS += /usr/lib/x86_64-linux-gnu/libbfio.a
         }
      }
   }
}
else {
   LIBS += -lguytools
   contains (USE_LIBEWF, 1) {
      message ("Using libewf!")
      LIBS += -lewf
      LIBS += -lbfio
   }
}

contains (USE_LIBEWF, 0) {
   message ("Not using libewf!")
}

# You may use any of the 3 following lines for the libssl hash functions
#LIBS += -lssl     # See also macro definitions in common.h
#LIBS += -lcrypto  # See also macro definitions in common.h
#LIBS += /usr/lib/libcrypto.a

TRANSLATIONS  = guymager_en.ts
TRANSLATIONS += guymager_de.ts
TRANSLATIONS += guymager_fr.ts
TRANSLATIONS += guymager_it.ts
TRANSLATIONS += guymager_nl.ts
TRANSLATIONS += guymager_cn.ts
