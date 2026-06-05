let releEditando = null;

function releGetCard(rele) {
  if (!rele.nome) rele.nome = "---";

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

    const card = releGetCard(rele);
    container.appendChild(card);
  });
}

function releGetOptionsSensores() {
  return (
    "<option value=''>Escolha um Sensor</option>" +
    eTomadaData.sensores
      .map((s) => {
        return s.pino == -1
          ? ""
          : `<option value="S${s.num}">${s.nome}</option>`;
      })
      .join("")
  );
}

function releOpenEditModal(numRele) {
  const rele = eTomadaData.reles.find((r) => r.num == numRele);
  if (!rele) return;

  releEditando = numRele;

  let [acao, param1, param2] = rele.regra.split("|");

  if (!param1) param1 = "";
  if (!param2) param2 = "";

  document.getElementById("modalTitle").innerHTML = "Editar Tomada " + numRele;
  document.getElementById("modalNome").value = rele.nome || "";

  document.getElementById("modalDivRegra").style.display = "block";
  document.getElementById("modalRegra").value = rele.regra || "";
  document.getElementById("modalRegraAcao").value = acao;

  document.getElementById("modalHorario").value =
    param1 != "" && param2 != "" ? `${param1}-${param2}` : "";

  let optionsSensores = releGetOptionsSensores();

  let sensorON = param1.substring(0, 2);
  let opON = param1.substring(2, 3);
  let valTesteON = param1.substring(3);
  document.getElementById("modalCondSensorLiga").innerHTML = optionsSensores;
  document.getElementById("modalCondSensorLiga").value = sensorON;
  document.getElementById("modalCondOpLiga").value = opON == ">" ? "+" : "-";
  document.getElementById("modalCondValTesteLiga").value = valTesteON;

  let sensorOF = param2.substring(0, 2);
  let opOF = param2.substring(2, 3);
  let valTesteOF = param2.substring(3);
  document.getElementById("modalCondSensorDesliga").innerHTML = optionsSensores;
  document.getElementById("modalCondSensorDesliga").value = sensorOF;
  document.getElementById("modalCondOpDesliga").value = opOF == ">" ? "+" : "-";
  document.getElementById("modalCondValTesteDesliga").value = valTesteOF;

  document.getElementById("modalSalvarBtn").onclick = function () {
    releSalvarFromModal();
  };

  releOCModalAcao();
  editModalOpen();
}

async function releSalvarFromModal() {
  if (releEditando == null) return;

  const btn = document.getElementById("modalSalvarBtn");

  btn.disabled = true;
  btn.innerText = "Salvando...";

  try {
    let regra = "";
    const acao = document.getElementById("modalRegraAcao").value;
    if (acao == "") {
      regra = "";
    } else if (acao == "SE") {
      const condON =
        document.getElementById("modalCondSensorLiga").value +
        (document.getElementById("modalCondOpLiga").value == "+" ? ">" : "<") +
        document.getElementById("modalCondValTesteLiga").value;
      const condOF =
        document.getElementById("modalCondSensorDesliga").value +
        (document.getElementById("modalCondOpDesliga").value == "+"
          ? ">"
          : "<") +
        document.getElementById("modalCondValTesteDesliga").value;
      regra = `SE|${condON}|${condOF}`;
    } else if (acao == "ON" || acao == "OF") {
      let horario = document.getElementById("modalHorario").value + "";
      regra = `${acao}|${horario.replace("-", "|")}`;
    } else {
      throw Error("Acao invalida!");
    }

    await eTomadaAPI(
      "setReleConfig",
      {
        rele: releEditando,
        nome: document.getElementById("modalNome").value,
        regra: regra,
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
}

function releOCModalAcao() {
  const acao = document.getElementById("modalRegraAcao").value;
  document.getElementById("divRegraHorario").style.display =
    acao == "" || acao == "SE" ? "none" : "block";
  document.getElementById("divRegraCondicional").style.display =
    acao == "" || acao != "SE" ? "none" : "block";
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
