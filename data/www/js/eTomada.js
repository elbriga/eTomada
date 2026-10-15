/* ===== eTomada.js ===== */
const API_BASE =
  window.location.host == "localhost" ||
  window.location.host == "pi" ||
  window.location.host == "pi.casa"
    ? "http://10.0.2.1" // IP do ESP quando o frontend esta hospedado para DEV
    : window.location.origin;

let eTomadaData = null;

function eTomadaInit() {
  sseInit();
  // Nao precisa Render aqui pois vem o evento sse_snapshot ao conectar
  // eTomadaRender();
}

async function eTomadaRefresh(snapshot) {
  if (!snapshot) {
    snapshot = await eTomadaAPI("getSnapshot");
  }

  if (!snapshot.recursos) {
    statusMsg("Erro nos dados, sem recursos!");
    return;
  }

  eTomadaData = snapshot;
}

let _eTomadaLoading = false;
async function eTomadaRender(snapshot) {
  statusMsg("");

  if (_eTomadaLoading) return;
  _eTomadaLoading = true;

  {
    await eTomadaRefresh(snapshot);

    document.title = eTomadaData.device_id
      ? `${eTomadaData.device_id} - eTomada`
      : "eTomada";

    document.getElementById("datahora").innerHTML =
      "uptime: " +
      formataTempo(eTomadaData.uptime) +
      " - " +
      eTomadaData.datahorastr;

    sensoresRenderFromRecursos();
    relesRenderFromRecursos();
    botoesRenderFromRecursos();
    umidificadorRenderFromRecursos();

    regrasRenderFromSnapshot();
  }

  _eTomadaLoading = false;
}

function eTomadaBoot() {
  eTomadaAPI("boot");
}

;

/* ===== api.js ===== */
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
    eTomadaRender();
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

  // Refresh de um recurso
  SSE.addEventListener("sse_recurso", (e) => {
    const recurso = JSON.parse(e.data);
    recursoAtualiza(recurso);
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

;

/* ===== recurso.js ===== */
function recursoAtualiza(recurso) {
  const idx = eTomadaData.recursos.findIndex((r) => r.id === recurso.id);

  if (idx === -1) {
    // Novo
    eTomadaData.recursos.push(recurso);
  } else {
    eTomadaData.recursos[idx] = recurso;
  }

  let newCard;
  switch (recurso.tipo) {
    case "RELE":
      newCard = releGetCard(recurso);
      break;

    case "SENSOR":
      newCard = sensorGetCard(recurso);
      break;

    case "BOTAO":
      newCard = botaoGetCard(recurso);
      break;

    case "UMIDIFICADOR":
      newCard = umidGetCard(recurso);
      break;

    default:
      break;
  }

  const oldCard = document.getElementById(`recursoCard-${recurso.id}`);
  oldCard.parentNode.replaceChild(newCard, oldCard);

  if (recurso.tipo == "SENSOR") {
    // Ao mudar o nome do sensor pode precisar atualizar regras SE dos reles
    relesRenderFromRecursos();
  }
}

function recursoGet(id) {
  return eTomadaData.recursos.find((r) => r.id === id) || null;
}

;

/* ===== reles.js ===== */
let releEditando = null;

function releGetCard(recurso) {
  if (recurso.tipo != "RELE") return null;

  let rele = recurso.device;

  const card = document.createElement("div");
  card.id = `recursoCard-${recurso.id}`;
  card.className = "card cardRele" + (recurso.remoto ? " cardRemoto" : "");
  card.innerHTML = `
<div class="headerTop">
  <div class="minHeight">
    <div class="medio">Tomada ${recurso.id}${recurso.remoto ? ` em ${recurso.nodo}` : ""}</div>
    <div class="title">${escapeHtml(recurso.nome || "")}</div>
  </div>
  <button class="editBtn" onclick="releOpenEditModal('${recurso.id}')">✏️</button>
</div>
<div class="status ${rele.estado ? "on" : "off"}">
  ${rele.estado ? "● Ligado" : "● Desligado"}
</div>

<button onclick="releControla('${recurso.id}', ${rele.estado ? "false" : "true"}, this)">
  ${rele.estado ? "🔴 Desligar" : "🟢 Ligar"}
</button>
`;
  return card;
}

function relesRenderFromRecursos() {
  const container = document.getElementById("reles");
  container.innerHTML = "";

  eTomadaData.recursos.forEach((recurso, i) => {
    if (recurso.tipo != "RELE") return;

    const card = releGetCard(recurso);
    container.appendChild(card);
  });
}

function releGetOptionsSensores() {
  return (
    "<option value=''>Escolha um Sensor</option>" +
    eTomadaData.recursos
      .filter((r) => r.tipo === "SENSOR" && r.device.pino != -1)
      .map((r) => `<option value="${r.id}">${r.device.nome}</option>`)
      .join("")
  );
}

function releOpenEditModal(recursoID) {
  const recurso = eTomadaData.recursos.find((r) => r.id == recursoID);
  if (!recurso) return;

  const rele = recurso.device;
  releEditando = recursoID;

  document.getElementById("modalTitle").innerHTML =
    "Editar Tomada " + recursoID;
  document.getElementById("modalNome").value = recurso.nome || "";

  document.getElementById("modalSalvarBtn").onclick = function () {
    releSalvarFromModal();
  };

  editModalOpen(false);
}

async function releSalvarFromModal() {
  if (releEditando == null) return;

  const btn = document.getElementById("modalSalvarBtn");

  btn.disabled = true;
  btn.innerText = "Salvando...";

  try {
    await eTomadaAPI(
      "setRecursoConfig",
      {
        id: releEditando,
        nome: document.getElementById("modalNome").value,
      },
      "PUT",
    );

    editModalClose();
  } catch (e) {
    statusMsg("Erro ao salvar rele: " + e);
  } finally {
    btn.disabled = false;
    btn.innerText = "💾 Salvar";
  }
}

async function releControla(recursoID, novoEstado, btn) {
  btn.innerText = "Processando...";
  btn.disabled = true;

  try {
    await eTomadaAPI(
      "setRecurso",
      {
        id: recursoID,
        estado: novoEstado ? "ON" : "OFF",
      },
      "PUT",
    );
  } catch (e) {
    statusMsg("Erro ao setar: " + e);
  }
}

;

/* ===== sensores.js ===== */
let sensorEditando = null;

function sensorGetCard(recurso) {
  if (recurso.tipo != "SENSOR") return null;

  let sensor = recurso.device;
  const tsOK = sensor.status == "OK";
  const card = document.createElement("div");
  const nomeSensor =
    recurso.id == "HORASSECO"
      ? "de Chuva"
      : `${recurso.id} de ${sensor.categoria} ${sensor.tipo}`;
  card.id = `recursoCard-${recurso.id}`;
  card.className =
    `card cardSensor${!tsOK ? " cardSensorInativo" : ""}` +
    (recurso.remoto ? " cardRemoto" : "");
  card.innerHTML = `
<div class="headerTop">
  <div>
    <div class="medio">
      Sensor ${nomeSensor}
      ${recurso.remoto ? ` em ${recurso.nodo}` : ""}
    </div>
    <div class="title">${escapeHtml(recurso.nome || "")}</div>
  </div>
  <button class="editBtn" onclick="sensorOpenEditModal('${recurso.id}')">✏️</button>
</div>
<br>
${
  recurso.id == "HORASSECO"
    ? `<div class="status ${!sensor.valor ? "on" : "off"}">
        ${!sensor.valor ? "MOLHADO" : `horas sem chuva: ${sensor.valor}`}
      </div>`
    : `<div class="status on">
        ${!tsOK ? sensor.status : `${sensor.valor} ${sensor.unidade}`}
      </div>`
}
`;
  return card;
}

function sensoresRenderFromRecursos() {
  const container = document.getElementById("sensores");
  container.innerHTML = "";

  eTomadaData.recursos.forEach((recurso, i) => {
    if (recurso.tipo != "SENSOR") return;

    let sensor = recurso.device;
    if (sensor.pino == -1) return;

    const card = sensorGetCard(recurso);
    container.appendChild(card);
  });
}

function sensorOpenEditModal(recursoID) {
  const recurso = eTomadaData.recursos.find((r) => r.id == recursoID);
  if (!recurso) return;

  const sensor = recurso.device;
  sensorEditando = recursoID;

  document.getElementById("modalTitle").innerHTML =
    "Editar Sensor " + recursoID;
  document.getElementById("modalNome").value = recurso.nome || "";
  document.getElementById("modalSalvarBtn").onclick = function () {
    sensorSalvarFromModal();
  };

  editModalOpen(false);
}

async function sensorSalvarFromModal() {
  if (sensorEditando == null) return;

  const btn = document.getElementById("modalSalvarBtn");

  btn.disabled = true;
  btn.innerText = "Salvando...";

  try {
    await eTomadaAPI(
      "setRecursoConfig",
      {
        id: sensorEditando,
        nome: document.getElementById("modalNome").value,
      },
      "PUT",
    );

    editModalClose();
  } catch (e) {
    statusMsg("Erro ao salvar sensor: " + e);
  } finally {
    btn.disabled = false;
    btn.innerText = "💾 Salvar";
  }
}

;

/* ===== botoes.js ===== */
let botaoEditando = null;

function botaoGetCard(recurso) {
  if (recurso.tipo != "BOTAO") return null;

  let botao = recurso.device;

  const card = document.createElement("div");
  card.id = `recursoCard-${recurso.id}`;
  card.className = "card cardBotao" + (recurso.remoto ? " cardRemoto" : "");
  card.innerHTML = `
<div class="headerTop">
  <div class="minHeight">
    <div class="medio">Botao ${recurso.id}${recurso.remoto ? ` em ${recurso.nodo}` : ""}</div>
    <div class="title">${escapeHtml(recurso.nome || "")}</div>
  </div>
  <button class="editBtn" onclick="botaoOpenEditModal('${recurso.id}')">✏️</button>
</div>
<br>
<div class="status ${botao.estado ? "on" : "off"}">
  ${botao.estado ? "● Ligado" : "● Desligado"}
</div>
<button onclick="botaoMock('TOGGLE', '${recurso.id}', this)">
  Enviar Toggle
</button>
<br>
<button onclick="botaoMock('CLICK', '${recurso.id}', this)">
  Enviar Click
</button>
`;
  return card;
}

function botoesRenderFromRecursos() {
  const container = document.getElementById("botoes");
  container.innerHTML = "";

  eTomadaData.recursos.forEach((recurso, i) => {
    if (recurso.tipo != "BOTAO") return;

    const card = botaoGetCard(recurso);
    container.appendChild(card);
  });
}

function botaoOpenEditModal(recursoID) {
  const recurso = eTomadaData.recursos.find((r) => r.id == recursoID);
  if (!recurso) return;

  const botao = recurso.device;
  botaoEditando = recursoID;

  document.getElementById("modalTitle").innerHTML = "Editar Botão " + recursoID;
  document.getElementById("modalNome").value = recurso.nome || "";
  document.getElementById("modalSalvarBtn").onclick = function () {
    botaoSalvarFromModal();
  };

  editModalOpen(false);
}

async function botaoSalvarFromModal() {
  if (botaoEditando == null) return;

  const btn = document.getElementById("modalSalvarBtn");

  btn.disabled = true;
  btn.innerText = "Salvando...";

  try {
    await eTomadaAPI(
      "setRecursoConfig",
      {
        id: botaoEditando,
        nome: document.getElementById("modalNome").value,
      },
      "PUT",
    );

    editModalClose();
  } catch (e) {
    statusMsg("Erro ao salvar botao: " + e);
  } finally {
    btn.disabled = false;
    btn.innerText = "💾 Salvar";
  }
}

async function botaoMock(acao, recursoID, btn) {
  //btn.innerText = "Processando...";
  //btn.disabled = true;

  try {
    await eTomadaAPI(
      "mock",
      {
        recursoID: recursoID,
        acao: acao,
      },
      "POST",
    );
  } catch (e) {
    statusMsg("Erro no mock! : " + e);
  }
}

;

/* ===== config.js ===== */
async function openConfig() {
  try {
    await eTomadaRefresh();
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
  if (eTomadaData.nodosRemotos) {
    html = "<h2>Nodos Remotos</h2>";
    eTomadaData.nodosRemotos.forEach((nodo) => {
      html += `<button onclick="nodoInfo('${nodo.id}')">${nodo.online ? "🟢" : "🔴"} ${nodo.id} (${nodo.tipo})</button><br>\n`;
    });
  }

  if (eTomadaData.novosNodos) {
    html += "<h2>Novos eTomada!</h2>";
    eTomadaData.novosNodos.forEach((nodo) => {
      html += `<button onclick="nodoInfo('${nodo.id}')">🚨 ${nodo.id} (${nodo.tipo})</button><br>\n`;
    });
  }

  container.innerHTML = html;
}

async function salvarConfigGeral() {
  statusMsg("TODO :: Configuração salva");

  closeConfig();

  eTomadaRender();
}

/*async function factoryReset() {
  if (confirm("Deseja realmente ZERAR TUDO!?!")) {
    await eTomadaAPI("factoryReset", { senha: 1333 }, "POST");

    closeConfig();

    statusMsg("Recarregado com configuração de fábrica");
  }
}*/

async function soReset() {
  if (confirm("Confirma?")) {
    await eTomadaAPI("reboot");

    closeConfig();

    statusMsg("Reset!");
  }
}

function closeCard(id) {
  const card = document.getElementById(id);
  if (card) {
    card.parentNode.removeChild(card);
  }
}

function tenhoRecurso(nodo, id) {
  if (eTomadaData.recursos == undefined) return false;

  let rec = eTomadaData.recursos.find(
    (r) => r.remoto && r.nodo == nodo && r.idRemoto == id,
  );

  return !!rec;
}

async function nodoInfo(id) {
  const cardID = `nodoCard-${id}`;
  const nodoInfoID = `nodoInfo-${id}`;
  const oldCard = document.getElementById(cardID);

  let nodo = eTomadaData.nodosRemotos.find((nr) => nr.id === id);
  if (!nodo) {
    if (eTomadaData.novosNodos) {
      // Add!
      nodo = eTomadaData.novosNodos.find((nr) => nr.id === id);
      nodo.novo = true;
      nodo.descricao = nodo.id;
    } else {
      return;
    }
  }

  const msgOnline = nodo.online ? `🟢 ${nodo.ip}` : "🔴 offline";

  const card = document.createElement("div");
  card.id = cardID;
  card.className = "card cardRemoto";
  card.innerHTML = `
<div class="headerTop">
  <div class="minHeight">
    <div class="medio">Nodo ${id} ${msgOnline}</div>
    <div class="title">${nodo.descricao}</div>
  </div>
  <div>
    <div id="btnDelNodo-${id}"></div>
    <button class="editBtn" onclick="closeCard('${card.id}')">✖</button>
  </div>
</div>
<div id="${nodoInfoID}"></div>
`;

  const painel = document.getElementById("painel");
  oldCard != undefined
    ? painel.replaceChild(card, oldCard)
    : painel.appendChild(card);

  var htmlInfo = `
descrição: <input type="text" id="descNodo-${id}" value="${nodo.descricao}" /><br>
Modelo: ${nodo.tipo}<input type="hidden" id="tipoNodo-${id}" value="${nodo.tipo}" /><br>
`;

  if (nodo.novo) {
    htmlInfo += `<button onclick="nodoAdd('${id}')">🟢 Adicionar</button>`;
  } else {
    const recursos = eTomadaData.recursos.filter(
      (r) => r.remoto && r.nodo == id,
    );

    let htmlRecursos = "Recursos:<br><ul>";
    recursos.forEach((r) => {
      htmlRecursos += `<li>${r.tipo} ${r.id} (${r.idRemoto}) <button class="editBtn" onclick="recursoRemotoDel('${id}','${r.idRemoto}')">Del</button></li>`;
    });

    if (nodo.online) {
      const nodoSnapshot = await eTomadaAPI("getNodo?id=" + id);
      if (nodoSnapshot.msg != undefined && nodoSnapshot.msg == "OK") {
        const snapshot = nodoSnapshot.nodo;
        htmlInfo += `WiFi: ${snapshot.ssid} (${snapshot.wifiPower} db)<br>`;

        if (snapshot.recursos != undefined && snapshot.recursos.length > 0) {
          snapshot.recursos.forEach((r) => {
            if (tenhoRecurso(id, r.id)) return;
            htmlRecursos += `<li>${r.tipo} (${r.id}) <button class="editBtn" onclick="recursoRemotoAdd('${id}','${r.id}')">Add</button></li>`;
          });
        }
      }
    }
    htmlRecursos += "</ul>";

    htmlInfo += htmlRecursos;

    if (recursos.length == 0)
      document.getElementById(`btnDelNodo-${id}`).innerHTML =
        `<button class="editBtn" onclick="nodoDel('${id}')">🔴 del</button>`;
  }

  document.getElementById(nodoInfoID).innerHTML = htmlInfo;
}

async function nodoAdd(id) {
  const desc = document.getElementById(`descNodo-${id}`).value;

  try {
    let msg = await eTomadaAPI("addNodo", { id: id, desc: desc }, "PUT");
    if (msg.msg != undefined && msg.msg != "OK") statusMsg(msg.msg);
    else closeCard("nodoCard-" + id);
  } catch (e) {
    statusMsg("Erro ao adicionar nodo: " + e);
  }
}

async function nodoDel(id) {
  try {
    let msg = await eTomadaAPI("delNodo", { id: id }, "PUT");
    if (msg.msg != undefined && msg.msg != "OK") statusMsg(msg.msg);
    else closeCard("nodoCard-" + id);
  } catch (e) {
    statusMsg("Erro ao remover nodo: " + e);
  }
}

async function recursoRemotoAdd(nodo, idRemoto) {
  try {
    let msg = await eTomadaAPI(
      "addRecursoRemoto",
      { nodo: nodo, idRemoto: idRemoto },
      "PUT",
    );
    if (msg.msg != undefined) statusMsg(msg.msg);
    await eTomadaRender();
    nodoInfo(nodo); // Refresh
  } catch (e) {
    statusMsg("Erro ao adicionar recurso: " + e);
  }
}

async function recursoRemotoDel(nodo, idRemoto) {
  try {
    let msg = await eTomadaAPI(
      "delRecursoRemoto",
      { nodo: nodo, idRemoto: idRemoto },
      "PUT",
    );
    if (msg.msg != undefined) statusMsg(msg.msg);
    await eTomadaRender();
    nodoInfo(nodo); // Refresh
  } catch (e) {
    statusMsg("Erro ao remover recurso: " + e);
  }
}

;

/* ===== util.js ===== */
function statusMsg(msg) {
  document.getElementById("status").innerHTML = msg;
}

function editModalOpen(comRegras) {
  document.getElementById("modalOverlay").classList.add("open");
  document.getElementById("editModal").classList.add("open");

  document.getElementById("modalDivRegra").style.display = comRegras
    ? "block"
    : "none";
}

function editModalClose() {
  releEditando = null;

  document.getElementById("modalOverlay").classList.remove("open");
  document.getElementById("editModal").classList.remove("open");
}

function escapeHtml(str) {
  return String(str)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
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

;

/* ===== regras.js ===== */
let regraEditando = null;

function regraGetCard(regra) {
  const card = document.createElement("div");
  card.id = `regraCard-${regra.id}`;
  card.className = "card cardRegra";
  card.innerHTML = `
<div class="headerTop">
  <div class="minHeight">
    <div class="medio">Regra ${regra.id}</div>
    <div class="title">${regra.nome}</div>
  </div>
  <div>
    <button class="editBtn" onclick="regraDelete('${regra.id}', this)">🗑️</button>
    <button class="editBtn" onclick="regraSetAtiva('${regra.id}', ${regra.ativa ? 0 : 1}, this)">${regra.ativa ? "🟢" : "🔴"}</button>
    <button class="editBtn" onclick="regraOpenEditModal('${regra.id}')">✏️</button>
  </div>
</div>
<div class="status">
  ${regra.descricao}
</div>
`;
  return card;
}

function regrasRenderFromSnapshot() {
  const container = document.getElementById("regras");
  container.innerHTML = "";

  eTomadaData.regras.forEach((regra, i) => {
    //if (!regra.ativa) return;

    const card = regraGetCard(regra);
    container.appendChild(card);
  });
}

function regraPopulaComboRecursos(comboID, filtro) {
  if (!filtro) filtro = (r) => true;
  var options =
    "<option value=''>Escolha um Recurso</option>\n" +
    eTomadaData.recursos
      .filter(filtro)
      .map((r) => `<option value="${r.id}">${r.tipo} - ${r.nome}</option>`)
      .join("\n");

  document.getElementById(comboID).innerHTML = options;
}

function regraPopulaCombosHorario() {
  var options = "<option value=''>Escolha uma Hora</option>\n";
  for (let h = 0; h < 24; h++)
    options += `<option value='${h}'>${String(h).padStart(2, "0")}</option>\n`;
  document.getElementById("modalRegraHora").innerHTML = options;

  options = "<option value=''>Escolha um Minuto</option>\n";
  for (let m = 0; m < 60; m++)
    options += `<option value='${m}'>${String(m).padStart(2, "0")}</option>\n`;
  document.getElementById("modalRegraMinuto").innerHTML = options;
}

function regraOpenEditModal(regraID) {
  let regra = {};
  if (regraID == 0) {
    regra = {
      id: 0,
      nome: "Nova Regra",
      quando: {
        tipo: "EVENTO",
        recurso: "",
      },
      acao: {
        tipo: "ESTADO",
        recurso: "",
      },
    };
  } else {
    regra = eTomadaData.regras.find((r) => r.id == regraID);
    if (!regra) {
      statusMsg("Regra não encontrada!");
      return;
    }
  }

  regraPopulaCombosHorario();
  regraPopulaComboRecursos(
    "modalRegraRecursoEvento",
    (r) => r.tipo != "SENSOR",
  );
  regraPopulaComboRecursos("modalRegraVar");

  regraEditando = regraID;
  document.getElementById("modalTitle").innerHTML =
    regraID > 0 ? "Editar Regra " + regraID : "Editar Nova Regra";

  document.getElementById("modalNome").value = regra.nome || "";

  document.getElementById("modalRegraCondicao").value = regra.quando.tipo || "";
  if (regra.quando.tipo == "EVENTO") {
    document.getElementById("modalRegraRecursoEvento").value =
      regra.quando.recurso || "";
    document.getElementById("modalRegraEvento").value =
      regra.quando.evento || "";
  } else if (regra.quando.tipo == "HORARIO") {
    document.getElementById("modalRegraHora").value = regra.quando.hora || 0;
    document.getElementById("modalRegraMinuto").value =
      regra.quando.minuto || 0;
  }
  if (regra.quando.check != undefined) {
    document.getElementById("modalRegraVar").value =
      regra.quando.check.variavel || "";
    document.getElementById("modalRegraOperacao").value =
      regra.quando.check.operacao || "";
    document.getElementById("modalRegraVal").value =
      regra.quando.check.valor || "";
  }

  document.getElementById("modalRegraCheck").checked =
    regra.quando.check &&
    regra.quando.check.variavel &&
    regra.quando.check.variavel != "";

  regrasOCModalCondicao();
  regrasOCModalCheck();

  document.getElementById("modalRegraAcao").value = regra.acao.tipo;
  regrasOCModalAcao();

  // Acao ESTADO
  regraPopulaComboRecursos(
    "modalRegraAcaoEstadoRecurso",
    (r) => r.tipo == "RELE",
  );
  document.getElementById("modalRegraAcaoEstadoRecurso").value =
    regra.acao.recurso;
  document.getElementById("modalRegraAcaoEstadoComando").value =
    regra.acao.comando;

  // ACAO TIMER
  regraPopulaComboRecursos(
    "modalRegraAcaoTimerRecurso",
    (r) => r.tipo == "RELE",
  );
  document.getElementById("modalRegraAcaoTimerRecurso").value =
    regra.acao.recurso;
  document.getElementById("modalRegraAcaoTimerTempo").value =
    regra.acao.timer | 10;

  document.getElementById("modalSalvarBtn").onclick = function () {
    regraSalvarFromModal();
  };

  editModalOpen(true);
}

function regrasOCModalCheck() {
  const mostra = document.getElementById("modalRegraCheck").checked;
  document.getElementById("divRegraCheck").style.display = mostra
    ? "block"
    : "none";
}

function regrasOCModalCondicao() {
  const condicao = document.getElementById("modalRegraCondicao").value;

  document.getElementById("divRegraEvento").style.display =
    condicao == "EVENTO" ? "block" : "none";
  document.getElementById("divRegraHorario").style.display =
    condicao == "HORARIO" ? "block" : "none";
}

function regrasOCModalAcao() {
  const acao = document.getElementById("modalRegraAcao").value;

  document.getElementById("divRegraAcaoEstado").style.display =
    acao == "ESTADO" ? "block" : "none";
  document.getElementById("divRegraAcaoTimer").style.display =
    acao == "TIMER" ? "block" : "none";
}

async function regraSalvarFromModal() {
  if (regraEditando == null) return;

  const btn = document.getElementById("modalSalvarBtn");

  btn.disabled = true;
  btn.innerText = "Salvando...";

  let body = {
    id: regraEditando,
    nome: document.getElementById("modalNome").value,
    quando: {
      tipo: document.getElementById("modalRegraCondicao").value,
      check: {},
    },
    acao: {
      tipo: document.getElementById("modalRegraAcao").value,
    },
  };

  if (body.quando.tipo == "") {
    alert("Escolha o tipo da Condição!");
    return;
  }
  if (body.quando.tipo == "EVENTO") {
    body.quando.recurso = document.getElementById(
      "modalRegraRecursoEvento",
    ).value;
    if (body.quando.recurso == "") {
      alert("Escolha o Recurso!");
      return;
    }
    body.quando.evento = document.getElementById("modalRegraEvento").value;
    if (body.quando.evento == "") {
      alert("Escolha o Evento!");
      return;
    }
  } else if (body.quando.tipo == "HORARIO") {
    body.quando.hora = document.getElementById("modalRegraHora").value;
    if (body.quando.hora == "") {
      alert("Escolha a Hora!");
      return;
    }
    body.quando.minuto = document.getElementById("modalRegraMinuto").value;
    if (body.quando.minuto == "") {
      alert("Escolha o Minuto!");
      return;
    }
  }

  if (document.getElementById("modalRegraCheck").checked) {
    body.quando.check.variavel = document.getElementById("modalRegraVar").value;
    if (body.quando.check.variaval == "") {
      alert("Escolha a Variável!");
      return;
    }
    body.quando.check.operacao =
      document.getElementById("modalRegraOperacao").value;
    if (body.quando.check.operacao == "") {
      alert("Escolha a Operação!");
      return;
    }
    body.quando.check.valor = document.getElementById("modalRegraVal").value;
    if (body.quando.check.valor == "") {
      alert("Digite o Valor!");
      return;
    }
  }

  if (body.acao.tipo == "ESTADO") {
    body.acao.recurso = document.getElementById(
      "modalRegraAcaoEstadoRecurso",
    ).value;
    body.acao.comando = document.getElementById(
      "modalRegraAcaoEstadoComando",
    ).value;
  } else if (body.acao.tipo == "TIMER") {
    body.acao.recurso = document.getElementById(
      "modalRegraAcaoTimerRecurso",
    ).value;
    body.acao.timer = document.getElementById("modalRegraAcaoTimerTempo").value;
  }

  try {
    await eTomadaAPI("setRegra", body, "PUT");
    editModalClose();
    eTomadaRender();
  } catch (e) {
    statusMsg("Erro ao salvar regra: " + e);
  } finally {
    btn.disabled = false;
    btn.innerText = "💾 Salvar";
  }
}

async function regraDelete(regraID, btn) {
  if (!confirm("Deseja deletar essa regra?")) return;

  btn.disabled = true;
  btn.innerText = "...";

  try {
    await eTomadaAPI("delRegra", { id: regraID }, "PUT");
    eTomadaRender();
  } catch (e) {
    statusMsg("Erro ao deletar regra: " + e);
  }
}

async function regraSetAtiva(regraID, ativa) {
  try {
    await eTomadaAPI("setRegra", { id: regraID, ativa: ativa }, "PUT");
    eTomadaRender();
  } catch (e) {
    statusMsg(`Erro ao ${ativa ? "ativar" : "desativar"} regra: ` + e);
  }
}

;

/* ===== umid.js ===== */
let umidEditando = null;

function umidGetCard(recurso) {
  const power = recurso.device.estado;
  const fanPower =
    recurso.device.estadoFan == undefined ? -1 : recurso.device.estadoFan;

  const card = document.createElement("div");
  card.id = `recursoCard-${recurso.id}`;
  card.className = "card cardUmidificador";
  card.innerHTML = `
<div class="headerTop">
  <div class="minHeight">
    <div class="medio">Umid. ${recurso.id}${recurso.remoto ? ` em ${recurso.nodo}` : ""}</div>
    <div class="title">${escapeHtml(recurso.nome || "")}</div>
  </div>
  <button class="editBtn" onclick="releOpenEditModal('${recurso.id}')">✏️</button>
</div>
<div class="status">
  <button class="editBtn" onclick="umidificadorSetPower('${recurso.id}', ${power == 1 ? 0 : 1}, ${fanPower})">${power == 1 ? "🟢" : "🔴"}</button>
  <button class="editBtn" onclick="umidificadorSetPower('${recurso.id}', ${power == 2 ? 0 : 2}, ${fanPower})">${power == 2 ? "🟢" : "🔴"}</button>
  <button class="editBtn" onclick="umidificadorSetPower('${recurso.id}', ${power == 3 ? 0 : 3}, ${fanPower})">${power == 3 ? "🟢" : "🔴"}</button>
</div>
${
  fanPower >= 0
    ? `<br><div class="minHeight">
  <div class="title">Ventilador</div>
</div>
<div class="status">
  <button class="editBtn" onclick="umidificadorSetPower('${recurso.id}', ${power}, ${fanPower == 1 ? 0 : 1})">${fanPower == 1 ? "🟢" : "🔴"}</button>
  <button class="editBtn" onclick="umidificadorSetPower('${recurso.id}', ${power}, ${fanPower == 2 ? 0 : 2})">${fanPower == 2 ? "🟢" : "🔴"}</button>
  <button class="editBtn" onclick="umidificadorSetPower('${recurso.id}', ${power}, ${fanPower == 3 ? 0 : 3})">${fanPower == 3 ? "🟢" : "🔴"}</button>
</div>`
    : ""
}`;
  return card;
}

function umidificadorRenderFromRecursos() {
  const container = document.getElementById("umidificador");
  container.innerHTML = "";

  eTomadaData.recursos.forEach((recurso, i) => {
    if (recurso.tipo != "UMIDIFICADOR") return;

    const card = umidGetCard(recurso);
    container.appendChild(card);
  });
}

function umidOpenEditModal(recursoID) {
  const recurso = eTomadaData.recursos.find((r) => r.id == recursoID);
  if (!recurso) return;

  const umid = recurso.device;
  umidEditando = recursoID;

  document.getElementById("modalTitle").innerHTML =
    "Editar Umidificador " + recursoID;
  document.getElementById("modalNome").value = recurso.nome || "";

  document.getElementById("modalSalvarBtn").onclick = function () {
    umidSalvarFromModal();
  };

  editModalOpen(false);
}

async function umidSalvarFromModal() {
  if (umidEditando == null) return;

  const btn = document.getElementById("modalSalvarBtn");

  btn.disabled = true;
  btn.innerText = "Salvando...";

  try {
    await eTomadaAPI(
      "setRecursoConfig",
      {
        id: umidEditando,
        nome: document.getElementById("modalNome").value,
      },
      "PUT",
    );

    editModalClose();
  } catch (e) {
    statusMsg("Erro ao salvar umid: " + e);
  } finally {
    btn.disabled = false;
    btn.innerText = "💾 Salvar";
  }
}

async function umidificadorSetPower(recursoID, power, powerFan) {
  try {
    await eTomadaAPI(
      "setRecurso",
      {
        id: recursoID,
        estado: power,
        estadoFan: powerFan,
      },
      "PUT",
    );
  } catch (e) {
    statusMsg(`Erro ao controlar UMID: ` + e);
  }
}

;

