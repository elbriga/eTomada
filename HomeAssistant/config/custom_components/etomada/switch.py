from homeassistant.components.switch import SwitchEntity
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN


async def async_setup_entry(hass, entry, async_add_entities):
    coordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities(
        ETomadaSwitch(coordinator, resource)
        for resource in coordinator.data.get("recursos", [])
        if resource.get("tipo") == "RELE"
    )


class ETomadaSwitch(CoordinatorEntity, SwitchEntity):
    def __init__(self, coordinator, resource):
        super().__init__(coordinator)
        self.resource_id = resource["id"]

    @property
    def resource(self):
        return self.coordinator.get_resource(self.resource_id)

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
    def is_on(self):
        return self.resource.get("device", {}).get("estado")

    async def async_turn_on(self, **kwargs):
        await self.coordinator.set_resource(self.resource_id, True)

    async def async_turn_off(self, **kwargs):
        await self.coordinator.set_resource(self.resource_id, False)
        