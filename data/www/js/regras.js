let regraEditando = null;

function regraGetCard(regra) {
  const card = document.createElement("div");
  card.id = `regraCard-${regra.id}`;
  card.className = "card cardRegra";
  card.innerHTML = `
<div class="headerTop">
  <div class="minHeight">
    <div class="medio">Regra ${regra.id}</div>
    <div class="title">${regra.nome}</div>
  </div>
  <div>
    <button class="editBtn" onclick="regraDelete('${regra.id}', this)">🗑️</button>
    <button class="editBtn" onclick="regraSetAtiva('${regra.id}', ${regra.ativa ? 0 : 1}, this)">${regra.ativa ? "🟢" : "🔴"}</button>
    <button class="editBtn" onclick="regraOpenEditModal('${regra.id}')">✏️</button>
  </div>
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
    //if (!regra.ativa) return;

    const card = regraGetCard(regra);
    container.appendChild(card);
  });
}

function regraPopulaComboRecursos(comboID, filtro) {
  if (!filtro) filtro = (r) => true;
  var options =
    "<option value=''>Escolha um Recurso</option>\n" +
    eTomadaData.recursos
      .filter(filtro)
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

  regraPopulaCombosHorario();
  regraPopulaComboRecursos(
    "modalRegraRecursoEvento",
    (r) => r.tipo != "SENSOR",
  );
  regraPopulaComboRecursos("modalRegraVar");

  regraEditando = regraID;
  document.getElementById("modalTitle").innerHTML =
    regraID > 0 ? "Editar Regra " + regraID : "Editar Nova Regra";

  document.getElementById("modalNome").value = regra.nome || "";

  document.getElementById("modalRegraCondicao").value = regra.quando.tipo || "";
  if (regra.quando.tipo == "EVENTO") {
    document.getElementById("modalRegraRecursoEvento").value =
      regra.quando.recurso || "";
    document.getElementById("modalRegraEvento").value =
      regra.quando.evento || "";
  } else if (regra.quando.tipo == "HORARIO") {
    document.getElementById("modalRegraHora").value = regra.quando.hora || 0;
    document.getElementById("modalRegraMinuto").value =
      regra.quando.minuto || 0;
  }
  if (regra.quando.check != undefined) {
    document.getElementById("modalRegraVar").value =
      regra.quando.check.variavel || "";
    document.getElementById("modalRegraOperacao").value =
      regra.quando.check.operacao || "";
    document.getElementById("modalRegraVal").value =
      regra.quando.check.valor || "";
  }

  document.getElementById("modalRegraCheck").checked =
    regra.quando.check &&
    regra.quando.check.variavel &&
    regra.quando.check.variavel != "";

  regrasOCModalCondicao();
  regrasOCModalCheck();

  document.getElementById("modalRegraAcao").value = regra.acao.tipo;
  regrasOCModalAcao();

  if (regra.acao.tipo == "ESTADO") {
    regraPopulaComboRecursos(
      "modalRegraAcaoEstadoRecurso",
      (r) => r.tipo == "RELE",
    );
    document.getElementById("modalRegraAcaoEstadoRecurso").value =
      regra.acao.recurso;
    document.getElementById("modalRegraAcaoEstadoComando").value =
      regra.acao.comando;
  } else if (regra.acao.tipo == "TIMER") {
    regraPopulaComboRecursos(
      "modalRegraAcaoTimerRecurso",
      (r) => r.tipo == "RELE",
    );
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

function regrasOCModalCheck() {
  const mostra = document.getElementById("modalRegraCheck").checked;
  document.getElementById("divRegraCheck").style.display = mostra
    ? "block"
    : "none";
}

function regrasOCModalCondicao() {
  const condicao = document.getElementById("modalRegraCondicao").value;

  document.getElementById("divRegraEvento").style.display =
    condicao == "EVENTO" ? "block" : "none";
  document.getElementById("divRegraHorario").style.display =
    condicao == "HORARIO" ? "block" : "none";
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
      check: {},
    },
    acao: {
      tipo: document.getElementById("modalRegraAcao").value,
    },
  };

  if (body.quando.tipo == "") {
    alert("Escolha o tipo da Condição!");
    return;
  }
  if (body.quando.tipo == "EVENTO") {
    body.quando.recurso = document.getElementById(
      "modalRegraRecursoEvento",
    ).value;
    if (body.quando.recurso == "") {
      alert("Escolha o Recurso!");
      return;
    }
    body.quando.evento = document.getElementById("modalRegraEvento").value;
    if (body.quando.evento == "") {
      alert("Escolha o Evento!");
      return;
    }
  } else if (body.quando.tipo == "HORARIO") {
    body.quando.hora = document.getElementById("modalRegraHora").value;
    if (body.quando.hora == "") {
      alert("Escolha a Hora!");
      return;
    }
    body.quando.minuto = document.getElementById("modalRegraMinuto").value;
    if (body.quando.minuto == "") {
      alert("Escolha o Minuto!");
      return;
    }
  }

  if (document.getElementById("modalRegraCheck").checked) {
    body.quando.check.variavel = document.getElementById("modalRegraVar").value;
    if (body.quando.check.variaval == "") {
      alert("Escolha a Variável!");
      return;
    }
    body.quando.check.operacao =
      document.getElementById("modalRegraOperacao").value;
    if (body.quando.check.operacao == "") {
      alert("Escolha a Operação!");
      return;
    }
    body.quando.check.valor = document.getElementById("modalRegraVal").value;
    if (body.quando.check.valor == "") {
      alert("Digite o Valor!");
      return;
    }
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
    eTomadaRender();
  } catch (e) {
    statusMsg("Erro ao salvar regra: " + e);
  } finally {
    btn.disabled = false;
    btn.innerText = "💾 Salvar";
  }
}

async function regraDelete(regraID, btn) {
  if (!confirm("Deseja deletar essa regra?")) return;

  btn.disabled = true;
  btn.innerText = "...";

  try {
    await eTomadaAPI("delRegra", { id: regraID }, "PUT");
    eTomadaRender();
  } catch (e) {
    statusMsg("Erro ao deletar regra: " + e);
  }
}

async function regraSetAtiva(regraID, ativa) {
  try {
    await eTomadaAPI("setRegra", { id: regraID, ativa: ativa }, "PUT");
    eTomadaRender();
  } catch (e) {
    statusMsg(`Erro ao ${ativa ? "ativar" : "desativar"} regra: ` + e);
  }
}
