#----------------------------------------------------------------------
# OpenRGB Plugin: Corsair iCUE H150i Elite CAPELLIX XT
#
# Build:
#   export OPENRGB_DIR=/path/to/OpenRGB
#   qmake CorsairCapellixXT.pro
#   make -j$(nproc)
#----------------------------------------------------------------------

QT      -= gui
TEMPLATE = lib
CONFIG  += plugin c++17

TARGET   = OpenRGBCorsairCapellixXTPlugin

# Version info embedded in the plugin binary
VERSION_STRING = "0.1.0"
GIT_COMMIT_ID  = ""
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

INCLUDEPATH += \
    $$OPENRGB_DIR                                           \
    $$OPENRGB_DIR/RGBController                             \
    $$OPENRGB_DIR/ResourceManager                           \
    $$OPENRGB_DIR/plugins                                   \
    $$OPENRGB_DIR/dependencies/hidapi-0.14.0/hidapi         \
    $$OPENRGB_DIR/dependencies/hidapi                       \
    $$OPENRGB_DIR/i2c_smbus                                 \
    $$OPENRGB_DIR/net_port

#----------------------------------------------------------------------
# hidapi link flags (Linux)
#----------------------------------------------------------------------

unix:!macx {
    CONFIG  += link_pkgconfig
    PKGCONFIG += hidapi-hidraw
}

macx {
    LIBS += -framework IOKit -framework CoreFoundation
    LIBS += -lhidapi
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

#----------------------------------------------------------------------
# Install target
#----------------------------------------------------------------------

unix {
    target.path = $$[QT_INSTALL_PLUGINS]/OpenRGB
    INSTALLS += target
}
