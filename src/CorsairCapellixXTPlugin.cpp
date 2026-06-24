#include "CorsairCapellixXTPlugin.h"
#include "CorsairCapellixXTDetect.h"
#include "CorsairCapellixXTController.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFont>
#include <QLineEdit>
#include <QPushButton>
#include <QApplication>
#include <QClipboard>
#include <cstdlib>

OpenRGBPluginInfo CorsairCapellixXTPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;

    info.Name           = "Corsair Commander Core";
    info.Description    = "Adds support for Corsair Commander Core RGB controllers "
                          "used in Capellix and Capellix XT series AIO coolers.";
    info.Version        = VERSION_STRING;
    info.Commit         = GIT_COMMIT_ID;
    info.URL            = "https://github.com/Frosthaven/openrgb-plugin-corsair-commander-core";
    info.Icon.load(":/fan.svg");

    info.Label          = "Commander Core Cooling";
    info.Location       = OPENRGB_PLUGIN_LOCATION_TOP;

    return info;
}

unsigned int CorsairCapellixXTPlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

void CorsairCapellixXTPlugin::Load(ResourceManagerInterface* rm)
{
    if(loaded)
    {
        return;
    }

    resource_manager = rm;

    /*-------------------------------------------------------------*\
    | Detect devices and register controllers directly               |
    \*-------------------------------------------------------------*/
    std::vector<RGBController*> detected = DetectCorsairCapellixXT(&pump_controllers);

    for(RGBController* ctrl : detected)
    {
        controllers.push_back(ctrl);
        resource_manager->RegisterRGBController(ctrl);
    }

    loaded = true;
}

QWidget* CorsairCapellixXTPlugin::GetWidget()
{
    QWidget*     widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);

    /*-----------------------------------------------------------------*\
    | If nothing was detected, show a clear message instead of dead     |
    | controls. OpenRGB always shows a loaded plugin's tab (it cannot   |
    | be hidden), so we make the empty state explicit.                  |
    \*-----------------------------------------------------------------*/
    if(pump_controllers.empty())
    {
        QLabel* none = new QLabel(
            "No Corsair Commander Core (Capellix / Capellix XT AIO) detected.\n\n"
            "Connect the cooler, and make sure OpenRGB's built-in \"Corsair "
            "Commander Core\" and \"Corsair Commander Core XT\" detectors are "
            "unchecked in Settings > Supported Devices.");
        none->setWordWrap(true);
        layout->addWidget(none);
        layout->addStretch();
        return widget;
    }

    /*-----------------------------------------------------------------*\
    | Title shows the actual detected device so the user knows which    |
    | cooler this tab is controlling.                                   |
    \*-----------------------------------------------------------------*/
    QString devName = QString::fromStdString(pump_controllers[0]->GetDeviceName()).trimmed();
    if(devName.isEmpty()) { devName = "Corsair Commander Core"; }

    QLabel* title = new QLabel(devName + "  -  Pump & Fan Speed");
    QFont   tfont = title->font();
    tfont.setBold(true);
    title->setFont(tfont);
    layout->addWidget(title);

    QLabel* desc = new QLabel(
        "Corsair Commander Core (Capellix AIO). Controls the pump and radiator "
        "fans together. Auto follows the liquid temperature; at idle it runs "
        "at the Silent floor and ramps up only under load. Silent, Quiet, "
        "Balanced and Performance hold "
        "fixed speeds. Disabled stops managing them so they run on their own or "
        "under another tool. Fans never drop below their stall floor. The "
        "selected mode is saved to the config file shown below, so other tools "
        "can read it and stay in sync.");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    struct ModeDef { const char* label; int mode; };
    const ModeDef defs[] =
    {
        { "Disabled (hands off, pump and fans run externally)",           PUMP_MODE_DISABLED },
        { "Auto (follows liquid temperature, ~1130 rpm idle)", PUMP_MODE_AUTO        },
        { "Silent (~1130 rpm)",                                PUMP_MODE_SILENT        },
        { "Quiet (~2150 rpm)",                                        PUMP_MODE_QUIET       },
        { "Balanced (~2500 rpm)",                                     PUMP_MODE_BALANCED    },
        { "Performance (~2800 rpm)",                                  PUMP_MODE_PERFORMANCE },
    };

    int current = pump_controllers.empty()
                ? PUMP_MODE_AUTO
                : pump_controllers[0]->GetPumpMode();

    QButtonGroup* group = new QButtonGroup(widget);
    for(const ModeDef& d : defs)
    {
        QRadioButton* rb = new QRadioButton(QString::fromUtf8(d.label));
        rb->setChecked(d.mode == current);
        group->addButton(rb, d.mode);
        layout->addWidget(rb);
    }

    QObject::connect(group, &QButtonGroup::idClicked,
        [this](int mode)
        {
            for(CorsairCapellixXTController* c : pump_controllers)
            {
                c->SetPumpMode(mode);
            }
        });

    /*-----------------------------------------------------------------*\
    | Config path: the selected mode is persisted here so other tools  |
    | read it and stay in sync. Shown + copyable in the pane so it is    |
    | easy to find for fan-speed configuration or scripting.            |
    \*-----------------------------------------------------------------*/
    const char* home = getenv("HOME");
    QString cfgDir = (home ? QString::fromUtf8(home) : QString())
                   + "/.config/OpenRGB/plugins/settings/";

    QLabel* pathLbl = new QLabel(
        "Mode is saved here. Other tools can read this file to stay in sync:");
    pathLbl->setWordWrap(true);
    layout->addWidget(pathLbl);

    QHBoxLayout* pathRow = new QHBoxLayout();
    QLineEdit*   pathEdit = new QLineEdit(cfgDir + "CommanderCorePump.conf");
    pathEdit->setReadOnly(true);
    pathEdit->setCursorPosition(0);
    QPushButton* copyBtn = new QPushButton("Copy folder path");
    pathRow->addWidget(pathEdit);
    pathRow->addWidget(copyBtn);
    layout->addLayout(pathRow);

    QObject::connect(copyBtn, &QPushButton::clicked,
        [cfgDir]() { QApplication::clipboard()->setText(cfgDir); });

    layout->addStretch();
    return widget;
}

QMenu* CorsairCapellixXTPlugin::GetTrayMenu()
{
    /* No tray menu */
    return nullptr;
}

void CorsairCapellixXTPlugin::Unload()
{
    for(RGBController* ctrl : controllers)
    {
        resource_manager->UnregisterRGBController(ctrl);
    }
    controllers.clear();
    loaded = false;
}
