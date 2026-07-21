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

  eTomadaData.sensores.forEach((sensor, i) => {
    const num = i + 1;

    html += `
<div class="configCard cardSensor">
  <div class="headerTop">
    <div><b>Sensor ${num}</b></div>
    <span>
      <select id="cfg-sensor-${num}">
        ${getTipoSensorOptions(sensor.tipo)}
      </select>
    </span>
  </div>
</div>
`;
  });

  container.innerHTML = html;
}

function getTipoSensorOptions(selected) {
  return (
    `<option value="">Desativado!</option>\n` +
    eTomadaData.tipoSensores
      .map((ts) => {
        return `
<option value="${ts.nome}" ${ts.nome == selected ? "selected" : ""}>
  ${ts.tipo} - ${ts.nome}
</option>
`;
      })
      .join("")
  );
}

async function salvarConfigGeral() {
  try {
    await Promise.all(
      eTomadaData.sensores.map(async (old, i) => {
        const sensor = i + 1;
        const tipo = document.getElementById(`cfg-sensor-${sensor}`).value;

        if (old.tipo != tipo) {
          await eTomadaAPI(
            "setSensorConfig",
            {
              sensor,
              tipo,
            },
            "PUT",
          );
        }
      }),
    );

    statusMsg("Configuração salva");

    closeConfig();

    eTomadaRender();
  } catch (e) {
    statusMsg("Erro salvarConfigGeral: " + e);
  }
}

async function factoryReset() {
  // TODO Modal confirm
  await eTomadaAPI("factoryReset", { senha: 1333 }, "POST");

  closeConfig();

  statusMsg("Recarregado com configuração de fábrica");
}
