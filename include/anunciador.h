#include "recurso.h"

typedef enum
{
    ANUNCIO_RECURSO
} TipoAnuncio;

typedef struct
{
    TipoAnuncio tipo;
    Recurso *recurso;
} Anuncio;

void anunciadorInit();
void anunciadorPost(Anuncio anuncio);
