#pragma once

typedef enum
{
  RECURSO_TODOS = 0,
  RECURSO_RELE = 1,
  RECURSO_SENSOR = 2,
  RECURSO_BOTAO = 3,
  RECURSO_UMIDIFICADOR = 4,
  RECURSO_INVALIDO = 250,
} TipoRecurso;

enum ComandoRecurso
{
  COMANDO_NENHUM = 0,
  COMANDO_ON = 10,
  COMANDO_OFF = 20,
  COMANDO_TOGGLE = 30,
  COMANDO_PULSE = 40
};

ComandoRecurso comandoRecursoGetFromString(String comando);
const char *comandoRecursoGetString(ComandoRecurso comando);
