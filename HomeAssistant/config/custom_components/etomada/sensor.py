from homeassistant.components.sensor import SensorEntity
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN


async def async_setup_entry(hass, entry, async_add_entities):
    coordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities(
        ETomadaSensor(coordinator, resource)
        for resource in coordinator.data.get("recursos", [])
        if resource.get("tipo") == "SENSOR"
    )


class ETomadaSensor(CoordinatorEntity, SensorEntity):
    def __init__(self, coordinator, resource):
        super().__init__(coordinator)
        self.resource_id = resource["id"]

    @property
    def resource(self):
        return self.coordinator.get_resource(self.resource_id)

    @property
    def sensor(self):
        return self.resource.get("device", {})

    @property
    def name(self):
        return self.resource.get("nome") or self.resource_id

    @property
    def unique_id(self):
        return f"{self.coordinator.mac}_{self.resource_id}"

    @property
    def device_info(self):
        return self.coordinator.device_info

    @property
    def available(self):
        return self.coordinator.connected and super().available

    @property
    def native_value(self):
        return self.sensor.get("valor")

    @property
    def native_unit_of_measurement(self):
        return self.sensor.get("unidade")

    @property
    def extra_state_attributes(self):
        return {
            "tipo": self.sensor.get("tipo"),
            "categoria": self.sensor.get("categoria"),
            "status": self.sensor.get("status"),
            "pino": self.sensor.get("pino"),
        }