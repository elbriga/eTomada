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
      html += `<button onclick="nodoInfo('${nodo.id}')">${nodo.ip == "0.0.0.0" ? "🔴" : "🟢"} ${nodo.id} (${nodo.tipo})</button><br>\n`;
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

async function factoryReset() {
  if (confirm("Deseja realmente ZERAR TUDO!?!")) {
    await eTomadaAPI("factoryReset", { senha: 1333 }, "POST");

    closeConfig();

    statusMsg("Recarregado com configuração de fábrica");
  }
}

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

async function nodoInfo(id) {
  const cardID = `nodoCard-${id}`;
  const nodoInfoID = `nodoInfo-${id}`;
  if (document.getElementById(cardID)) return;

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

  const online = nodo.ip != "0.0.0.0";
  const msgOnline = online ? `🟢 ${nodo.ip}` : "🔴 offline";

  const card = document.createElement("div");
  card.id = cardID;
  card.className = "card cardRemoto";
  card.innerHTML = `
<div class="headerTop">
  <div class="minHeight">
    <div class="medio">Nodo ${id} ${msgOnline}</div>
    <div class="title">${nodo.descricao}</div>
  </div>
  <button class="editBtn" onclick="closeCard('${card.id}')">✖</button>
</div>
<div id="${nodoInfoID}"></div>
`;

  const painel = document.getElementById("painel");
  painel.appendChild(card);

  var htmlInfo = `
descrição: <input type="text" id="descNodo-${id}" value="${nodo.descricao}" /><br>
Modelo: ${nodo.tipo}<input type="hidden" id="tipoNodo-${id}" value="${nodo.tipo}" /><br>
`;

  if (nodo.novo) {
    htmlInfo += `<button onclick="nodoAdd('${id}')">🟢 Adicionar</button>`;
  } else {
    if (!online) return;

    const nodoSnapshot = await eTomadaAPI("getNodo?id=" + id);
    if (!nodoSnapshot.msg || nodoSnapshot.msg != "OK") return;

    const snapshot = nodoSnapshot.nodo;
    if (snapshot.recursos == undefined) snapshot.recursos = [];

    htmlInfo += `WiFi: ${snapshot.ssid} (${snapshot.wifiPower} db)<br>
Recursos:<br>
<ul>
`;
    snapshot.recursos.forEach((r) => {
      htmlInfo += `<li>${r.tipo} ${r.id}</li>`;
    });
    htmlInfo += "</ul>";
  }

  document.getElementById(nodoInfoID).innerHTML = htmlInfo;
}

async function nodoAdd(id) {
  const desc = document.getElementById(`descNodo-${id}`).value;

  try {
    await eTomadaAPI("addNodo", { id: id, desc: desc }, "PUT");
    closeCard(id);
  } catch (e) {
    statusMsg("Erro ao adicionar nodo: " + e);
  }
}
