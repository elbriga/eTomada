function sensorGetCard(sensor) {
  const card = document.createElement("div");
  card.id = "sensorCard-" + sensor.num;
  card.className = "card cardSensor";
  card.innerHTML = `
<div class="medio">Sensor ${sensor.num}</div>
<div class="title">${escapeHtml(sensor.nome || "")}</div>
<div class="small">pino: ${escapeHtml(sensor.pino)}</div>
<br>
<div class="status on">${sensor.valorStr}</div>
<button onclick="sensorToggleEdit(${sensor.num}, true)">✏️ Editar</button>
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
