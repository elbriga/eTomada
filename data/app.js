const API_BASE =
  window.location.host === "localhost"
    ? "http://192.168.18.105" // IP do ESP quando o frontend esta hospedado para DEV
    : window.location.origin;

function getRegraTXT(regra) {
  return regra;
}

async function tomadaAPI(endpoint, body = undefined, method = "GET") {
  let httpConfig = {
    method,
    headers: {
      "Content-type": "application/json; charset=UTF-8",
    },
  };
  if (body != undefined) {
    httpConfig["body"] = JSON.stringify(body);
  }

  try {
    const res = await fetch(`${API_BASE}/api/${endpoint}`, httpConfig);
    return await res.json();
  } catch (e) {
    alert("Erro API");
    return { message: "erro" };
  }
}

async function load() {
  try {
    const data = await tomadaAPI("data");

    if (!data.reles) {
      // TODO msg de erro!
      return;
    }

    document.getElementById("datahora").innerHTML = data.datahorastr;

    const container = document.getElementById("reles");
    container.innerHTML = "";

    data.reles.forEach((rele, i) => {
      if (!rele.nome) rele.nome = "---";

      let numRele = i + 1;

      const card = document.createElement("div");
      card.id = "tomadaCard-" + numRele;
      card.className = "card";

      let html = `
<div class="title">Tomada ${numRele}: ${escapeHtml(rele.nome || "")}</div>
<div class="small">Regra: ${getRegraTXT(rele.regra)}</div>
<div class="small">pino: ${rele.pino}</div>
<br>`;
      if (rele.ativo) {
        html += `
<div class="status ${rele.estado ? "on" : "off"}">${rele.estado ? "● Ligado" : "● Desligado"}</div>
<div id="tomadaEdit-${numRele}" style="display: none">
  Nome: <input id="nome-${numRele}" value="${escapeHtml(rele.nome || "")}"><br>
  Regra: <input id="regra-${numRele}" value="${escapeHtml(rele.regra || "")}" placeholder="ON=08:00-18:00">
  <button onclick="tomadaSalvar(${numRele}, this)">💾 Salvar</button>
  <br><br>
  <button onclick="tomadaToggleEdit(${numRele}, false)">❌ Cancelar</button>
</div>
<div id="tomadaView-${numRele}">
  <button onclick="tomadaOverride(${numRele}, ${rele.estado ? "false" : "true"})">
    ${rele.estado ? "🔴 Desligar" : "🟢 Ligar"}${rele.regra == "" ? "" : " por 30 minutos"}
  </button>
  <br><br>
  <button onclick="tomadaToggleEdit(${numRele}, true)">✏️ Editar</button>
</div>
`;
      } else {
        html += `
<div class="status off">● Desativado</div>`;
      }

      card.innerHTML = html;
      container.appendChild(card);
    });
  } catch (e) {
    console.error("Erro ao carregar:", e);
  }
}

async function tomadaSalvar(numRele, btn) {
  btn.innerText = "Salvando...";
  btn.disabled = true;

  document.getElementById(`tomadaCard-${numRele}`).classList.add("saving");

  await tomadaAPI(
    "setReleConfig",
    {
      rele: numRele,
      nome: document.getElementById(`nome-${numRele}`).value,
      regra: document.getElementById(`regra-${numRele}`).value,
    },
    "PUT",
  );

  btn.disabled = false;
  btn.innerText = "💾 Salvar";

  load();
}

async function tomadaOverride(numRele, novoEstado) {
  await tomadaAPI(
    "setRele",
    {
      rele: numRele,
      estado: novoEstado ? "1" : "0",
    },
    "PUT",
  );

  load();
}

function tomadaToggleEdit(id, editing) {
  document.getElementById(`tomadaEdit-${id}`).style.display = editing
    ? "block"
    : "none";
  document.getElementById(`tomadaView-${id}`).style.display = !editing
    ? "block"
    : "none";
}

function escapeHtml(str) {
  return String(str)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

load();
