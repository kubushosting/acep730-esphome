# display.py for acep730 external component
# Place this file in: components/acep730/display.py

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display
from esphome.const import (
    CONF_ID,
)
from esphome import pins as pins_

# Gebruik het algemene PLATFORM_SCHEMA uit config_validation
DEPENDENCIES = []

# Verwijs naar de C++ klasse (globale namespace)
ACeP730Display = cg.class_('ACeP730Display', cg.PollingComponent)

CONF_CS_PIN = 'cs_pin'
CONF_DC_PIN = 'dc_pin'
CONF_RESET_PIN = 'reset_pin'
CONF_BUSY_PIN = 'busy_pin'
CONF_RAIL_EN_PIN = 'rail_en_pin'
CONF_SPI_FREQUENCY = 'spi_frequency'
CONF_POWER_OFF_AFTER_UPDATE = 'power_off_after_update'

# Gebruik cv.PLATFORM_SCHEMA zodat het compatibel is met meerdere ESPHome versies
PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(ACeP730Display),
    cv.Required(CONF_CS_PIN): pins_.gpio_output_pin_schema,
    cv.Required(CONF_DC_PIN): pins_.gpio_output_pin_schema,
    cv.Required(CONF_RESET_PIN): pins_.gpio_output_pin_schema,
    cv.Required(CONF_BUSY_PIN): pins_.gpio_input_pin_schema,
    cv.Optional(CONF_RAIL_EN_PIN, default=-1): cv.int_,
    cv.Optional(CONF_SPI_FREQUENCY, default=4000000): cv.positive_int,
    cv.Optional(CONF_POWER_OFF_AFTER_UPDATE, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    cs = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    busy = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])

    rail = config.get(CONF_RAIL_EN_PIN, -1)
    spi_freq = config.get(CONF_SPI_FREQUENCY, 4000000)
    power_off = config.get(CONF_POWER_OFF_AFTER_UPDATE, False)

    # Maak de C++ instantie; constructor: ACeP730Display(cs, dc, reset, busy, rail)
    var = cg.Pvariable(config[CONF_ID], cs, dc, reset, busy, rail)

    # Stel optionele eigenschappen in via methodes
    cg.add(var.set_spi_frequency(spi_freq))
    cg.add(var.set_power_off_after_update(power_off))

    # Registreer als display platform (maakt display entities mogelijk)
    await display.register_display(var, config)
