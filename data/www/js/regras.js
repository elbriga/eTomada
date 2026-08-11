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
  <button class="editBtn" onclick="releOpenEditModal('${regra.id}')">✏️</button>
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

function regraOpenEditModal(regraID) {
  editModalOpen();
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
