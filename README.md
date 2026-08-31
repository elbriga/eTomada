# ⚡ eTomada

Controle inteligente de tomadas e sensores via Wi-Fi usando ESP32.

O **eTomada** permite ligar e desligar tomadas remotamente, criar automações por horário ou sensores e acompanhar tudo em tempo real pelo navegador — sem aplicativos.

---

# ✨ Recursos

- 📱 Interface web responsiva
- ⚡ Controle remoto de relés/tomadas
- ⏰ Automação por horário
- 🌡️ Integração com sensores
- 🔄 Atualização em tempo real (SSE)
- 📶 Configuração Wi-Fi pelo navegador
- 💾 Configuração salva na memória do ESP32
- 🧩 Sistema modular de sensores
- 🌙 Interface escura otimizada para celular

---

# 🖥️ Interface

O sistema possui:

## Painel principal

- Visualização das tomadas
- Estado atual (ligado/desligado)
- Nome personalizado
- Regras configuradas
- Controle manual
- Valores dos sensores em tempo real

---

## Configuração

Permite configurar:

- GPIO de cada tomada
- Tipo de cada sensor
- Nome das tomadas
- Regras automáticas

---

# 📦 Hardware necessário

## Compatível com

- ESP32
- ESP8266 (dependendo da versão do firmware)

---

## Relés

Módulos relé compatíveis com GPIO digital.

Exemplo:

- Relé 5V
- Relé SSR
- Módulo 4 canais

---

## Sensores suportados

Atualmente o sistema suporta:

- 🌡️ Temperatura
- 💧 Umidade
- ☀️ Luminosidade (LUX)

O firmware foi desenvolvido para facilitar a adição de novos sensores.

---

# 🚀 Como usar

## 1. Instale o firmware

Compile e envie o projeto para o ESP usando:

- Arduino IDE
- PlatformIO

---

## 2. Ligue o dispositivo

Na primeira inicialização o eTomada cria uma rede Wi-Fi própria.

Exemplo:

```txt
eTomada-Setup
```

---

## 3. Configure o Wi-Fi

1. Conecte-se à rede do dispositivo
2. Abra o navegador
3. Acesse:

```txt
192.168.4.1
```

4. Escolha sua rede Wi-Fi
5. Digite a senha
6. Salve

O dispositivo irá reiniciar e conectar à sua rede.

---

## 4. Acesse o painel

Abra o IP do ESP no navegador.

Exemplo:

```txt
http://192.168.0.120
```

---

# ⚙️ Automação

## Regras por horário

Formato:

```txt
ON|08:00|18:00
```

Liga às 08:00 e desliga às 18:00.

---

## Regras por sensor

Formato:

```txt
SE|S1>30|S1<25
```

Exemplo:

- Liga se temperatura > 30°C
- Desliga se temperatura < 25°C

---

# 🟢 Controle manual

É possível ligar/desligar manualmente uma tomada mesmo com automação ativa.

Quando uma regra automática existe:

- o acionamento manual dura 30 minutos # TODO!
- depois o sistema volta ao modo automático

---

# 🔄 Atualizações em tempo real

O frontend utiliza **Server-Sent Events (SSE)** para:

- atualizar estados instantaneamente
- mostrar sensores em tempo real
- evitar polling constante
- reduzir uso de CPU e rede

---

# 📱 Compatibilidade

Funciona diretamente no navegador:

- Android
- iPhone
- Tablet
- Desktop

Sem necessidade de aplicativo.

---

# 🔒 Segurança

Recomendado para uso em rede local.

Para acesso externo:

- utilize VPN
- ou proxy reverso com autenticação

Evite expor o ESP diretamente na internet.

---

# 🛠️ Tecnologias utilizadas

- ESP32 / ESP8266
- C++
- Arduino Framework
- HTML
- CSS
- JavaScript
- SSE (Server-Sent Events)

---

# 📸 Exemplos de uso

- Automação de iluminação
- Controle de ventiladores
- Estufa automatizada
- Irrigação
- Monitoramento ambiental
- Automação residencial
- Controle de equipamentos

---

# 📄 Licença

Este projeto é open source.

Use, modifique e adapte livremente.

---

# ❤️ Projeto pessoal

O eTomada foi criado com foco em:

- simplicidade
- baixo custo
- baixa latência
- funcionamento local
- facilidade de expansão

Sem dependência de nuvem.
