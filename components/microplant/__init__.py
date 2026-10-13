"""MicroPlant: the ESPHome side. The three settings of plugin.yaml (demo data, how often to ask Home Assistant, and the
screen's own last watering) are ESPHome entities; this component is handed them and registers the tile, the card and
the top bar item (microplant.cpp). register_plugin() reads the id, version, tile memory and the screen's texts from the
manifest and translations/ beside this folder (docs/FIRMWARE_API.md of the plugins repository)."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, smart_display, switch, text
from esphome.const import CONF_ID

# The core component every Tessera screen has; the plugin API lives in it.
DEPENDENCIES = ["smart_display"]
AUTO_LOAD = ["switch", "number", "text"]

CONF_DEMO = "demo"
CONF_POLL = "poll"
CONF_WATERED = "watered"

microplant_ns = cg.esphome_ns.namespace("microplant")
Microplant = microplant_ns.class_("Microplant", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Microplant),
        cv.Required(CONF_DEMO): cv.use_id(switch.Switch),
        cv.Required(CONF_POLL): cv.use_id(number.Number),
        cv.Required(CONF_WATERED): cv.use_id(text.Text),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    cg.add(var.set_demo_switch(await cg.get_variable(config[CONF_DEMO])))
    cg.add(var.set_poll_number(await cg.get_variable(config[CONF_POLL])))
    cg.add(var.set_watered_text(await cg.get_variable(config[CONF_WATERED])))
