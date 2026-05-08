const API_BASE =
  window.location.protocol === "file:"
    ? "http://192.168.18.105" // TODO : remover
    : window.location.origin;

async function load() {
  try {
    const res = await fetch(`${API_BASE}/api/data`);
    const data = await res.json();

    document.getElementById("datahora").innerHTML = data.datahorastr;

    const container = document.getElementById("reles");
    container.innerHTML = "";

    data.reles.forEach((rele, i) => {
      if (!rele.nome) rele.nome = "---";

      const card = document.createElement("div");
      card.className = "card";

      card.innerHTML = `
<div class="title">Tomada ${i + 1}: ${rele.nome}</div>
<div class="small">(pino ${rele.pino})</div><br>
<div class="status ${rele.estado ? "on" : "off"}">
    ${rele.estado ? "● Ligado" : "● Desligado"}
</div>
<input id="regra-${i}" value="${rele.regra || ""}" placeholder="ON=08:00-18:00">
<button onclick="salvar(${i}, this)">Salvar</button>
`;

      container.appendChild(card);
    });
  } catch (e) {
    console.error("Erro ao carregar:", e);
  }
}

async function salvar(i, btn) {
  const input = document.getElementById(`regra-${i}`);
  const regra = input.value;

  btn.innerText = "Salvando...";
  btn.disabled = true;

  try {
    await fetch(`${API_BASE}/api/setReleConfig`, {
      method: "PUT",
      body: JSON.stringify({
        rele: i + 1,
        regra: regra,
      }),
      headers: {
        "Content-type": "application/json; charset=UTF-8",
      },
    });
  } catch (e) {
    alert("Erro ao salvar");
  }

  btn.innerText = "Salvar";
  btn.disabled = false;

  load();
}

load();
//setInterval(load, 5000);
