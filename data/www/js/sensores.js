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
