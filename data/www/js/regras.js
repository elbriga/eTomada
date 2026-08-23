let regraEditando = null;

function regraGetCard(regra) {
  const card = document.createElement("div");
  card.id = `regraCard-${regra.id}`;
  card.className = "card cardRegra";
  card.innerHTML = `
<div class="headerTop">
  <div class="minHeight">
    <div class="medio">Regra ${regra.id}</div>
    <div class="title">NOME</div>
  </div>
  <button class="editBtn" onclick="regraOpenEditModal('${regra.id}')">✏️</button>
</div>
<div class="status">
  ${regra.descricao}
</div>
`;
  return card;
}

function regrasRenderFromSnapshot() {
  const container = document.getElementById("regras");
  container.innerHTML = "";

  eTomadaData.regras.forEach((regra, i) => {
    //if (!regra.ativo) return;

    const card = regraGetCard(regra);
    container.appendChild(card);
  });
}

function regraPopulaComboRecursos(comboID, somenteReles) {
  var options =
    "<option value=''>Escolha um Recurso</option>\n" +
    eTomadaData.recursos
      .filter((r) => r.tipo != "SENSOR" && (!somenteReles || r.tipo == "RELE"))
      .map((r) => `<option value="${r.id}">${r.tipo} - ${r.nome}</option>`)
      .join("\n");

  document.getElementById(comboID).innerHTML = options;
}

function regraPopulaCombosHorario() {
  var options = "<option value=''>Escolha uma Hora</option>\n";
  for (let h = 0; h < 24; h++)
    options += `<option value='${h}'>${String(h).padStart(2, "0")}</option>\n`;
  document.getElementById("modalRegraHora").innerHTML = options;

  options = "<option value=''>Escolha um Minuto</option>\n";
  for (let m = 0; m < 60; m++)
    options += `<option value='${m}'>${String(m).padStart(2, "0")}</option>\n`;
  document.getElementById("modalRegraMinuto").innerHTML = options;
}

function regraOpenEditModal(regraID) {
  let regra = {};
  if (regraID == 0) {
    regra = {
      id: 0,
      nome: "Nova Regra",
      quando: {
        tipo: "EVENTO",
        recurso: "",
      },
      acao: {
        tipo: "ESTADO",
        recurso: "",
      },
    };
  } else {
    regra = eTomadaData.regras.find((r) => r.id == regraID);
    if (!regra) {
      // TODO :: msg
      return;
    }
  }

  regraEditando = regraID;

  document.getElementById("modalTitle").innerHTML =
    regraID > 0 ? "Editar Regra " + regraID : "Editar Nova Regra";

  document.getElementById("modalNome").value = regra.nome || "";

  document.getElementById("modalRegraCondicao").value = regra.quando.tipo;
  regrasOCModalCondicao();

  if (regra.quando.tipo == "EVENTO") {
    regraPopulaComboRecursos("modalRegraRecursoEvento", false);
    document.getElementById("modalRegraRecursoEvento").value =
      regra.quando.recurso;
    document.getElementById("modalRegraEvento").value = regra.quando.evento;
  } else if (regra.quando.tipo == "HORARIO") {
    regraPopulaCombosHorario();
    document.getElementById("modalRegraHora").value = regra.quando.hora | 0;
    document.getElementById("modalRegraMinuto").value = regra.quando.minuto | 0;
  }

  document.getElementById("modalRegraAcao").value = regra.acao.tipo;
  regrasOCModalAcao();

  if (regra.acao.tipo == "ESTADO") {
    regraPopulaComboRecursos("modalRegraAcaoEstadoRecurso", true);
    document.getElementById("modalRegraAcaoEstadoRecurso").value =
      regra.acao.recurso;
    document.getElementById("modalRegraAcaoEstadoComando").value =
      regra.acao.comando;
  } else if (regra.acao.tipo == "TIMER") {
    regraPopulaComboRecursos("modalRegraAcaoTimerRecurso", true);
    document.getElementById("modalRegraAcaoTimerRecurso").value =
      regra.acao.recurso;
    document.getElementById("modalRegraAcaoTimerTempo").value =
      regra.acao.timer | 10;
  }

  document.getElementById("modalSalvarBtn").onclick = function () {
    regraSalvarFromModal();
  };

  editModalOpen(true);
}

function regrasOCModalCondicao() {
  const condicao = document.getElementById("modalRegraCondicao").value;

  document.getElementById("divRegraEvento").style.display =
    condicao == "EVENTO" ? "block" : "none";
  document.getElementById("divRegraHorario").style.display =
    condicao == "HORARIO" ? "block" : "none";
  document.getElementById("divRegraSensor").style.display =
    condicao == "SENSOR" ? "block" : "none";
}

function regrasOCModalAcao() {
  const acao = document.getElementById("modalRegraAcao").value;

  document.getElementById("divRegraAcaoEstado").style.display =
    acao == "ESTADO" ? "block" : "none";
  document.getElementById("divRegraAcaoTimer").style.display =
    acao == "TIMER" ? "block" : "none";
}

async function regraSalvarFromModal() {
  if (regraEditando == null) return;

  const btn = document.getElementById("modalSalvarBtn");

  btn.disabled = true;
  btn.innerText = "Salvando...";

  let body = {
    id: regraEditando,
    nome: document.getElementById("modalNome").value,
    quando: {
      tipo: document.getElementById("modalRegraCondicao").value,
    },
    acao: {
      tipo: document.getElementById("modalRegraAcao").value,
    },
  };

  if (body.quando.tipo == "EVENTO") {
    body.quando.recurso = document.getElementById(
      "modalRegraRecursoEvento",
    ).value;
    body.quando.evento = document.getElementById("modalRegraEvento").value;
  } else if (body.quando.tipo == "HORARIO") {
    body.quando.hora = document.getElementById("modalRegraHora").value;
    body.quando.minuto = document.getElementById("modalRegraMinuto").value;
  } else if (body.quando.tipo == "SENSOR") {
    // TODO
  }

  if (body.acao.tipo == "ESTADO") {
    body.acao.recurso = document.getElementById(
      "modalRegraAcaoEstadoRecurso",
    ).value;
    body.acao.comando = document.getElementById(
      "modalRegraAcaoEstadoComando",
    ).value;
  } else if (body.acao.tipo == "TIMER") {
    body.acao.recurso = document.getElementById(
      "modalRegraAcaoTimerRecurso",
    ).value;
    body.acao.timer = document.getElementById("modalRegraAcaoTimerTempo").value;
  }

  try {
    await eTomadaAPI("setRegra", body, "PUT");

    editModalClose();
  } catch (e) {
    statusMsg("Erro ao salvar regra: " + e);
  } finally {
    btn.disabled = false;
    btn.innerText = "💾 Salvar";
  }
}
