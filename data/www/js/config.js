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

  let html = "<h2>Nodos Remotos</h2>";
  eTomadaData.nodosRemotos.forEach((nodo) => {
    html += `> @ ${nodo.ip} > ${nodo.id} (${nodo.tipo})<br>\n`;
  });

  if (eTomadaData.novosNodos) {
    html += "<h2>Novos eTomada!</h2>";
    eTomadaData.novosNodos.forEach((nodo) => {
      html += `> @ ${nodo.ip} > ${nodo.id} (${nodo.tipo})<br>\n`;
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
  // TODO Modal confirm
  await eTomadaAPI("factoryReset", { senha: 1333 }, "POST");

  closeConfig();

  statusMsg("Recarregado com configuração de fábrica");
}

async function soReset() {
  // TODO Modal confirm
  await eTomadaAPI("reboot");

  closeConfig();

  statusMsg("Reset!");
}
