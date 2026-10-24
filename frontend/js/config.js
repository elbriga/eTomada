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
  container.innerHTML = html;
}

async function salvarConfigGeral() {
  statusMsg("TODO :: Configuração salva");

  closeConfig();

  eTomadaRender();
}

/*async function factoryReset() {
  if (confirm("Deseja realmente ZERAR TUDO!?!")) {
    await eTomadaAPI("factoryReset", { senha: 1333 }, "POST");

    closeConfig();

    statusMsg("Recarregado com configuração de fábrica");
  }
}*/

async function soReset() {
  if (confirm("Confirma?")) {
    await eTomadaAPI("reboot");

    closeConfig();

    statusMsg("Reset!");
  }
}

function closeCard(id) {
  const card = document.getElementById(id);
  if (card) {
    card.parentNode.removeChild(card);
  }
}
