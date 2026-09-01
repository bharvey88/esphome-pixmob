import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID

CODEOWNERS = ["@bharvey88"]
DEPENDENCIES = ["remote_transmitter", "cc1101"]

pixmob_ns = cg.esphome_ns.namespace("pixmob")
PixMobLight = pixmob_ns.class_("PixMobLight", light.LightOutput, cg.Component)

remote_transmitter_ns = cg.esphome_ns.namespace("remote_transmitter")
RemoteTransmitterComponent = remote_transmitter_ns.class_("RemoteTransmitterComponent")
cc1101_ns = cg.esphome_ns.namespace("cc1101")
CC1101Component = cc1101_ns.class_("CC1101Component")

CONF_TRANSMITTER_ID = "transmitter_id"
CONF_CC1101_ID = "cc1101_id"
CONF_GROUP = "group"
CONF_ATTACK = "attack"
CONF_HOLD = "hold"
CONF_RELEASE = "release"
CONF_RANDOM = "random"
CONF_REFRESH_INTERVAL = "refresh_interval"
CONF_OFF_REPEATS = "off_repeats"

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(PixMobLight),
        cv.Required(CONF_TRANSMITTER_ID): cv.use_id(RemoteTransmitterComponent),
        cv.Required(CONF_CC1101_ID): cv.use_id(CC1101Component),
        cv.Optional(CONF_GROUP, default=0): cv.int_range(min=0, max=31),
        cv.Optional(CONF_ATTACK, default=0): cv.int_range(min=0, max=7),
        cv.Optional(CONF_HOLD, default=7): cv.int_range(min=0, max=7),
        cv.Optional(CONF_RELEASE, default=2): cv.int_range(min=0, max=7),
        cv.Optional(CONF_RANDOM, default=0): cv.int_range(min=0, max=7),
        cv.Optional(
            CONF_REFRESH_INTERVAL, default="90ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_OFF_REPEATS, default=5): cv.int_range(min=1, max=20),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    transmitter = await cg.get_variable(config[CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(transmitter))
    radio = await cg.get_variable(config[CONF_CC1101_ID])
    cg.add(var.set_cc1101(radio))

    cg.add(var.set_group(config[CONF_GROUP]))
    cg.add(var.set_attack(config[CONF_ATTACK]))
    cg.add(var.set_hold(config[CONF_HOLD]))
    cg.add(var.set_release(config[CONF_RELEASE]))
    cg.add(var.set_random(config[CONF_RANDOM]))
    cg.add(var.set_refresh_interval(config[CONF_REFRESH_INTERVAL]))
    cg.add(var.set_off_repeats(config[CONF_OFF_REPEATS]))
