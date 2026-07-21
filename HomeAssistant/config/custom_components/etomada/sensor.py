from homeassistant.components.sensor import SensorEntity
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN


async def async_setup_entry(
    hass,
    entry,
    async_add_entities,
):

    coordinator = hass.data[DOMAIN][entry.entry_id]

    entities = []

    for sensor in coordinator.data["sensores"]:

        if sensor["ativo"]:
            entities.append(
                ETomadaSensor(
                    coordinator,
                    sensor["num"],
                )
            )

    async_add_entities(entities)


class ETomadaSensor(
    CoordinatorEntity,
    SensorEntity,
):

    def __init__(
        self,
        coordinator,
        sensor_num,
    ):
        super().__init__(coordinator)

        self.sensor_num = sensor_num

    @property
    def sensor(self):

        for sensor in self.coordinator.data["sensores"]:

            if sensor["num"] == self.sensor_num:
                return sensor

        return None

    @property
    def name(self):
        return self.sensor["nome"]

    @property
    def unique_id(self):

        mac = self.coordinator.data.get(
            "mac",
            "unknown",
        )

        return f"{mac}_sensor_{self.sensor_num}"

    @property
    def native_value(self):
        return self.sensor["valor"]

    @property
    def extra_state_attributes(self):

        sensor = self.sensor

        return {
            "tipo": sensor["tipo"],
            "valorStr": sensor["valorStr"],
            "pino": sensor["pino"],
        }