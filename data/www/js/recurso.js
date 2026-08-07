function recursoAtualiza(recurso) {
  const idx = eTomadaData.recursos.findIndex((r) => r.id === recurso.id);

  if (idx === -1) {
    // Novo
    eTomadaData.recursos.push(recurso);
  } else {
    eTomadaData.recursos[idx] = recurso;
  }

  let newCard;
  switch (recurso.tipo) {
    case "RELE":
      newCard = releGetCard(recurso);
      break;

    case "SENSOR":
      newCard = sensorGetCard(recurso);
      break;

    case "BOTAO":
      newCard = botaoGetCard(recurso);
      break;

    default:
      break;
  }

  const oldCard = document.getElementById(`recursoCard-${recurso.id}`);
  oldCard.parentNode.replaceChild(newCard, oldCard);

  if (recurso.tipo == "SENSOR") {
    // Ao mudar o nome do sensor pode precisar atualizar regras SE dos reles
    relesRenderFromRecursos();
  }
}

function recursoGet(id) {
  return eTomadaData.recursos.find((r) => r.id === id) || null;
}
