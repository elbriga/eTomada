#!/usr/bin/env node

const nodos = [
  "mestre",
  "grow",
  "umid-grow",
  "terrario",
  "umid-terrario",
  "quarto",
  "sw3jantar",
  "sw6jantar",
  "dev",
  "cozy",
  "mini01",
  "mini03",
];

const TIMEOUT = 3000;

function formataUptime(s) {
  if (s == null) return "-";

  const segundos = s / 1000;
  const dias = Math.floor(segundos / 86400);
  const horas = Math.floor((segundos % 86400) / 3600);
  const minutos = Math.floor((segundos % 3600) / 60);

  return `${dias}d ${horas}h ${minutos}m`;
}

async function getSnapshot(nodo) {
  const inicio = performance.now();

  try {
    const response = await fetch(`http://${nodo}/api/getSnapshot`, {
      signal: AbortSignal.timeout(TIMEOUT),
    });

    if (!response.ok) throw new Error(`HTTP ${response.status}`);

    const json = await response.json();

    const ms = Math.round(performance.now() - inicio);
    // console.log(`${nodo} respondeu em [${ms} ms]!`);

    return {
      NODO: nodo,
      API: json.api ?? "-",
      IP: json.ip ?? "-",
      VERSAO: json.fw_version ?? "-",
      SSID: json.ssid ?? "-",
      SINAL: json.wifiPower != null ? `${json.wifiPower} dBm` : "-",
      MODELO: json.device_model ?? "-",
      BOARD: json.device_board ?? "-",
      //MESTRE: json.mestre ?? "-",
      UPTIME: formataUptime(json.uptime),
      //RECURSOS: json.recursos?.length ?? 0,
      MS: ms,
      STATUS: "OK",
    };
  } catch (err) {
    return {
      NODO: nodo,
      STATUS: `ERRO: ${err.name === "TimeoutError" ? "TIMEOUT" : err.message}`,
      MS: Math.round(performance.now() - inicio),
    };
  }
}

async function main() {
  console.log(
    `eTomadaPing! Consultando [${nodos.length}] nodos. Timeout de ${TIMEOUT}ms`,
  );

  const resultados = await Promise.all(nodos.map((nodo) => getSnapshot(nodo)));

  console.table(resultados);

  const ok = resultados.filter((r) => r.STATUS === "OK").length;
  console.log(`Nodos: ${nodos.length} | Online: ${ok}`);
}

main().catch(console.error);
