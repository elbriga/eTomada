from homeassistant.components.zeroconf import ZeroconfServiceInfo
from homeassistant.config_entries import ConfigFlow

from .const import CONF_DEVICE_ID, DOMAIN


class ETomadaConfigFlow(ConfigFlow, domain=DOMAIN):
    VERSION = 1

    async def async_step_zeroconf(self, discovery_info: ZeroconfServiceInfo):
        properties = discovery_info.properties
        device_id = properties.get("id") or discovery_info.hostname.rstrip(".")
        await self.async_set_unique_id(device_id)
        self._abort_if_unique_id_configured(
            updates={"host": discovery_info.host, "port": discovery_info.port}
        )

        title = properties.get("id") or discovery_info.name
        return self.async_create_entry(
            title=title,
            data={
                "host": discovery_info.host,
                "port": discovery_info.port,
                CONF_DEVICE_ID: device_id,
                "model": properties.get("model", "eTomada"),
            },
        )