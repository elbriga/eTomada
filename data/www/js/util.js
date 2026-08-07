function statusMsg(msg) {
  document.getElementById("status").innerHTML = msg;
}

function editModalOpen() {
  document.getElementById("modalOverlay").classList.add("open");
  document.getElementById("editModal").classList.add("open");
}

function editModalClose() {
  releEditando = null;

  document.getElementById("modalOverlay").classList.remove("open");
  document.getElementById("editModal").classList.remove("open");
}

function escapeHtml(str) {
  return String(str)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function getHoraFromTS(ts) {
  var date = new Date(ts * 1000);
  var h = "0" + date.getHours();
  var m = "0" + date.getMinutes();
  return h.slice(-2) + ":" + m.slice(-2);
}

function formataTempo(millis) {
  const secs = Math.floor(millis / 1000);

  const dias = Math.floor(secs / 86400);
  const horas = Math.floor((secs % 86400) / 3600);
  const minutos = Math.floor((secs % 3600) / 60);
  const segundos = secs % 60;

  const tempo =
    String(horas).padStart(2, "0") +
    ":" +
    String(minutos).padStart(2, "0") +
    ":" +
    String(segundos).padStart(2, "0");

  if (dias > 0) {
    return `${dias}d ${tempo}`;
  }

  return tempo;
}
