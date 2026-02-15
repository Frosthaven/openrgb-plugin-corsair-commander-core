#----------------------------------------------------------------------
# OpenRGB Plugin: Corsair iCUE H150i Elite CAPELLIX XT
#
# Build:
#   export OPENRGB_DIR=/path/to/OpenRGB
#   qmake CorsairCapellixXT.pro
#   make -j$(nproc)
#----------------------------------------------------------------------

QT      -= gui
QT      += svg
TEMPLATE = lib
CONFIG  += plugin c++17

TARGET   = OpenRGBCorsairCapellixXTPlugin

# Version info embedded in the plugin binary
VERSION_STRING = "0.1.0"
GIT_COMMIT_ID  = $$system(git rev-parse --short=8 HEAD)
DEFINES += VERSION_STRING=\\\"$$VERSION_STRING\\\"
DEFINES += GIT_COMMIT_ID=\\\"$$GIT_COMMIT_ID\\\"

#----------------------------------------------------------------------
# OpenRGB source tree (set via environment or override here)
#----------------------------------------------------------------------

isEmpty(OPENRGB_DIR) {
    OPENRGB_DIR = $$(OPENRGB_DIR)
}

isEmpty(OPENRGB_DIR) {
    error("OPENRGB_DIR is not set. Point it at your OpenRGB source checkout.")
}

#----------------------------------------------------------------------
# Include paths — OpenRGB headers and all internal subdirectories
#----------------------------------------------------------------------

INCLUDEPATH += $$OPENRGB_DIR

# Dynamically discover directories containing headers in the OpenRGB tree.
# This avoids adding non-source dirs (debian/, Documentation/, .git/) that
# would break compilation. Works on Linux, macOS, and MSYS2/MinGW.
INCLUDEPATH += $$system(find $$OPENRGB_DIR -name '*.h' | sed 's|/[^/]*$$||' | sort -u 2>/dev/null)

#----------------------------------------------------------------------
# hidapi link flags
#----------------------------------------------------------------------

unix:!macx {
    CONFIG  += link_pkgconfig
    PKGCONFIG += hidapi-hidraw
}

macx {
    LIBS += -framework IOKit -framework CoreFoundation
    CONFIG  += link_pkgconfig
    PKGCONFIG += hidapi
    # Ensure C++17 filesystem support on macOS
    QMAKE_CXXFLAGS += -std=c++17
}

win32 {
    LIBS += -lhidapi
}

#----------------------------------------------------------------------
# Plugin sources
#----------------------------------------------------------------------

HEADERS += \
    src/CorsairCapellixXTPlugin.h           \
    src/CorsairCapellixXTController.h       \
    src/RGBController_CorsairCapellixXT.h   \
    src/CorsairCapellixXTDetect.h

SOURCES += \
    src/CorsairCapellixXTPlugin.cpp         \
    src/CorsairCapellixXTController.cpp     \
    src/RGBController_CorsairCapellixXT.cpp \
    src/CorsairCapellixXTDetect.cpp

RESOURCES += \
    resources/resources.qrc

#----------------------------------------------------------------------
# Install target
#----------------------------------------------------------------------

unix {
    target.path = $$[QT_INSTALL_PLUGINS]/OpenRGB
    INSTALLS += target
}
