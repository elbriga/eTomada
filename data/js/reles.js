let releEditando = null;

function releGetCard(rele) {
  const card = document.createElement("div");
  card.id = "tomadaCard-" + rele.num;
  card.className = "card cardRele";
  card.innerHTML = `
<div class="headerTop">
  <div class="minHeight">
    <div class="medio">Tomada ${rele.num}</div>
    <div class="title">${escapeHtml(rele.nome || "")}</div>
    <div class="medio">${releGetRegraTXT(rele.regra)}</div>
    <div class="small">pino: ${escapeHtml(rele.pino)}</div>
  </div>
  <button class="editBtn" onclick="releOpenEditModal(${rele.num})">✏️</button>
</div>
<br>
<div class="status ${rele.estado ? "on" : "off"}">
  ${rele.estado ? "● Ligado" : "● Desligado"}
  ${rele.override > Date.now() / 1000 && rele.regra != "" ? ` (até ${getHoraFromTS(rele.override)})` : ""}
</div>

<button onclick="releOverride(${rele.num}, ${rele.estado ? "false" : "true"}, this)">
  ${rele.estado ? "🔴 Desligar" : "🟢 Ligar"}${rele.regra == "" ? "" : " por 30 minutos"}
</button>
`;
  return card;
}

function relesRender(reles) {
  const container = document.getElementById("reles");
  container.innerHTML = "";

  reles.forEach((rele, i) => {
    if (!rele.ativo) return;
    if (!rele.nome) rele.nome = "---";

    const card = releGetCard(rele);
    container.appendChild(card);
  });
}

function releOpenEditModal(numRele) {
  const rele = eTomadaData.reles.find((r) => r.num == numRele);
  if (!rele) return;

  releEditando = numRele;

  document.getElementById("modalTitle").innerHTML = "Editar Tomada " + numRele;
  document.getElementById("modalNome").value = rele.nome || "";
  document.getElementById("modalDivRegra").style.display = "block";
  document.getElementById("modalRegra").value = rele.regra || "";
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
      "setReleConfig",
      {
        rele: releEditando,
        nome: document.getElementById("modalNome").value,
        regra: document.getElementById("modalRegra").value,
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

async function releOverride(numRele, novoEstado, btn) {
  btn.innerText = "Processando...";
  btn.disabled = true;

  try {
    await eTomadaAPI(
      "setRele",
      {
        rele: numRele,
        estado: novoEstado ? "1" : "0",
      },
      "PUT",
    );
  } catch (e) {
    statusMsg("Erro ao setar: " + e);
  }

  // load recria o HTML com o botão habilitado
  //load();
}

function releGetRegraTXT(regra) {
  if (!regra || regra.trim() === "") {
    return "Modo Manual";
  }

  regra = regra.trim();

  let [acao, param1, param2] = regra.split("|");

  if (acao === "ON" || acao === "OF") {
    return (
      (acao === "ON" ? "Ligado" : "Desligado") +
      " das " +
      param1 +
      " às " +
      param2
    );
  }

  if (acao === "SE") {
    if (param1 != "") {
      let numSensor = parseInt(param1[1] + "") - 1;
      let sensor = eTomadaData.sensores[numSensor];
      if (sensor) {
        let op = param1[2];
        let val = param1.substring(3);
        param1 = `${sensor.nome} ${op} ${val}`;
      }
    }
    if (param2 != "") {
      let numSensor = parseInt(param2[1] + "") - 1;
      let sensor = eTomadaData.sensores[numSensor];
      if (sensor) {
        let op = param2[2];
        let val = param2.substring(3);
        param2 = `${sensor.nome} ${op} ${val}`;
      }
    }
    return (
      (param1 !== "" ? `Ligar SE ${param1}` : "") +
      (param1 !== "" && param2 != "" ? "<br>\n" : "") +
      (param2 !== "" ? `Desligar SE ${param2}` : "")
    );
  }

  return "??" + regra;
}

function releAtualiza(rele) {
  eTomadaData.reles[rele.num - 1] = rele;

  const newCard = releGetCard(rele);
  const oldCard = document.getElementById(`tomadaCard-${rele.num}`);
  oldCard.parentNode.replaceChild(newCard, oldCard);
}
