async function carregarNodos() {
  const response = await fetch("/api/nodes");
  const nodes = await response.json();
  const select = document.getElementById("device");
  const valorAtual = select.value;

  select.innerHTML = '<option value="">Todos</option>';
  for (const node of nodes) {
    const option = document.createElement("option");
    option.value = node.device_id;
    option.textContent = node.device_id;
    select.appendChild(option);
  }

  select.value = valorAtual;
}

async function carregarLogs() {
  const device = document.getElementById("device").value;
  const level = document.getElementById("level").value;
  const module = document.getElementById("module").value;
  const search = document.getElementById("search").value;
  const params = new URLSearchParams();

  if (device) params.set("deviceID", device);
  if (level) params.set("level", level);
  if (module) params.set("module", module);
  if (search) params.set("search", search);

  const response = await fetch("/api/logs?" + params.toString());
  const logs = await response.json();

  const tbody = document.getElementById("logs");
  tbody.innerHTML = "";
  for (const log of logs) {
    const tr = document.createElement("tr");
    const levelClass = "log-" + log.level.toLowerCase().replaceAll("!", "");
    tr.innerHTML = `
            <td class="timestamp">
                ${formataData(escapeHtml(log.timestamp))}
            </td>

            <td class="device">
                ${escapeHtml(log.device_id)}
            </td>

            <td class="level ${levelClass}">
                ${escapeHtml(log.level)}
            </td>

            <td class="module">
                ${escapeHtml(log.module || "")}
            </td>

            <td>
                ${formatUptime(log.uptime)}
            </td>

            <td class="message">
                ${escapeHtml(log.message)}
            </td>
        `;

    tbody.appendChild(tr);
  }

  document.getElementById("status").textContent = `${logs.length} logs`;
}

function formataData(ts) {
  const data = new Date(ts * 1000);
  return data.toLocaleString("pt-BR");
}

function formatUptime(seconds) {
  if (seconds === null || seconds === undefined) return "";

  let s = seconds;
  const days = Math.floor(s / 86400);
  s %= 86400;
  const hours = Math.floor(s / 3600);
  s %= 3600;
  const minutes = Math.floor(s / 60);
  s %= 60;

  if (days > 0) {
    return (
      `${days}d ` +
      `${String(hours).padStart(2, "0")}:` +
      `${String(minutes).padStart(2, "0")}:` +
      `${String(s).padStart(2, "0")}`
    );
  }

  return (
    `${String(hours).padStart(2, "0")}:` +
    `${String(minutes).padStart(2, "0")}:` +
    `${String(s).padStart(2, "0")}`
  );
}

function escapeHtml(value) {
  const div = document.createElement("div");

  div.textContent = value ?? "";

  return div.innerHTML;
}

carregarNodos();
carregarLogs();
