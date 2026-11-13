# SmartPark IoT

Firmware do ESP32 que simula a cancela do estacionamento inteligente no [Wokwi](https://wokwi.com/). O dispositivo detecta o veículo, consulta a API do SmartPark e libera ou nega o acesso.

## Projetos relacionados

[SmartPark API](https://github.com/kauanpinto/smartpark-api.git)
[SmartPark Web](https://github.com/kauanpinto/smartpark-web.git)

## Componentes

- ESP32 DevKit v1
- 2x sensor ultrassônico HC-SR04 (entrada e saída)
- Servo motor (cancela: 0° fechada, 90° aberta)
- LED verde e LED vermelho (acesso liberado / negado)
- LCD 16x2 I2C (mensagens de status)
- 2x push buttons (simulam entrada e saída manualmente)

## Requisitos

- Conta no [Wokwi](https://wokwi.com/).
- [API](https://github.com/kauanpinto/smartpark-api.git) online com URL pública.
- Uma reserva com status `CONFIRMED` no banco do Firestore.

## Como executar

1. Crie o `secrets.h` no Wokwi com os seus dados:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

#define BACKEND_URL     "https://sua-url-do-backend.com/api/iot/access-request"
#define DEVICE_SECRET   "seu-segredo-aqui"
#define RESERVATION_ID  "id_da_reserva_confirmada"

#endif
```

2. Adicione ao `libraries.txt`:

```text
ArduinoJson
ESP32Servo
LiquidCrystal I2C
```

3. Cole o código no `sketch.ino` e inicie a simulação.

4. Aguarde a conexão na rede `Wokwi-GUEST`.

5. Pressione o botão de entrada ou aproxime um objeto do sensor para simular a chegada de um carro.

## Segurança

- O ESP32 não guarda credenciais do Firebase nem do Mercado Pago, só fala com a API.
- Toda requisição envia o segredo no cabeçalho `x-device-secret`.
- Em caso de falha de rede ou da API, a cancela permanece fechada (LED vermelho).
- Cada evento de entrada ou saída usa um `eventId` único, evitando processamento duplicado.

## Autores

- [Everson Miranda](https://github.com/eversonMI)
- [Kauan Fernando](https://github.com/kauanpinto)
- [Simon Henrique](https://github.com/SimHen90)