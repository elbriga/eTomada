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

  eTomadaData.reles.forEach((rele, i) => {
    const num = i + 1;

    html += `
<div class="configCard cardRele">
  <div class="headerTop">
    <div><b>Tomada ${num}</b></div>
    <span>
      <select id="cfg-pino-${num}">
        ${getPinosOptions(rele.pino)}
      </select>
    </span>
  </div>
</div>
`;
  });

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

function getPinosOptions(selected) {
  const pinos = [13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33];

  return (
    `<option value="-1">Desativada!</option>\n` +
    pinos
      .map((p) => {
        return `
<option value="${p}" ${p == selected ? "selected" : ""}>
  GPIO ${p}
</option>
`;
      })
      .join("")
  );
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
      eTomadaData.reles.map(async (old, i) => {
        const rele = i + 1;
        const pino = parseInt(
          document.getElementById(`cfg-pino-${rele}`).value,
        );
        const ativo = pino != -1;

        if (old.ativo != ativo || old.pino != pino) {
          await eTomadaAPI(
            "setReleConfig",
            {
              rele,
              ativo,
              pino,
            },
            "PUT",
          );
        }
      }),
    );

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

function factoryReset() {
  // TODO Modal confirm
  statusMsg("Reiniciando");
  closeConfig();
}
