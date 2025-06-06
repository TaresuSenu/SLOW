# Protocolo SLOW - Implementação do Peripheral

## 1. Introdução

Este projeto é uma implementação do lado "Peripheral" do protocolo SLOW. O SLOW é um protocolo ad hoc da camada de transporte projetado para controle de fluxo de dados, utilizando UDP como sua infraestrutura de comunicação. Esta implementação foca nas funcionalidades essenciais do peripheral conforme especificado para o trabalho.

O protocolo SLOW opera na porta `udp/7033` tanto para o `central` quanto para o `peripheral`. Todos os pacotes SLOW não devem exceder 1472 bytes (incluindo o cabeçalho) e utilizam a ordenação de bits little-endian.

## 2. Funcionalidades Implementadas no Peripheral

O peripheral implementado suporta as seguintes operações principais:

* **Estabelecimento de Conexão (Handshake de 3 Vias)**:
    1.  Peripheral envia uma mensagem `Connect` para o central.
    2.  Aguarda uma mensagem `Setup` do central, que pode aceitar ou rejeitar a conexão.
    3.  Se aceito, o peripheral envia uma mensagem `Data` que também serve como ACK para o `Setup`, completando o handshake.
* **Envio de Dados de Aplicação**:
    * Após a conexão estabelecida, o peripheral pode enviar pacotes de dados para o central.
    * Cada pacote de dados enviado aguarda um `Ack` do central.
    * Implementa uma lógica básica de retransmissão: se um `Ack` não é recebido após um timeout, o pacote de dados é reenviado algumas vezes.
* **Desconexão da Sessão**:
    * O peripheral pode enviar uma mensagem `Disconnect` para o central. Esta mensagem é caracterizada pelas flags `Connect`, `Revive` e `Ack` todas ativas.
    * Aguarda um `Ack` do central para confirmar a desconexão.
* **0-Way Connect (Revive de Sessão)**:
    * Permite tentar reativar uma sessão anterior válida enviando uma mensagem `Data` com a flag `Revive` ativa. Esta mensagem já pode conter dados da aplicação.
    * O central pode responder com um `Ack` (contendo uma flag `Accept/Reject` para indicar o sucesso ou falha do revive) ou uma mensagem `Failed` explícita (com SID Nil e flag `Reject`).

## 3. Estrutura do Cabeçalho SLOW (Resumido)

O cabeçalho SLOW possui os seguintes campos principais:
* `sid` (128 bits): ID da Sessão (UUIDv8).
* `sttl` (27 bits): Time-To-Live da sessão (uso do central).
* `flags` (5 bits, conforme diagrama): `C` (Connect), `R` (Revive), `ACK` (Acknowledge), `A/R` (Accept/Reject), `MB` (More Bits).
* `seqnum` (32 bits): Número de sequência do pacote.
* `acknum` (32 bits): Número de sequência do último pacote recebido.
* `window` (16 bits): Tamanho da janela de recepção.
* `fid` (8 bits): ID de Fragmento.
* `fo` (8 bits): Offset de Fragmento.

## 4. Como Compilar

Este projeto foi desenvolvido em C++. Para compilar, você precisará de um compilador C++ (como g++). Assumindo que os arquivos de implementação (`main.cpp`, `peripheral.cpp`, `slow.cpp`) e cabeçalho (`peripheral.h`, `slow.h`) estão no mesmo diretório, use o Makefile que veio junto, para compilar o projeto:

```bash
make
```
## 5. Como Executar e Servidor de Teste

Após a compilação, execute o programa:

```bash
./peripheral_slow
```
O peripheral tentará se conectar ao servidor central de teste especificado no código-fonte, que é `slow.gmelodie.com:7033`

## 6. Exemplos de Utilização (Interface Interativa)
A aplicação `main.cpp` desenvolvida entra em um estado de conexão e, em seguida, em um loop onde você pode digitar comandos.

### a. Conexão Inicial bem-sucedida:
Ao iniciar, o programa automaticamente tenta o handshake de 3 vias:
```bash
$ ./peripheral_slow
Socket UDP criado com sucesso: 3
Endereço central: slow.gmelodie.com:7033
Mensagem Connect enviada sem problemas
Aguardando mensagem Setup do central...
Pacote recebido do central (32 bytes).
Cabeçalho Setup desserializado.
Conexão aceita pela central
SID da Sessão: [ID da sessão do central]
STTL: [STTL do central]
SeqNum Inicial do Central: [SeqNum do central]
Janela do Central: [Janela do central]
Setup bem sucedido
Preparando mensagem Data (ACK do handshake)...
Cabeçalho Data (ACK do handshake) serializado. SeqNum: 1, AckNum: [SeqNum do central]
Mensagem Data (ACK do handshake) enviada com sucesso (32 bytes).
Aguardando ACK final do handshake do central...
Pacote ACK recebido do central (32 bytes).
Cabeçalho ACK desserializado.
ACK final do handshake validado com sucesso!
STTL atualizado: [STTL]
Próximo SeqNum esperado do Central: [Próximo SeqNum do Central]
Janela atualizada do Central: [Janela]
CONEXÃO COMPLETAMENTE ESTABELECIDA
Digite 'data' para enviar uma mensagem, 'disconnect' para desconectar, 'revive' para 0-way, ou 'end' para sair.
>
```

### b. Enviando Dados:
Após a conexão, digite `data` e pressione Enter. Em seguida, digite sua mensagem e pressione Enter.
```bash
> data
Digite os dados a enviar: Ola SLOW!
Preparado para enviar dados. SeqNum: 2, Payload: 10 bytes.
Pacote de dados enviado (SeqNum: 2). Aguardando ACK...
Pacote ACK recebido do central (32 bytes).
Cabeçalho ACK desserializado.
ACK para dados (SeqNum: 2) recebido e validado!
Dados enviados com êxito.
>
```
### c. Testando o 0-Way Connect (Revive):
Para testar o revive, primeiro precisamos de uma sessão que foi "salva". A forma como o `main.cpp` foi estruturado permite isso se você primeiro encerrar uma sessão com "end" ou usar o comando "revive" (que internamente salva e desconecta a sessão atual para teste).

Exemplo de fluxo para testar "revive" (supondo que uma sessão está ativa):
```bash
> revive
INFO: Preparando para testar revive. Armazenando sessão atual (se ativa) e desconectando...
Informações da sessão atual armazenadas para possível revive.
Mensagem de disconnect enviada E ACK recebido e validado. (ou similar)
Tentando 0-Way connect (revive)...
Digite dados para enviar com o revive (ou deixe em branco): Testando revive
Mensagem Data (revive) enviada. SeqNum: X, AckNum: Y. Aguardando resposta...
Pacote recebido do central (32 bytes).
Cabeçalho desserializado.
0-Way Connect ACEITO! Sessão reavivada.
   STTL: [STTL]
   Próximo SeqNum do Central: [SeqNum]
   Janela do Central: [Janela]
0-Way Connect (Revive) BEM-SUCEDIDO!
>
```
Se o revive for rejeitado:
```bash
...
Mensagem Data (revive) enviada. SeqNum: X, AckNum: Y. Aguardando resposta...
Pacote recebido do central (32 bytes).
Cabeçalho desserializado.
0-Way Connect REJEITADO (mensagem Failed recebida do central).
Falha na tentativa de 0-Way Connect (Revive).
>
```

### d. Desconectando a Sessão:
Digite `disconnect` e pressione Enter.
```bash
> disconnect
Mensagem de disconnect enviada E ACK recebido e validado.
Desconectado com êxito.
(O programa pode terminar ou esperar um novo comando dependendo da lógica exata do 'break' no main.cpp)
```
### e. Encerrando o Programa
Digite `end` e pressione Enter. Isso tentará salvar a sessão para um futuro revive e desconectar antes de sair.
```bash
> end
Informações da sessão atual armazenadas para possível revive.
Mensagem de disconnect enviada E ACK recebido e validado.
(Programa encerra)
```

