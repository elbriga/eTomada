import asyncio
import json
import logging

import aiohttp
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.update_coordinator import (
    DataUpdateCoordinator,
    UpdateFailed,
)

from .const import DOMAIN

_LOGGER = logging.getLogger(__name__)


class ETomadaCoordinator(DataUpdateCoordinator):

    def __init__(self, hass, host, port):
        self.host = host
        self.port = port
        self._session = None
        self._stream_task = None
        self.connected = False

        super().__init__(
            hass,
            logger=_LOGGER,
            name=f"eTomada {host}",
            update_interval=None,
        )

    @property
    def base_url(self):
        port = f":{self.port}" if self.port != 80 else ""
        return f"http://{self.host}{port}"

    @property
    def mac(self):
        return self.data.get("mac") or self.data.get("device_id") or self.host

    @property
    def device_info(self):
        return DeviceInfo(
            identifiers={(DOMAIN, self.mac)},
            name=self.data.get("device_id", "eTomada"),
            manufacturer="eTomada",
            model=self.data.get("device_model"),
            sw_version=self.data.get("fw_version"),
        )

    def get_resource(self, resource_id):
        for resource in self.data.get("recursos", []):
            if resource.get("id") == resource_id:
                return resource
        return {}

    async def _async_get_session(self):
        if self._session is None or self._session.closed:
            timeout = aiohttp.ClientTimeout(
                total=None,
                connect=10,
                sock_connect=10,
                sock_read=75,
            )
            self._session = aiohttp.ClientSession(timeout=timeout)
        return self._session

    async def _async_update_data(self):
        session = await self._async_get_session()
        try:
            async with session.get(f"{self.base_url}/api/getSnapshot") as response:
                response.raise_for_status()
                return await response.json(content_type=None)
        except (aiohttp.ClientError, asyncio.TimeoutError) as err:
            raise UpdateFailed(f"Não foi possível ler o snapshot: {err}") from err

    async def async_start(self):
        self._stream_task = self.hass.async_create_task(
            self._async_stream_events(),
            name=f"eTomada SSE {self.host}",
        )

    async def _async_stream_events(self):
        retry_delay = 1
        while True:
            try:
                session = await self._async_get_session()
                async with session.get(
                    f"{self.base_url}/events",
                    headers={"Accept": "text/event-stream"},
                ) as response:
                    response.raise_for_status()
                    self._set_connected(True)
                    retry_delay = 1
                    await self._async_read_events(response)
            except asyncio.CancelledError:
                raise
            except (aiohttp.ClientError, asyncio.TimeoutError, OSError) as err:
                _LOGGER.warning("Conexão SSE com eTomada %s perdida: %s", self.host, err)
            except Exception:
                _LOGGER.exception("Erro ao processar eventos SSE de eTomada %s", self.host)

            self._set_connected(False)
            await asyncio.sleep(retry_delay)
            retry_delay = min(retry_delay * 2, 60)

    async def _async_read_events(self, response):
        event_name = "message"
        event_data = []

        async for raw_line in response.content:
            line = raw_line.decode("utf-8").rstrip("\r\n")
            if not line:
                if event_data:
                    self._async_process_event(event_name, "\n".join(event_data))
                event_name = "message"
                event_data = []
                continue

            if line.startswith(":"):
                continue

            field, separator, value = line.partition(":")
            if separator and value.startswith(" "):
                value = value[1:]
            if field == "event":
                event_name = value
            elif field == "data":
                event_data.append(value)

        if event_data:
            self._async_process_event(event_name, "\n".join(event_data))

    def _async_process_event(self, event_name, payload):
        if event_name == "sse_ping":
            self._set_connected(True)
            return

        if event_name not in ("sse_snapshot", "sse_recurso"):
            return

        try:
            event_data = json.loads(payload)
        except json.JSONDecodeError:
            _LOGGER.warning("Payload SSE inválido recebido de eTomada %s", self.host)
            return

        if event_name == "sse_snapshot":
            if isinstance(event_data, dict):
                self._set_connected(True)
                self.async_set_updated_data(event_data)
            return

        if not isinstance(event_data, dict) or not isinstance(self.data, dict):
            return

        resources = list(self.data.get("recursos", []))
        for index, resource in enumerate(resources):
            if resource.get("id") == event_data.get("id"):
                resources[index] = event_data
                break
        else:
            resources.append(event_data)

        updated_data = dict(self.data)
        updated_data["recursos"] = resources
        self._set_connected(True)
        self.async_set_updated_data(updated_data)

    def _set_connected(self, connected):
        if self.connected == connected:
            return
        self.connected = connected
        self.async_update_listeners()

    async def set_resource(self, resource_id, state):
        session = await self._async_get_session()
        payload = {
            "id": resource_id,
            "estado": "ON" if state else "OFF",
        }
        async with session.put(
            f"{self.base_url}/api/setRecurso",
            json=payload,
        ) as response:
            response.raise_for_status()
            result = await response.json(content_type=None)

        message = result.get("msg", "")
        if not message.startswith(("Ligando", "Desligando", "Rele ", "SIM MESTRE!:")):
            raise HomeAssistantError(message or "O eTomada não confirmou o comando")

    async def async_close(self):
        if self._stream_task is not None:
            self._stream_task.cancel()
            try:
                await self._stream_task
            except asyncio.CancelledError:
                pass
            self._stream_task = None
        if self._session is not None:
            await self._session.close()