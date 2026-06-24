#include "CorsairCapellixXTPlugin.h"
#include "CorsairCapellixXTDetect.h"
#include "CorsairCapellixXTController.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFont>

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

    info.Label          = "";
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

    QLabel* title = new QLabel("Pump Speed Mode");
    QFont   tfont = title->font();
    tfont.setBold(true);
    title->setFont(tfont);
    layout->addWidget(title);

    QLabel* desc = new QLabel(
        "Controls the AIO pump and radiator fans together. Auto follows the "
        "liquid temperature (quiet at idle, ramps up under sustained load). "
        "Silent / Quiet / Balanced / Performance hold fixed speeds. Fans never "
        "drop below ~300 rpm. The choice is saved and restored automatically.");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    struct ModeDef { const char* label; int mode; };
    const ModeDef defs[] =
    {
        { "Auto (liquid-temp curve) — ~1130 rpm idle, ramps to full", PUMP_MODE_AUTO        },
        { "Silent (fixed, ~1130 rpm)",                                PUMP_MODE_SILENT        },
        { "Quiet — ~2150 rpm",                                        PUMP_MODE_QUIET       },
        { "Balanced — ~2500 rpm",                                     PUMP_MODE_BALANCED    },
        { "Performance — ~2800 rpm",                                  PUMP_MODE_PERFORMANCE },
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
