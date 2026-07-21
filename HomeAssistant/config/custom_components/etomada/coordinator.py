from datetime import timedelta

import aiohttp

from homeassistant.helpers.update_coordinator import (
    DataUpdateCoordinator,
)


class ETomadaCoordinator(DataUpdateCoordinator):

    def __init__(self, hass, host):
        self.host = host

        super().__init__(
            hass,
            logger=None,
            name="etomada",
            update_interval=timedelta(seconds=5),
        )

    async def _async_update_data(self):
        async with aiohttp.ClientSession() as session:
            async with session.get(
                f"http://{self.host}/api/getSnapshot"
            ) as resp:
                return await resp.json()

    async def set_rele(self, rele, estado):

        payload = {
            "rele": rele,
            "estado": "1" if estado else "0",
        }

        async with aiohttp.ClientSession() as session:

            await session.post(
                f"http://{self.host}/api/setRele",
                json=payload,
            )

        await self.async_request_refresh()