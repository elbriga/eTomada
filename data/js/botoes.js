let botaoEditando = null;

function botaoGetCard(recurso) {
  if (recurso.tipo != "BOTAO") return null;

  let botao = recurso.device;

  const card = document.createElement("div");
  card.id = `recursoCard-${recurso.id}`;
  card.className = "card cardBotao" + (recurso.remoto ? " cardRemoto" : "");
  card.innerHTML = `
<div class="headerTop">
  <div class="minHeight">
    <div class="medio">Botao ${recurso.id}</div>
    <div class="title">${escapeHtml(recurso.nome || "")}</div>
  </div>
  <button class="editBtn" onclick="botaoOpenEditModal('${recurso.id}')">✏️</button>
</div>
<br>
<div class="status ${botao.estado ? "on" : "off"}">
  ${botao.estado ? "● Ligado" : "● Desligado"}
</div>
<button onclick="botaoToggle('${recurso.id}', this)">
  Enviar Toggle
</button>
`;
  return card;
}

function botoesRenderFromRecursos() {
  const container = document.getElementById("botoes");
  container.innerHTML = "";

  eTomadaData.recursos.forEach((recurso, i) => {
    if (recurso.tipo != "BOTAO") return;

    let botao = recurso.device;
    if (!botao.ativo) return;

    const card = botaoGetCard(recurso);
    container.appendChild(card);
  });
}

function botaoOpenEditModal(recursoID) {
  const recurso = eTomadaData.recursos.find((r) => r.id == recursoID);
  if (!recurso) return;

  const botao = recurso.device;
  botaoEditando = recursoID;

  document.getElementById("modalTitle").innerHTML = "Editar Botão " + recursoID;
  document.getElementById("modalNome").value = recurso.nome || "";
  document.getElementById("modalDivRegra").style.display = "none";
  document.getElementById("modalSalvarBtn").onclick = function () {
    botaoSalvarFromModal();
  };

  editModalOpen();
}

async function botaoSalvarFromModal() {
  if (botaoEditando == null) return;

  const btn = document.getElementById("modalSalvarBtn");

  btn.disabled = true;
  btn.innerText = "Salvando...";

  try {
    await eTomadaAPI(
      "setRecursoConfig",
      {
        id: botaoEditando,
        nome: document.getElementById("modalNome").value,
      },
      "PUT",
    );

    editModalClose();
  } catch (e) {
    statusMsg("Erro ao salvar botao: " + e);
  } finally {
    btn.disabled = false;
    btn.innerText = "💾 Salvar";
  }
}
