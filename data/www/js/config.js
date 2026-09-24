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

function tenhoRecurso(nodo, id) {
  if (eTomadaData.recursos == undefined) return false;

  let rec = eTomadaData.recursos.find(
    (r) => r.nodo == nodo && r.idRemoto == id,
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
    const recursos = eTomadaData.recursos.filter((r) => r.nodo == id);

    let htmlRecursos = "Recursos:<br><ul>";
    recursos.forEach((r) => {
      htmlRecursos += `<li>${r.tipo} ${r.id} (${r.idRemoto}) <button class="editBtn" onclick="recursoRemotoDel('${id}','${r.idRemoto}')">Del</button></li>`;
    });

    if (online) {
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
