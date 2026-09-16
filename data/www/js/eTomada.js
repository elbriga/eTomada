const API_BASE =
  window.location.host == "localhost" ||
  window.location.host == "pi" ||
  window.location.host == "pi.casa"
    ? "http://10.0.2.200" // IP do ESP quando o frontend esta hospedado para DEV
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

  if (!snapshot.recursos) {
    statusMsg("Erro nos dados, sem recursos!");
    return;
  }

  eTomadaData = snapshot;
}

let _eTomadaLoading = false;
async function eTomadaRender(snapshot) {
  statusMsg("");

  if (_eTomadaLoading) return;
  _eTomadaLoading = true;

  {
    await eTomadaRefresh(snapshot);

    document.title = eTomadaData.device_id
      ? `${eTomadaData.device_id} - eTomada`
      : "eTomada";

    document.getElementById("datahora").innerHTML =
      "uptime: " +
      formataTempo(eTomadaData.uptime) +
      " - " +
      eTomadaData.datahorastr;

    sensoresRenderFromRecursos();
    relesRenderFromRecursos();
    botoesRenderFromRecursos();
    umidificadorRenderFromRecursos();

    regrasRenderFromSnapshot();
  }

  _eTomadaLoading = false;
}

function eTomadaRoleta() {
  eTomadaAPI("roleta");
}
