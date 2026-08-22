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

function regraPopulaComboRecursos(comboID) {
  var options =
    "<option value=''>Escolha um Recurso</option>\n" +
    eTomadaData.recursos
      .filter((r) => r.tipo != "SENSOR")
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
  const regra = eTomadaData.regras.find((r) => r.id == regraID);
  if (!regra) {
    // TODO :: msg
    return;
  }

  regraEditando = regraID;

  document.getElementById("modalTitle").innerHTML = "Editar Regra " + regraID;

  document.getElementById("modalNome").value = regra.nome || "";

  document.getElementById("modalRegraCondicao").value = regra.quando.tipo;
  regrasOCModalCondicao();

  if (regra.quando.tipo == "EVENTO") {
    regraPopulaComboRecursos("modalRegraRecursoEvento");
    document.getElementById("modalRegraRecursoEvento").value =
      regra.quando.recurso;
    document.getElementById("modalRegraEvento").value = regra.quando.evento;
  } else if (regra.quando.tipo == "HORARIO") {
    regraPopulaCombosHorario();
    document.getElementById("modalRegraHora").value = regra.quando.hora;
    document.getElementById("modalRegraMinuto").value = regra.quando.minuto;
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

async function regraSalvarFromModal() {
  if (regraEditando == null) return;

  const btn = document.getElementById("modalSalvarBtn");

  btn.disabled = true;
  btn.innerText = "Salvando...";

  try {
    await eTomadaAPI(
      "setRegra",
      {
        id: regraEditando,
        nome: document.getElementById("modalNome").value,
        // TODO
      },
      "PUT",
    );

    editModalClose();
  } catch (e) {
    statusMsg("Erro ao salvar regra: " + e);
  } finally {
    btn.disabled = false;
    btn.innerText = "💾 Salvar";
  }
}
