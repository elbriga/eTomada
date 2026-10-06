#include <Preferences.h>

#include "eTomada.h"
#include "http.h"
#include "loga.h"
#include "wifi.h"
#include "recovery.h"
#include "api.h"

#define DEV // TODO :: remover

// Função de log para esta modulo
#define logaM(nivel, fmt, ...) loga("HTTP", nivel, fmt, ##__VA_ARGS__)

// Web Server
AsyncWebServer httpServer(80);
AsyncEventSource sse("/events");
String httpSenha;

void httpServerInitModoAP();
void httpServerInitModoAPI();
void httpMiddlewareAuth(AsyncWebServerRequest *request, ArMiddlewareNext next);

void httpServerInit()
{
  logaM(LOG_NORMAL, "Inicializando o servidor http");

  Preferences prefs;
  prefs.begin("eTomada", false);

  // Para setar:
  // prefs.putString("adminPass", "sapo");

  if (!prefs.isKey("adminPass"))
    prefs.putString("adminPass", ETOMADA_HTTP_DEFAULT_PASSWORD);

  httpSenha = prefs.getString("adminPass");
  prefs.end();

#ifdef DEV
  //  Adicionar headers para functionar o CORS quando em DEV localhost
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
#endif

  if (WiFiGetModoAP())
    httpServerInitModoAP();
  else
    httpServerInitModoAPI();

  httpServer.onNotFound([](AsyncWebServerRequest *request)
                        {
    if (WiFiGetModoAP()) {
      logaRequest(request, "Redir /");
      request->redirect("/");
      return;
    }

    // Tratar o OPTIONS
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
      logaRequest(request, "OPTIONS");
    } else {
      request->send(404);
      logaRequest(request, "404 Not Found");
    } });

  httpServer.begin();
}

void httpServerInitModoAPI()
{
  httpServer.addMiddleware(httpMiddlewareAuth);

  ArRequestHandlerFunction funcVazia = [](AsyncWebServerRequest *request) {};

  httpServer.on("/api/getSnapshot", HTTP_GET, apiSnapshot);
  httpServer.on("/api/getRecurso", HTTP_GET, apiGetRecurso);
  httpServer.on("/api/setRecurso", HTTP_PUT, funcVazia, NULL, apiSetRecurso);
  httpServer.on("/api/setRecursoConfig", HTTP_PUT, funcVazia, NULL, apiSetRecursoConfig);
  httpServer.on("/api/mock", HTTP_POST, funcVazia, NULL, apiMock);
  httpServer.on("/api/getFile", HTTP_GET, apiGetFile);
  httpServer.on("/api/setRegra", HTTP_PUT, funcVazia, NULL, apiSetRegra);
  httpServer.on("/api/delRegra", HTTP_PUT, funcVazia, NULL, apiDelRegra);
  httpServer.on("/api/factoryReset", HTTP_POST, apiFactoryReset);
  httpServer.on("/api/resetWiFiConfig", HTTP_POST, funcVazia, NULL, apiResetWifiConfig);
  httpServer.on("/api/setConfig", HTTP_POST, funcVazia, NULL, apiSetConfig);
  httpServer.on("/api/checkWWW", HTTP_GET, apiCheckWWW);
  httpServer.on("/api/roleta", HTTP_GET, apiRoleta);

  if (eTomadaGetModoOperacao() == MODO_CONTROLADOR)
  {
    httpServer.on("/api/evento", HTTP_POST, funcVazia, NULL, apiEvento);
    httpServer.on("/api/getNodo", HTTP_GET, apiGetNodo);
    httpServer.on("/api/addNodo", HTTP_PUT, funcVazia, NULL, apiAddNodo);
    httpServer.on("/api/delNodo", HTTP_PUT, funcVazia, NULL, apiDelNodo);
    httpServer.on("/api/addRecursoRemoto", HTTP_PUT, funcVazia, NULL, apiAddRecursoRemoto);
    httpServer.on("/api/delRecursoRemoto", HTTP_PUT, funcVazia, NULL, apiDelRecursoRemoto);
  }

  recoveryAPIRegister();

  // Eventos de conexão/desconexão
  sse.authorizeConnect([](AsyncWebServerRequest *request)
                       {
    String remoteIP = request->client()->remoteIP().toString();
    String clientIP = remoteIP;
    if (request->hasHeader("CF-Connecting-IP"))
    {
      const AsyncWebHeader *forwardedIP = request->getHeader("CF-Connecting-IP");
      if (forwardedIP && forwardedIP->value().length() > 0)
        clientIP = forwardedIP->value();
    }

    logaM(LOG_NORMAL, "Cliente SSE conectado de [%s]", clientIP.c_str());
    return true; });

  sse.onConnect([](AsyncEventSourceClient *client)
                {
    // Snapshot ao conectar
    String body = eTomadaGetSnapshotJSON();
    client->send(body, "sse_snapshot", millis(), 2500); });

  httpServer.addHandler(&sse);

  httpServer.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");
}

void httpServerInitModoAP()
{
  ArRequestHandlerFunction funcRedir = [](AsyncWebServerRequest *request)
  {
    request->redirect("/");
    logaRequest(request, "Redir /");
  };

  httpServer.on("/generate_204", HTTP_GET, funcRedir);        // Android
  httpServer.on("/hotspot-detect.html", HTTP_GET, funcRedir); // iOS
  httpServer.on("/connecttest.txt", HTTP_GET, funcRedir);     // Windows

  httpServer.on("/api/redes", HTTP_GET, apiAPRedes);
  httpServer.on("/api/setWiFiConfig", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, apiAPSetWiFiConfig);

  httpServer.serveStatic("/", LittleFS, "/www/").setDefaultFile("portal.html");
}

void httpEnviaSSE(String msg, String tipo)
{
  sse.send(msg, tipo);
}

void httpEnviaSSERefresh()
{
  String body = eTomadaGetSnapshotJSON();
  httpEnviaSSE(body, "sse_snapshot");
}

// Auth
void httpSetSenha(const String &senha)
{
  httpSenha = senha;

  Preferences prefs;
  prefs.begin("eTomada", false);
  prefs.putString("adminPass", senha);
  prefs.end();
}

String httpGetSenha()
{
  return httpSenha;
}

constexpr uint8_t AUTH_FAILURE_LIMIT = 5;
constexpr uint32_t AUTH_FAILURE_WINDOW_MS = 10UL * 60 * 1000;
constexpr uint32_t AUTH_BLOCK_DURATION_MS = 15UL * 60 * 1000;
constexpr size_t AUTH_TRACKED_CLIENTS = 16;

struct AuthClientState
{
  String ip;
  uint8_t failures = 0;
  uint32_t windowStartedAt = 0;
  uint32_t blockedAt = 0;
  uint32_t lastSeenAt = 0;
};

AuthClientState authClientStates[AUTH_TRACKED_CLIENTS];

AuthClientState &authClientStateFor(const String &ip, uint32_t now)
{
  AuthClientState *available = nullptr;
  AuthClientState *oldest = &authClientStates[0];

  for (AuthClientState &state : authClientStates)
  {
    if (state.ip == ip && state.ip.length() > 0)
    {
      state.lastSeenAt = now;
      return state;
    }

    if (state.ip.length() == 0 && available == nullptr)
      available = &state;

    if (now - state.lastSeenAt > now - oldest->lastSeenAt)
      oldest = &state;
  }

  AuthClientState &state = available ? *available : *oldest;
  state.ip = ip;
  state.failures = 0;
  state.windowStartedAt = now;
  state.blockedAt = 0;
  state.lastSeenAt = now;
  return state;
}

IPAddress httpGetClientIP(AsyncWebServerRequest *request)
{
  IPAddress clientIP = request->client()->remoteIP();
  if (request->hasHeader("CF-Connecting-IP"))
  {
    const AsyncWebHeader *forwardedIP = request->getHeader("CF-Connecting-IP");
    if (forwardedIP && forwardedIP->value().length() > 0)
      clientIP.fromString(forwardedIP->value());
  }
  return clientIP;
}

void httpMiddlewareAuth(AsyncWebServerRequest *request, ArMiddlewareNext next)
{
  if (httpSenha.length() == 0)
  {
    next();
    return;
  }

  // Deixa passar o OPTIONS e os IPs da rede 10.x.x.x (rede local)
  String clientIP = httpGetClientIP(request).toString();
  if (request->method() == HTTP_OPTIONS || clientIP.startsWith("10."))
  {
    next();
    return;
  }

  uint32_t now = millis();
  AuthClientState &state = authClientStateFor(clientIP, now);
  if (state.failures >= AUTH_FAILURE_LIMIT && now - state.blockedAt < AUTH_BLOCK_DURATION_MS)
  {
    request->send(429, "text/plain", "Muitas tentativas de login. Tente novamente mais tarde.");
    logaRequest(request, "429 Too Many Requests");
    return;
  }

  if (now - state.windowStartedAt >= AUTH_FAILURE_WINDOW_MS || state.failures >= AUTH_FAILURE_LIMIT)
  {
    state.failures = 0;
    state.windowStartedAt = now;
    state.blockedAt = 0;
  }

  if (request->authenticate(ETOMADA_HTTP_USERNAME, httpSenha.c_str()))
  {
    state.failures = 0;
    state.windowStartedAt = now;
    state.blockedAt = 0;
    next();
    return;
  }

  state.failures++;
  if (state.failures >= AUTH_FAILURE_LIMIT)
  {
    state.blockedAt = now;
    request->send(429, "text/plain", "Muitas tentativas de login. Tente novamente mais tarde.");
    logaRequest(request, "429 Too Many Requests 2");
    return;
  }

  request->requestAuthentication(AsyncAuthType::AUTH_BASIC, "eTomada");
}

void logaRequest(AsyncWebServerRequest *request, String resultado)
{
  logaM(LOG_NORMAL, "[org:%s] %s %s => [%s]",
        request->client()->remoteIP().toString(),
        request->methodToString(),
        request->url().c_str(),
        resultado.c_str());
}
