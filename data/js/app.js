const API_BASE =
  window.location.host === "localhost"
    ? "http://192.168.18.105" // IP do ESP quando o frontend esta hospedado para DEV
    : window.location.origin;

let eTomadaData = null;

function init() {
  sseInit();
  //load();
}

async function eTomadaRender(snapshot) {
  if (!snapshot) {
    snapshot = await eTomadaAPI("data");
  }

  if (!snapshot.reles || !snapshot.sensores) {
    statusMsg("Erro nos dados!");
    return;
  }

  if (!snapshot.api) {
    // TODO!
    statusMsg("Erro versao API!");
    return;
  }

  eTomadaData = snapshot;

  document.getElementById("datahora").innerHTML =
    "uptime: " +
    formataTempo(eTomadaData.uptime) +
    " - " +
    eTomadaData.datahorastr;

  sensoresRender(eTomadaData.sensores);

  relesRender(eTomadaData.reles);
}

let loading = false;
async function load() {
  statusMsg("");

  if (loading) return;
  loading = true;

  try {
    await eTomadaRender();
  } catch (e) {
    statusMsg("Erro ao carregar: " + e);
  } finally {
    loading = false;
  }
}
