const API_BASE =
  window.location.host === "localhost"
    ? "http://192.168.1.141" // IP do ESP quando o frontend esta hospedado para DEV
    : window.location.origin;

let eTomadaData = null;

function eTomadaInit() {
  sseInit();
  // Nao precisa Render aqui pois vem o evento sse_snapshot ao conectar
  // eTomadaRender();
}

async function eTomadaRefresh(snapshot) {
  if (!snapshot) {
    snapshot = await eTomadaAPI("getSnapshot");
  }

  if (!snapshot.reles || !snapshot.sensores) {
    statusMsg("Erro nos dados!");
    return;
  }

  if (snapshot.api < 3) {
    statusMsg("Erro versao API!");
    return;
  }

  eTomadaData = snapshot;
}

let _eTomadaLoading = false;
async function eTomadaRender(snapshot) {
  statusMsg("");

  if (_eTomadaLoading) return;
  _eTomadaLoading = true;

  try {
    await eTomadaRefresh(snapshot);

    document.getElementById("datahora").innerHTML =
      "uptime: " +
      formataTempo(eTomadaData.uptime) +
      " - " +
      eTomadaData.datahorastr;

    sensoresRender(eTomadaData.sensores);

    relesRender(eTomadaData.reles);
  } catch (e) {
    statusMsg("Erro eTomadaRender: " + e);
  } finally {
    _eTomadaLoading = false;
  }
}

function eTomadaDiscover() {
  eTomadaAPI("discover");
}

function eTomadaRoleta() {
  eTomadaAPI("roleta");
}
