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

function regraPopulaCombosHorario(idx) {
  var options = "<option value=''>Escolha uma Hora</option>\n";
  for (let h = 0; h < 24; h++)
    options += `<option value='${h}'>${String(h).padStart(2, "0")}</option>\n`;
  document.getElementById(`modalRegraHora${idx}`).innerHTML = options;

  options = "<option value=''>Escolha um Minuto</option>\n";
  for (let m = 0; m < 60; m++)
    options += `<option value='${m}'>${String(m).padStart(2, "0")}</option>\n`;
  document.getElementById(`modalRegraMinuto${idx}`).innerHTML = options;
}

function regraAddFormCondicao(idx, condicao) {
  let divCondicoes = document.getElementById("modalRegraCondicoes");

  let novaDiv = document.createElement("div");
  novaDiv.innerHTML = `${idx > 1 ? "<br><b>E</b><br><br>" : ""}
          <span class="emLinha">
            Tipo:
            <select id="modalRegraCondicao${idx}" onchange="regrasOCModalCondicao(${idx})">
              <option value="">Escolha um Tipo</option>
              <option value="EVENTO">Eventos</option>
              <option value="HORARIO">Horário</option>
              <option value="EXPRESSAO">Expressão</option>
            </select>
          </span>
          <div id="divRegraEvento${idx}">
            <span class="emLinha">
              Recurso:
              <select id="modalRegraRecursoEvento${idx}"></select>
            </span>
            <span class="emLinha">
              Evento:
              <select id="modalRegraEvento${idx}">
                <option value="">Escolha um Evento</option>
                <option value="ON">Ligou</option>
                <option value="OFF">Desligou</option>
                <option value="TOGGLE">Mudou</option>
                <option value="CLICK">Click!</option>
                <option value="DUPCLICK">Duplo Click!</option>
              </select>
            </span>
          </div>
          <div id="divRegraHorario${idx}">
            <span class="emLinha">
              Horário:
              <select id="modalRegraHora${idx}"></select>
              :
              <select id="modalRegraMinuto${idx}"></select>
            </span>
          </div>
          <div id="divRegraExpressao${idx}">
            <span class="emLinha">
              Expressão:
              <select id="modalRegraVar${idx}"></select>
              <select id="modalRegraOperacao${idx}" style="width: 60px">
                <option value="=">=</option>
                <option value="!=">!=</option>
                <option value="&gt;">&gt;</option>
                <option value="&gt;=">&gt;=</option>
                <option value="&lt;">&lt;</option>
                <option value="&lt;=">&lt;=</option>
              </select>
              <input id="modalRegraVal${idx}" maxlength="5" style="width: 60px" />
            </span>
          </div>`;
  divCondicoes.appendChild(novaDiv);

  regraPopulaCombosHorario(idx);
  regraPopulaComboRecursos(
    `modalRegraRecursoEvento${idx}`,
    (r) => r.tipo != "SENSOR",
  );
  regraPopulaComboRecursos(`modalRegraVar${idx}`);

  document.getElementById(`modalRegraCondicao${idx}`).value = condicao.tipo;
  regrasOCModalCondicao(idx);

  if (condicao.tipo == "EVENTO") {
    document.getElementById(`modalRegraRecursoEvento${idx}`).value =
      condicao.recurso;
    document.getElementById(`modalRegraEvento${idx}`).value = condicao.evento;
  } else if (condicao.tipo == "HORARIO") {
    document.getElementById(`modalRegraHora${idx}`).value = condicao.hora | 0;
    document.getElementById(`modalRegraMinuto${idx}`).value =
      condicao.minuto | 0;
  } else if (condicao.tipo == "EXPRESSAO") {
    document.getElementById(`modalRegraVar${idx}`).value = condicao.variavel;
    document.getElementById(`modalRegraOperacao${idx}`).value =
      condicao.operacao;
    document.getElementById(`modalRegraVal${idx}`).value = condicao.valor;
  }
}

function regraOpenEditModal(regraID) {
  let regra = {};
  if (regraID == 0) {
    regra = {
      id: 0,
      nome: "Nova Regra",
      quando: [
        {
          tipo: "EVENTO",
          recurso: "",
        },
      ],
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

  // Criar a interface das condicoes
  document.getElementById("modalRegraCondicoes").innerHTML = "";
  var idxCondicao = 0;
  regra.quando.forEach((condicao) => {
    idxCondicao++;
    regraAddFormCondicao(idxCondicao, condicao);
  });

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

function regrasOCModalCondicao(idx) {
  const condicao = document.getElementById(`modalRegraCondicao${idx}`).value;

  document.getElementById(`divRegraEvento${idx}`).style.display =
    condicao == "EVENTO" ? "block" : "none";
  document.getElementById(`divRegraHorario${idx}`).style.display =
    condicao == "HORARIO" ? "block" : "none";
  document.getElementById(`divRegraExpressao${idx}`).style.display =
    condicao == "EXPRESSAO" ? "block" : "none";
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

  const REGRAS_MAX_CONDICOES = 3; // Acompanha regras.h do backend

  const btn = document.getElementById("modalSalvarBtn");

  btn.disabled = true;
  btn.innerText = "Salvando...";

  let body = {
    id: regraEditando,
    nome: document.getElementById("modalNome").value,
    quando: [],
    acao: {
      tipo: document.getElementById("modalRegraAcao").value,
    },
  };

  for (var idx = 1; idx <= REGRAS_MAX_CONDICOES; idx++) {
    var tipo = document.getElementById(`modalRegraCondicao${idx}`);
    if (!tipo) break;

    tipo = tipo.value;
    if (tipo == "") {
      alert("Escolha o tipo da Condição!");
      return;
    }

    var condicao = { tipo: tipo };
    if (tipo == "EVENTO") {
      condicao.recurso = document.getElementById(
        `modalRegraRecursoEvento${idx}`,
      ).value;
      if (condicao.recurso == "") {
        alert("Escolha o Recurso!");
        return;
      }
      condicao.evento = document.getElementById(`modalRegraEvento${idx}`).value;
      if (condicao.evento == "") {
        alert("Escolha o Evento!");
        return;
      }
    } else if (tipo == "HORARIO") {
      condicao.hora = document.getElementById(`modalRegraHora${idx}`).value;
      if (condicao.hora == "") {
        alert("Escolha a Hora!");
        return;
      }
      condicao.minuto = document.getElementById(`modalRegraMinuto${idx}`).value;
      if (condicao.minuto == "") {
        alert("Escolha o Minuto!");
        return;
      }
    } else if (tipo == "EXPRESSAO") {
      condicao.variavel = document.getElementById(`modalRegraVar${idx}`).value;
      if (condicao.variaval == "") {
        alert("Escolha a Variável!");
        return;
      }
      condicao.operacao = document.getElementById(
        `modalRegraOperacao${idx}`,
      ).value;
      if (condicao.operacao == "") {
        alert("Escolha a Operação!");
        return;
      }
      condicao.valor = document.getElementById(`modalRegraVal${idx}`).value;
      if (condicao.valor == "") {
        alert("Escolha o Valor!");
        return;
      }
    }

    body.quando.push(condicao);
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
