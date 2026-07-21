from homeassistant.components.switch import SwitchEntity
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN


async def async_setup_entry(
    hass,
    entry,
    async_add_entities,
):

    coordinator = hass.data[DOMAIN][entry.entry_id]

    entities = []

    for rele in coordinator.data["reles"]:

        if rele["ativo"]:
            entities.append(
                ETomadaSwitch(
                    coordinator,
                    rele["num"],
                )
            )

    async_add_entities(entities)


class ETomadaSwitch(
    CoordinatorEntity,
    SwitchEntity,
):

    def __init__(
        self,
        coordinator,
        rele_num,
    ):
        super().__init__(coordinator)

        self.rele_num = rele_num

    @property
    def rele(self):

        for rele in self.coordinator.data["reles"]:

            if rele["num"] == self.rele_num:
                return rele

        return None

    @property
    def name(self):
        return self.rele["nome"]

    @property
    def unique_id(self):

        mac = self.coordinator.data.get(
            "mac",
            "unknown",
        )

        return f"{mac}_rele_{self.rele_num}"

    @property
    def is_on(self):
        return self.rele["estado"]

    async def async_turn_on(self, **kwargs):
        await self.coordinator.set_rele(
            self.rele_num,
            True,
        )

    async def async_turn_off(self, **kwargs):
        await self.coordinator.set_rele(
            self.rele_num,
            False,
        )
        