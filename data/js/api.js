let SSE = null;

let ultimoEventoSSE = 0;
let offline = false;

const SSE_TIMEOUT = 50000;

function sseInit() {
  if (SSE) {
    SSE.close();
  }

  SSE = new EventSource(API_BASE + "/events");

  ultimoEventoSSE = Date.now();

  SSE.onopen = () => {
    console.log("SSE conectado");
    sseMarkOnline();
  };

  SSE.onerror = (err) => {
    console.log("Erro SSE", err);
  };

  // Refresh completo da tela
  SSE.addEventListener("sse_snapshot", (e) => {
    const snapshot = JSON.parse(e.data);
    console.log("SNAPSHOT");
    eTomadaRender(snapshot);
  });

  // Refresh de um rele
  SSE.addEventListener("sse_rele", (e) => {
    const rele = JSON.parse(e.data);
    releAtualiza(rele);
  });

  // refresh de um sensor
  SSE.addEventListener("sse_sensor", (e) => {
    const sensor = JSON.parse(e.data);
    sensorAtualiza(sensor);
  });

  SSE.addEventListener("sse_ping", () => {
    console.log("PONG!");
    sseMarkOnline();
  });

  setInterval(() => {
    const tempoSemEvento = Date.now() - ultimoEventoSSE;

    if (tempoSemEvento > SSE_TIMEOUT) {
      sseMarkOffline();
    }
  }, 2000);
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

function sseMarkOnline() {
  ultimoEventoSSE = Date.now();

  if (offline) {
    offline = false;

    document.getElementById("offlineOverlay").classList.remove("open");
    document.getElementById("offlineModal").classList.remove("open");

    statusMsg("");
  }
}

function sseMarkOffline() {
  if (offline) return;

  offline = true;

  document.getElementById("offlineOverlay").classList.add("open");
  document.getElementById("offlineModal").classList.add("open");

  statusMsg("eTomada offline");
}
