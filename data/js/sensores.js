let sensorEditando = null;

function sensorGetCard(recurso) {
  if (recurso.tipo != "SENSOR") return null;

  let sensor = recurso.device;
  let tipoSensor = eTomadaData.tipoSensores.find(
    (ts) => ts.nome == sensor.tipo,
  );
  if (!tipoSensor) {
    tipoSensor = { status: "TipoSensor Invalido" };
  }
  const tsOK = tipoSensor.status == "OK";
  const valor = !tsOK ? tipoSensor.status : `${sensor.valor} ${sensor.unidade}`;
  const card = document.createElement("div");
  card.id = `recursoCard-${recurso.id}`;
  card.className =
    `card cardSensor${!tsOK ? " cardSensorInativo" : ""}` +
    (recurso.remoto ? " cardRemoto" : "");
  card.innerHTML = `
<div class="headerTop">
  <div>
    <div class="medio">Sensor ${sensor.num} - ${tipoSensor.nome} - ${tipoSensor.tipo}</div>
    <div class="title">${escapeHtml(sensor.nome || "")}</div>
    <div class="small">pino: ${escapeHtml(sensor.pino)}</div>
  </div>
  <button class="editBtn" onclick="sensorOpenEditModal('${recurso.id}')">✏️</button>
</div>
<br>
<div class="status on">${valor}</div>
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
    if (!sensor.nome) sensor.nome = "---";

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
  document.getElementById("modalNome").value = sensor.nome || "";
  document.getElementById("modalDivRegra").style.display = "none";
  document.getElementById("modalSalvarBtn").onclick = function () {
    sensorSalvarFromModal();
  };

  editModalOpen();
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
