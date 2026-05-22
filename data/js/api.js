function sseInit() {
  const evt = new EventSource(API_BASE + "/events");

  evt.onopen = () => {
    console.log("SSE conectado");
  };

  evt.onerror = (err) => {
    console.log("Erro SSE", err);
  };

  evt.addEventListener("sse_rele", (e) => {
    const rele = JSON.parse(e.data);
    releAtualiza(rele);
  });

  evt.addEventListener("sse_sensor", (e) => {
    const sensor = JSON.parse(e.data);
    sensorAtualiza(sensor);
  });
}

async function eTomadaAPI(
  endpoint,
  body = undefined,
  method = "GET",
  timeout = 5000,
) {
  const controller = new AbortController();
  const timer = setTimeout(() => {
    controller.abort();
  }, timeout);

  let httpConfig = {
    method,
    signal: controller.signal,
    headers: {
      "Content-type": "application/json; charset=UTF-8",
    },
  };
  if (body != undefined) {
    httpConfig.body = JSON.stringify(body);
  }

  try {
    const res = await fetch(`${API_BASE}/api/${endpoint}`, httpConfig);
    if (!res.ok) {
      let erro = `HTTP ${res.status}`;

      try {
        const body = await res.json();
        if (body.msg) erro = body.msg;
      } catch {}

      throw new Error(erro);
    }

    const data = await res.json();
    return data;
  } finally {
    clearTimeout(timer);
  }
}
