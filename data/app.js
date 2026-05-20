const API_BASE =
  window.location.host === "localhost"
    ? "http://192.168.18.105" // IP do ESP quando o frontend esta hospedado para DEV
    : window.location.origin;

let configData = null;

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

async function tomadaAPI(
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
let editingTomada = null;

async function load() {
  statusMsg("");

  if (editingTomada !== null) {
    statusMsg("Reload desligado ao editar");
    return;
  }

  if (loading) return;
  loading = true;

  try {
    const data = await tomadaAPI("data");

    if (!data.reles || !data.sensores) {
      // TODO msg de erro!
      return;
    }

    document.getElementById("datahora").innerHTML =
      "uptime: " + formataTempo(data.uptime) + " - " + data.datahorastr;

    renderReles(data.reles);
    renderSensores(data.sensores);
  } catch (e) {
    statusMsg("Erro ao carregar: " + e);
  } finally {
    loading = false;
  }
}

function renderReles(reles) {
  const container = document.getElementById("reles");
  container.innerHTML = "";

  reles.forEach((rele, i) => {
    if (!rele.ativo) return;
    if (!rele.nome) rele.nome = "---";

    let numRele = rele.num;

    const card = document.createElement("div");
    card.id = "tomadaCard-" + numRele;
    card.className = "card cardRele";

    let html = `
<div class="medio">Tomada ${numRele}</div>
<div class="title">${escapeHtml(rele.nome || "")}</div>
<div class="medio">${getRegraTXT(rele.regra)}</div>
<div class="small">pino: ${escapeHtml(rele.pino)}</div>
<br>

<div class="status ${rele.estado ? "on" : "off"}">
  ${rele.estado ? "● Ligado" : "● Desligado"}
  ${rele.override > Date.now() / 1000 && rele.regra != "" ? ` (Manual até ${getHoraFromTS(rele.override)})` : ""}
</div>

<div id="tomadaEdit-${numRele}" style="display: none">
  Nome: <input id="nome-${numRele}" value="${escapeHtml(rele.nome || "")}"><br>
  Regra: <input id="regra-${numRele}" value="${escapeHtml(rele.regra || "")}" placeholder="ON=08:00-18:00" maxlength="31">
  <button onclick="tomadaSalvar(${numRele}, this)">💾 Salvar</button>
  <br><br>
  <button onclick="tomadaToggleEdit(${numRele}, false)">❌ Cancelar</button>
</div>

<div id="tomadaView-${numRele}">
  <button onclick="tomadaOverride(${numRele}, ${rele.estado ? "false" : "true"}, this)">
    ${rele.estado ? "🔴 Desligar" : "🟢 Ligar"}${rele.regra == "" ? "" : " por 30 minutos"}
  </button>
  <br><br>
  <button onclick="tomadaToggleEdit(${numRele}, true)">✏️ Editar</button>
</div>
`;

    card.innerHTML = html;
    container.appendChild(card);
  });
}

function renderSensores(sensores) {
  const container = document.getElementById("sensores");
  container.innerHTML = "";

  sensores.forEach((sensor, i) => {
    if (!sensor.tipo) return;
    if (!sensor.nome) sensor.nome = "---";

    let numSensor = sensor.num;

    const card = document.createElement("div");
    card.id = "sensorCard-" + numSensor;
    card.className = "card cardSensor";

    let html = `
<div class="medio">Sensor ${numSensor}</div>
<div class="title">${escapeHtml(sensor.nome || "")}</div>
<div class="small">pino: ${escapeHtml(sensor.pino)}</div>
<br>

<div class="status on">${sensor.valorStr}</div>

<div id="sensorEdit-${numSensor}" style="display: none">
  Nome: <input id="nomeSensor-${numSensor}" value="${escapeHtml(sensor.nome || "")}"><br>
  <button onclick="sensorSalvar(${numSensor}, this)">💾 Salvar</button>
  <br><br>
  <button onclick="sensorToggleEdit(${numSensor}, false)">❌ Cancelar</button>
</div>

<div id="sensorView-${numSensor}">
  <button onclick="sensorToggleEdit(${numSensor}, true)">✏️ Editar</button>
</div>
`;

    card.innerHTML = html;
    container.appendChild(card);
  });
}

async function tomadaSalvar(numRele, btn) {
  btn.innerText = "Salvando...";
  btn.disabled = true;

  document.getElementById(`tomadaCard-${numRele}`).classList.add("saving");

  try {
    await tomadaAPI(
      "setReleConfig",
      {
        rele: numRele,
        nome: document.getElementById(`nome-${numRele}`).value,
        regra: document.getElementById(`regra-${numRele}`).value,
      },
      "PUT",
    );
  } catch (e) {
    statusMsg("Erro ao salvar: " + e);
  }

  // load recria o HTML com o botão habilitado
  load();
}

async function tomadaOverride(numRele, novoEstado, btn) {
  btn.innerText = "Processando...";
  btn.disabled = true;

  try {
    await tomadaAPI(
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
  load();
}

function tomadaToggleEdit(id, editing) {
  editingTomada = editing ? id : null;
  document.getElementById(`tomadaEdit-${id}`).style.display = editing
    ? "block"
    : "none";
  document.getElementById(`tomadaView-${id}`).style.display = !editing
    ? "block"
    : "none";
}

async function openConfig() {
  document.getElementById("configPanel").classList.add("open");
  document.getElementById("configOverlay").classList.add("open");

  try {
    configData = await tomadaAPI("data");
    renderConfig();
  } catch (e) {
    statusMsg("Erro openConfig: " + e);
    throw e;
  }
}

function closeConfig() {
  document.getElementById("configPanel").classList.remove("open");
  document.getElementById("configOverlay").classList.remove("open");
}

function renderConfig() {
  const container = document.getElementById("configContent");

  let html = "";

  configData.reles.forEach((rele, i) => {
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

  configData.sensores.forEach((sensor, i) => {
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
    configData.tipoSensores
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
      configData.reles.map(async (old, i) => {
        const rele = i + 1;
        const pino = parseInt(
          document.getElementById(`cfg-pino-${rele}`).value,
        );
        const ativo = pino != -1;

        if (old.ativo != ativo || old.pino != pino) {
          await tomadaAPI(
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
