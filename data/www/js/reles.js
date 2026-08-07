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
    <div class="medio">Tomada ${recurso.id}</div>
    <div class="title">${escapeHtml(recurso.nome || "")}</div>
  </div>
  <button class="editBtn" onclick="releOpenEditModal('${recurso.id}')">✏️</button>
</div>
<div class="status ${rele.estado ? "on" : "off"}">
  ${rele.estado ? "● Ligado" : "● Desligado"}
  ${rele.override > Date.now() / 1000 ? ` (até ${getHoraFromTS(rele.override)})` : ""}
</div>

<button onclick="releOverride('${recurso.id}', ${rele.estado ? "false" : "true"}, this)">
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

    let rele = recurso.device;
    if (!rele.ativo) return;

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

  editModalOpen();
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

async function releOverride(recursoID, novoEstado, btn) {
  btn.innerText = "Processando...";
  btn.disabled = true;

  try {
    await eTomadaAPI(
      "setRecurso",
      {
        id: recursoID,
        estado: novoEstado ? "1" : "0",
      },
      "PUT",
    );
  } catch (e) {
    statusMsg("Erro ao setar: " + e);
  }
}
