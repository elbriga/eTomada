let umidEditando = null;

function umidGetCard(recurso) {
  const power = recurso.device.estado;
  const fanPower =
    recurso.device.estadoFan == undefined ? -1 : recurso.device.estadoFan;

  const card = document.createElement("div");
  card.id = `umid-01`;
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
  <button class="editBtn" onclick="umidificadorSetPower('${recurso.id}', ${power == 1 ? 0 : 1})">${power == 1 ? "🟢" : "🔴"}</button>
  <button class="editBtn" onclick="umidificadorSetPower('${recurso.id}', ${power == 2 ? 0 : 2})">${power == 2 ? "🟢" : "🔴"}</button>
  <button class="editBtn" onclick="umidificadorSetPower('${recurso.id}', ${power == 3 ? 0 : 3})">${power == 3 ? "🟢" : "🔴"}</button>
</div>
${
  fanPower >= 0
    ? `<br><div class="minHeight">
  <div class="title">Ventilador</div>
</div>
<div class="status">
  <button class="editBtn" onclick="umidificadorFanSetPower('${recurso.id}', ${fanPower == 1 ? 0 : 1})">${fanPower == 1 ? "🟢" : "🔴"}</button>
  <button class="editBtn" onclick="umidificadorFanSetPower('${recurso.id}', ${fanPower == 2 ? 0 : 2})">${fanPower == 2 ? "🟢" : "🔴"}</button>
  <button class="editBtn" onclick="umidificadorFanSetPower('${recurso.id}', ${fanPower == 3 ? 0 : 3})">${fanPower == 3 ? "🟢" : "🔴"}</button>
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

async function umidificadorSetPower(recursoID, power) {
  try {
    await eTomadaAPI(
      "setRecurso",
      {
        id: recursoID,
        estado: power,
      },
      "PUT",
    );
  } catch (e) {
    statusMsg(`Erro ao controlar UMID: ` + e);
  }
}

async function umidificadorFanSetPower(recursoID, power) {
  try {
    await eTomadaAPI(
      "setRecurso",
      {
        id: recursoID,
        estadoFan: power,
      },
      "PUT",
    );
  } catch (e) {
    statusMsg(`Erro ao controlar FAN: ` + e);
  }
}
