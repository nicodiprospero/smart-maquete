# Maquete Inteligente — Interface Web

Arquivos servidos pelo ESP32-S3 via LittleFS.

## Conteúdo

- `index.html` — interface principal do cliente/visitante.
- `admin.html` — painel administrativo do corretor (senha: `admin123`).

Ambas se conectam em `ws://<host>/ws` e trocam mensagens JSON com o firmware.

## Preview local

Como os arquivos são standalone (CSS e JS inline), basta um servidor HTTP qualquer
servindo a pasta `data/`:

```bash
cd data
python -m http.server 8080
```

Depois abra:

- http://localhost:8080/            — interface principal
- http://localhost:8080/admin.html  — painel admin

> Sem o ESP32, o WebSocket não conecta — o indicador no header ficará em
> "Reconectando…". A UI funciona normalmente em modo offline (com os 12
> canais default), apenas os comandos não terão efeito real.

## Deploy no ESP32

Coloque os dois `.html` na pasta `data/` do sketch Arduino e faça upload do
LittleFS (ferramenta "ESP32 LittleFS Data Upload" ou equivalente).

## Protocolo WebSocket

Mensagens enviadas pelo cliente:

```js
{ action:"setChannel",    channel:0, value:800, fade:true }
{ action:"scene",         name:"noite" }              // dia | noite | tour | destaque
{ action:"allOn" }
{ action:"allOff" }
{ action:"renameChannel", channel:0, name:"Cobertura", kind:"andar" }
{ action:"setTourSpeed",  speed:600 }
```

Estado recebido do firmware (broadcast a todos os clientes):

```json
{
  "type": "state",
  "channels": [
    { "id":0, "name":"Andar 1", "type":"andar", "value":750 }
  ],
  "scene": "noite",
  "tourSpeed": 800
}
```

Tipos de canal: `andar`, `comum`, `fachada`.
