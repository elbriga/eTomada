let sensorEditando = null;

function sensorGetCard(sensor) {
  const card = document.createElement("div");
  card.id = "sensorCard-" + sensor.num;
  card.className = "card cardSensor";
  card.innerHTML = `
<div class="headerTop">
  <div>
    <div class="medio">Sensor ${sensor.num} - ${sensor.unidade}</div>
    <div class="title">${escapeHtml(sensor.nome || "")}</div>
    <div class="small">pino: ${escapeHtml(sensor.pino)}</div>
  </div>
  <button class="editBtn" onclick="sensorOpenEditModal(${sensor.num})">✏️</button>
</div>
<br>
<div class="status on">${sensor.valorStr}</div>
`;
  return card;
}

function sensoresRender(sensores) {
  const container = document.getElementById("sensores");
  container.innerHTML = "";

  sensores.forEach((sensor, i) => {
    if (!sensor.tipo) return;
    if (!sensor.nome) sensor.nome = "---";

    const card = sensorGetCard(sensor);
    container.appendChild(card);
  });
}

function sensorOpenEditModal(numSensor) {
  const sensor = eTomadaData.sensores.find((r) => r.num == numSensor);
  if (!sensor) return;

  sensorEditando = numSensor;

  document.getElementById("modalTitle").innerHTML =
    "Editar Sensor " + numSensor;
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
      "setSensorConfig",
      {
        sensor: sensorEditando,
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

function sensorAtualiza(sensor) {
  eTomadaData.sensores[sensor.num - 1] = sensor;

  const newCard = sensorGetCard(sensor);
  const oldCard = document.getElementById(`sensorCard-${sensor.num}`);
  oldCard.parentNode.replaceChild(newCard, oldCard);

  // Ao mudar o nome do sensor pode precisar atualizar regras SE dos reles
  relesRender(eTomadaData.reles);
}
