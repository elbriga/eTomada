function umidGetCard(power) {
  const card = document.createElement("div");
  card.id = `umid-01`;
  card.className = "card cardUmidificador";
  card.innerHTML = `
<div class="headerTop">
  <div class="minHeight">
    <div class="title">Umidificador</div>
  </div>
  <div class="headerTop">
    <button class="editBtn" onclick="umidificadorSetPower(${power == 1 ? 0 : 1})">${power == 1 ? "🟢" : "🔴"}</button>
    <button class="editBtn" onclick="umidificadorSetPower(${power == 2 ? 0 : 2})">${power == 2 ? "🟢" : "🔴"}</button>
    <button class="editBtn" onclick="umidificadorSetPower(${power == 3 ? 0 : 3})">${power == 3 ? "🟢" : "🔴"}</button>
  </div>
</div>
`;
  return card;
}

function umidificadorRenderFromSnapshot() {
  const container = document.getElementById("umidificador");
  container.innerHTML = "";

  if (eTomadaData.umidPower == undefined) return;

  const card = umidGetCard(eTomadaData.umidPower);
  container.appendChild(card);
}

async function umidificadorSetPower(power) {
  try {
    await eTomadaAPI("setUmidificador", { estado: power }, "PUT");
    eTomadaRender();
  } catch (e) {
    statusMsg(`Erro ao ${ativa ? "ativar" : "desativar"} regra: ` + e);
  }
}
