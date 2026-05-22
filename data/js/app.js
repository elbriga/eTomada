const API_BASE =
  window.location.host === "localhost"
    ? "http://192.168.18.105" // IP do ESP quando o frontend esta hospedado para DEV
    : window.location.origin;

let eTomadaData = null;

function init() {
  sseInit();
  load();
}

let loading = false;
async function load() {
  statusMsg("");

  if (loading) return;
  loading = true;

  try {
    eTomadaData = await eTomadaAPI("data");

    if (!eTomadaData.reles || !eTomadaData.sensores) {
      statusMsg("Erro nos dados!");
      return;
    }

    document.getElementById("datahora").innerHTML =
      "uptime: " +
      formataTempo(eTomadaData.uptime) +
      " - " +
      eTomadaData.datahorastr;

    renderReles(eTomadaData.reles);
    renderSensores(eTomadaData.sensores);
  } catch (e) {
    statusMsg("Erro ao carregar: " + e);
  } finally {
    loading = false;
  }
}
