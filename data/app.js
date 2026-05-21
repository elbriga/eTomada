const API_BASE =
  window.location.host === "localhost"
    ? "http://192.168.18.105" // IP do ESP quando o frontend esta hospedado para DEV
    : window.location.origin;

let eTomadaData = null;
let releEditando = null;

function getRegraTXT(regra) {
  if (!regra || regra.trim() === "") {
    return "Modo Manual";
  }

  regra = regra.trim();

  const [acao, param1, param2] = regra.split("|");

  if (acao === "ON" || acao === "OF") {
    return (
      (acao === "ON" ? "Ligado" : "Desligado") +
      " das " +
      param1 +
      " às " +
      param2
    );
  }

  if (acao === "SE") {
    return (
      (param1 !== "" ? `Ligar SE ${param1}` : "") +
      (param1 !== "" && param2 != "" ? "<br>\n" : "") +
      (param2 !== "" ? `Desligar SE ${param2}` : "")
    );
  }

  return "??" + regra;
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

let loading = false;
async function load() {
  statusMsg("");

  if (loading) return;
  loading = true;

  try {
    eTomadaData = await eTomadaAPI("data");

    if (!eTomadaData.reles || !eTomadaData.sensores) {
      // TODO msg de erro!
      return;
    }

    document.getElementById("datahora").innerHTML =
      "uptime: " +
      formataTempo(eTomadaData.uptime) +
      " - " +
      eTomadaData.datahorastr;

    renderReles(eTomadaData.reles);
    renderSensores(eTomadaData.sensores);
  } catch (e) {
    statusMsg("Erro ao carregar: " + e);
  } finally {
    loading = false;
  }
}

function releGetCard(rele) {
  const card = document.createElement("div");
  card.id = "tomadaCard-" + rele.num;
  card.className = "card cardRele";
  card.innerHTML = `
<div class="medio">Tomada ${rele.num}</div>
<div class="title">${escapeHtml(rele.nome || "")}</div>
<div class="medio">${getRegraTXT(rele.regra)}</div>
<div class="small">pino: ${escapeHtml(rele.pino)}</div>
<br>

<div class="status ${rele.estado ? "on" : "off"}">
  ${rele.estado ? "● Ligado" : "● Desligado"}
  ${rele.override > Date.now() / 1000 && rele.regra != "" ? ` (até ${getHoraFromTS(rele.override)})` : ""}
</div>

<button onclick="tomadaOverride(${rele.num}, ${rele.estado ? "false" : "true"}, this)">
  ${rele.estado ? "🔴 Desligar" : "🟢 Ligar"}${rele.regra == "" ? "" : " por 30 minutos"}
</button>
<br><br>
<button onclick="openReleModal(${rele.num})">
  ✏️ Editar
</button>
`;
  return card;
}

function renderReles(reles) {
  const container = document.getElementById("reles");
  container.innerHTML = "";

  reles.forEach((rele, i) => {
    if (!rele.ativo) return;
    if (!rele.nome) rele.nome = "---";

    const card = releGetCard(rele);
    container.appendChild(card);
  });
}

function openReleModal(numRele) {
  const rele = eTomadaData.reles.find((r) => r.num == numRele);

  if (!rele) return;

  releEditando = numRele;

  document.getElementById("modalTitle").innerHTML = "Editar Tomada " + numRele;
  document.getElementById("modalNome").value = rele.nome || "";
  document.getElementById("modalRegra").value = rele.regra || "";
  document.getElementById("modalSalvarBtn").onclick = function () {
    tomadaSalvarModal();
  };

  document.getElementById("modalOverlay").classList.add("open");
  document.getElementById("editModal").classList.add("open");
}

function closeModal() {
  releEditando = null;

  document.getElementById("modalOverlay").classList.remove("open");
  document.getElementById("editModal").classList.remove("open");
}

function sensorGetCard(sensor) {
  const card = document.createElement("div");
  card.id = "sensorCard-" + sensor.num;
  card.className = "card cardSensor";
  card.innerHTML = `
<div class="medio">Sensor ${sensor.num}</div>
<div class="title">${escapeHtml(sensor.nome || "")}</div>
<div class="small">pino: ${escapeHtml(sensor.pino)}</div>
<br>
<div class="status on">${sensor.valorStr}</div>
<button onclick="sensorToggleEdit(${sensor.num}, true)">✏️ Editar</button>
`;
  return card;
}

function renderSensores(sensores) {
  const container = document.getElementById("sensores");
  container.innerHTML = "";

  sensores.forEach((sensor, i) => {
    if (!sensor.tipo) return;
    if (!sensor.nome) sensor.nome = "---";

    const card = sensorGetCard(sensor);
    container.appendChild(card);
  });
}

async function tomadaSalvarModal() {
  if (releEditando == null) return;

  const btn = document.getElementById("modalSalvarBtn");

  btn.disabled = true;
  btn.innerText = "Salvando...";

  try {
    await eTomadaAPI(
      "setReleConfig",
      {
        rele: releEditando,
        nome: document.getElementById("modalNome").value,
        regra: document.getElementById("modalRegra").value,
      },
      "PUT",
    );

    closeModal();

    await load();
  } catch (e) {
    statusMsg("Erro ao salvar: " + e);
  } finally {
    btn.disabled = false;
    btn.innerText = "💾 Salvar";
  }
}

async function tomadaOverride(numRele, novoEstado, btn) {
  btn.innerText = "Processando...";
  btn.disabled = true;

  try {
    await eTomadaAPI(
      "setRele",
      {
        rele: numRele,
        estado: novoEstado ? "1" : "0",
      },
      "PUT",
    );
  } catch (e) {
    statusMsg("Erro ao setar: " + e);
  }

  // load recria o HTML com o botão habilitado
  //load();
}

async function openConfig() {
  try {
    eTomadaData = await eTomadaAPI("data");
    renderConfig();
  } catch (e) {
    statusMsg("Erro openConfig: " + e);
    throw e;
  }

  document.getElementById("configPanel").classList.add("open");
  document.getElementById("configOverlay").classList.add("open");
}

function closeConfig() {
  document.getElementById("configPanel").classList.remove("open");
  document.getElementById("configOverlay").classList.remove("open");
}

function renderConfig() {
  const container = document.getElementById("configContent");

  let html = "";

  eTomadaData.reles.forEach((rele, i) => {
    const num = i + 1;

    html += `
<div class="configCard cardRele">
  <div class="headerTop">
    <div><b>Tomada ${num}</b></div>
    <span>
      <select id="cfg-pino-${num}">
        ${getPinosOptions(rele.pino)}
      </select>
    </span>
  </div>
</div>
`;
  });

  eTomadaData.sensores.forEach((sensor, i) => {
    const num = i + 1;

    html += `
<div class="configCard cardSensor">
  <div class="headerTop">
    <div><b>Sensor ${num}</b></div>
    <span>
      <select id="cfg-sensor-${num}">
        ${getTipoSensorOptions(sensor.tipo)}
      </select>
    </span>
  </div>
</div>
`;
  });

  container.innerHTML = html;
}

function getPinosOptions(selected) {
  const pinos = [13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33];

  return (
    `<option value="-1">Desativada!</option>\n` +
    pinos
      .map((p) => {
        return `
<option value="${p}" ${p == selected ? "selected" : ""}>
  GPIO ${p}
</option>
`;
      })
      .join("")
  );
}

function getTipoSensorOptions(selected) {
  return (
    `<option value="0">Desativado!</option>\n` +
    eTomadaData.tipoSensores
      .map((ts) => {
        return `
<option value="${ts.num}" ${ts.num == selected ? "selected" : ""}>
  ${ts.nome}
</option>
`;
      })
      .join("")
  );
}

async function salvarConfigGeral() {
  try {
    await Promise.all(
      eTomadaData.reles.map(async (old, i) => {
        const rele = i + 1;
        const pino = parseInt(
          document.getElementById(`cfg-pino-${rele}`).value,
        );
        const ativo = pino != -1;

        if (old.ativo != ativo || old.pino != pino) {
          await eTomadaAPI(
            "setReleConfig",
            {
              rele,
              ativo,
              pino,
            },
            "PUT",
          );
        }
      }),
    );

    statusMsg("Configuração salva");

    closeConfig();

    load();
  } catch (e) {
    statusMsg("Erro salvarConfigGeral: " + e);
  }
}

function escapeHtml(str) {
  return String(str)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function getHoraFromTS(ts) {
  var date = new Date(ts * 1000);
  var h = "0" + date.getHours();
  var m = "0" + date.getMinutes();
  return h.slice(-2) + ":" + m.slice(-2);
}

function formataTempo(millis) {
  const secs = Math.floor(millis / 1000);

  const dias = Math.floor(secs / 86400);
  const horas = Math.floor((secs % 86400) / 3600);
  const minutos = Math.floor((secs % 3600) / 60);
  const segundos = secs % 60;

  const tempo =
    String(horas).padStart(2, "0") +
    ":" +
    String(minutos).padStart(2, "0") +
    ":" +
    String(segundos).padStart(2, "0");

  if (dias > 0) {
    return `${dias}d ${tempo}`;
  }

  return tempo;
}

function statusMsg(msg) {
  document.getElementById("status").innerHTML = msg;
}

function init() {
  const evt = new EventSource(API_BASE + "/events");

  evt.onopen = () => {
    console.log("SSE conectado");
  };

  evt.onerror = (err) => {
    console.log("Erro SSE", err);
  };

  evt.addEventListener("sse_rele", (e) => {
    const rele = JSON.parse(e.data);
    const newCard = releGetCard(rele);
    const oldCard = document.getElementById(`tomadaCard-${rele.num}`);
    oldCard.parentNode.replaceChild(newCard, oldCard);
  });

  evt.addEventListener("sse_sensor", (e) => {
    const sensor = JSON.parse(e.data);
    const newCard = sensorGetCard(sensor);
    const oldCard = document.getElementById(`sensorCard-${sensor.num}`);
    oldCard.parentNode.replaceChild(newCard, oldCard);
  });

  load();
}
