#include "peripheral.h"

static int current_fid = 0;

Peripheral::Peripheral() : sockFileDescriptor(-1), sessionON(false), nextSeqNumToSend(0){
    memset(&centralAddress, 0, sizeof(centralAddress));
    currentSessionId = SID::Nil();
}

Peripheral::~Peripheral(){
    if(sockFileDescriptor >= 0){
        cout << "fechando o socket" << '\n';
        close(sockFileDescriptor);
    }
}

bool Peripheral::initNetwork(const char * hostName, int port){
    sockFileDescriptor = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockFileDescriptor < 0){
        cout << "Problema na criação do socket!!\n";
        return 0;
    }
    cout << " Socket UDP criado com sucesso: " << sockFileDescriptor << '\n';

    struct hostent * serverInfo;
    serverInfo = gethostbyname(hostName); // <- acho q n aceita a classe string 
    if(serverInfo == NULL){
        cout << "Problema em pegar o host!\n";
        close(sockFileDescriptor);
        sockFileDescriptor = -1;
        return 0;
    }

    centralAddress.sin_family = AF_INET;
    memcpy(&centralAddress.sin_addr.s_addr, serverInfo->h_addr_list[0], serverInfo->h_length);
    centralAddress.sin_port = htons(port);

    cout << "Endereço central: " << hostName << ":" << port << '\n';

    //configurando o timeout

    struct timeval timeVal;
    timeVal.tv_sec = 3000; // timeout definido para 5 segundos
    timeVal.tv_usec = 0; // microsegundos

    if(setsockopt(sockFileDescriptor, SOL_SOCKET, SO_RCVTIMEO, (const char * )&timeVal, sizeof(timeVal)) < 0){
        cout << "WARNING: Falha em configurar o timeout\n";
    }

    return 1;
}

bool Peripheral::connect(){
    if(this->sendConnectMessage()){
        if(this->waitSetupMessage()){
            cout << "Setup bem sucedido\n";
            
            if(this->sendDataMessage()){
                cout << "Envio de Data com sucesso\n";
                if(this->waitAck()==AckStatus::ACK_OK){
                    cout<< "Recebimento do ACK foi feito com êxito\n";
                    cout << "CONEXÃO COMPLETAMENTE ESTABELECIDA\n";
                    return 1;
                }else{
                    cout << "Falha na captura do ACK\n";
                }
            }else{
                cout << "Falha no envio de Data\n";
            }
        }else{
            cout << "Falha no setup da conexão\n";
        }
    }else{
        cout << "Falha no envio da connect\n";
    }

    return 0;
}

bool Peripheral::disconnect(){
    if(this->sendDisconnectMessage()){
        if(this->waitAck()==AckStatus::ACK_OK){
            this->storeSession();
            this->sessionON = false;
            return 1;
        }else{
            cout << "Erro na validação do ack\n";
            this->sessionON = false;
        }
    }else{  
        cout << "Falha no envio da mensagem de disconnect\n";
    }

    return 0;
}

int generateFID() {
    return current_fid++;
}

bool Peripheral::sendFragmentedData(const string & data, int fid, int fo, bool MB){
    if (sockFileDescriptor < 0 || !sessionON) {
        cout << "ERRO: Socket não inicializado ou sessão não ativa. Não é possível enviar dados de aplicação.\n";
        return false;
    }

    SlowHeader dataHeader;

    dataHeader.sid = this->currentSessionId;
    dataHeader.setSttl(this->centralSttl);

    Flags dataFlags;
    dataFlags.ACK = false;
    dataFlags.MB = MB;
    dataHeader.setFlags(dataFlags);

    dataHeader.seqNum = this->nextSeqNumToSend;
   dataHeader.ackNum = this->lastCentralSeqNum; // Último seqnum conhecido do central

    dataHeader.window = 5 * MAX_DATA_SIZE; // Janela de recebimento do peripheral (exemplo)
    dataHeader.fid = fid;
    dataHeader.fo = fo;

    // Preparar o buffer de envio completo (cabeçalho + dados)
    uint8_t sendBuffer[MAX_DATA_SIZE+SLOW_HEADER_SIZE]; // MAX_SLOW_PACKET_SIZE = 1472
    
    // 1. Serializar o cabeçalho no início do buffer
    serializationOfSlowHeader(dataHeader, sendBuffer); // Coloca 32 bytes no buffer

    // 2. Copiar os dados da aplicação para o buffer, logo após o cabeçalho
    size_t dataSize = data.size();
    memcpy(&sendBuffer[SLOW_HEADER_SIZE], data.c_str(), dataSize);

    size_t totalSize = SLOW_HEADER_SIZE + dataSize;

    int tentativas = 3; // tenta no maximo mais 3 vezes depois de dar errado
    bool ackReceived = 0;

    //antes do loop
    this->nextSeqNumToSend++;

    for(int i = 0;i<=tentativas;i++){
        if(i){
            cout << "tentando retransmissão\n";
        }
        if(!i){
            cout << "tentando transmissão de dados\n";
        }

        while (!canSendFragment(data.size())) {
            // Espera liberar espaço no buffer da central.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Enviar pela Rede
        ssize_t bytesSent = sendto(sockFileDescriptor, sendBuffer, totalSize, 0,
                                    (const struct sockaddr *)&centralAddress, sizeof(centralAddress));

        if (bytesSent < 0) {
            cout << "Não foi possivel nem sequer enviar os dados\n";
            this->nextSeqNumToSend--;
            return false;
        } else if (bytesSent != totalSize) {
            cout << "AVISO: Nem todos os bytes do pacote de dados foram enviados ("
                    << bytesSent << "/" << totalSize << ").\n";
            this->nextSeqNumToSend--;
            return false;
        } 

        AckStatus status = this->waitAck();

        if (status == AckStatus::ACK_OK) {
            SlowHeader ackHeader = this->lastReceivedAckHeader;
            processAck(ackHeader);
        }

        if(status == AckStatus::ACK_OK){
            ackReceived = true;
            break;
        }else if(status == AckStatus::TIMEOUT){
            // so não recebeu ack entao tenta enviar denovo.
            cout << "TIMED OUT\n";
        }else{
            cout << "Falha em receber o ACK\n";
            break;
        }

    }

    if(!ackReceived){
        cout << "Falha ao enviar os dados\n";
    }

    return ackReceived;

}

bool Peripheral::sendData(const string & data){
    if (sockFileDescriptor < 0 || !sessionON) {
        cout << "ERRO: Socket não inicializado ou sessão não ativa. Não é possível enviar dados de aplicação.\n";
        return false;
    }

    
    if (data.size() > MAX_DATA_SIZE) {
        int size = data.size();
        int numPackages = (size + MAX_DATA_SIZE - 1) / MAX_DATA_SIZE;

        int fid = generateFID();
        for(int i = 0; i < numPackages; i++){
            int fo = i;
            bool MB = true;
            if(i == numPackages - 1){
                MB = false;
            }

            const string &substring = data.substr(i*MAX_DATA_SIZE, MAX_DATA_SIZE);
            
            sendFragmentedData(substring, fid, fo, MB);
        }

        return true;
    }

    SlowHeader dataHeader;

    dataHeader.sid = this->currentSessionId;
    dataHeader.setSttl(this->centralSttl);

    Flags dataFlags;
    dataFlags.ACK = false;
    dataFlags.MB = false;  // Sem fragmentação por enquanto
    dataHeader.setFlags(dataFlags);

    dataHeader.seqNum = this->nextSeqNumToSend;
    dataHeader.ackNum = this->lastCentralSeqNum; // Último seqnum conhecido do central

    dataHeader.window = 5 * MAX_DATA_SIZE; // Janela de recebimento do peripheral (exemplo)
    dataHeader.fid = 0; // Sem fragmentação
    dataHeader.fo = 0;  // Sem fragmentação

    // Preparar o buffer de envio completo (cabeçalho + dados)
    uint8_t sendBuffer[MAX_DATA_SIZE+SLOW_HEADER_SIZE]; // MAX_SLOW_PACKET_SIZE = 1472
    
    // 1. Serializar o cabeçalho no início do buffer
    serializationOfSlowHeader(dataHeader, sendBuffer); // Coloca 32 bytes no buffer

    // 2. Copiar os dados da aplicação para o buffer, logo após o cabeçalho
    size_t dataSize = data.size();
    memcpy(&sendBuffer[SLOW_HEADER_SIZE], data.c_str(), dataSize);

    size_t totalSize = SLOW_HEADER_SIZE + dataSize;

    int tentativas = 3; // tenta no maximo mais 3 vezes depois de dar errado
    bool ackReceived = 0;

    //antes do loop
    this->nextSeqNumToSend++;

    for(int i = 0;i<=tentativas;i++){
        if(i){
            cout << "tentando retransmissão\n";
        }
        if(!i){
            cout << "tentando transmissão de dados";
        }
        
        while (!canSendFragment(data.size())) {
            // Espera o buffer da central ter espaço.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        // Enviar pela Rede
        ssize_t bytesSent = sendto(sockFileDescriptor, sendBuffer, totalSize, 0,
                                    (const struct sockaddr *)&centralAddress, sizeof(centralAddress));

        if (bytesSent < 0) {
            cout << "Não foi possivel nem sequer enviar os dados\n";
            this->nextSeqNumToSend--;
            return false;
        } else if (bytesSent != totalSize) {
            cout << "AVISO: Nem todos os bytes do pacote de dados foram enviados ("
                    << bytesSent << "/" << totalSize << ").\n";
            this->nextSeqNumToSend--;
            return false;
        } 

        AckStatus status = this->waitAck();

        if(status == AckStatus::ACK_OK){
            ackReceived = true;
            break;
        }else if(status == AckStatus::TIMEOUT){
            // so não recebeu ack entao tenta enviar denovo.
            cout << "TIMED OUT\n";
        }else{
            cout << "Falha em receber o ACK\n";
            break;
        }

    }

    if(!ackReceived){
        cout << "Falha ao enviar os dados\n";
    }

    return ackReceived;

}

bool Peripheral::sendConnectMessage(){
    if(sockFileDescriptor < 0){
        cout << "Sem socket\n";
        return 0;
    }

    SlowHeader connectHeader;

    Flags connectFlags;
    connectFlags.C = true;
    connectHeader.setFlags(connectFlags);

    connectHeader.window = 5 * 1440;

    uint8_t sendBuffer[SLOW_HEADER_SIZE];

    serializationOfSlowHeader(connectHeader, sendBuffer);

    // Em Peripheral::sendConnectMessage(), antes de sendto()
std::cout << "DEBUG: Enviando pacote Connect (Hex): ";
for (int i = 0; i < SLOW_HEADER_SIZE; ++i) { // SLOW_HEADER_SIZE deve ser 32
    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)sendBuffer[i] << " ";
}

std::cout << std::dec << std::endl; // Volta para decimal para outros couts

    //enviar pela rede usando sendto()
    ssize_t bytesSent = sendto(sockFileDescriptor, sendBuffer, SLOW_HEADER_SIZE, 0,
        (const struct sockaddr *)&centralAddress, sizeof(centralAddress));
    
    if(bytesSent < 0){
        perror("sendto");
        cout << "Erro no envio de Connect\n";
        return 0;
    }else if(bytesSent != SLOW_HEADER_SIZE){
        cout << "WARNING: NEM TODOS OS BYTES FORAM ENVIADOS\n";
        cout << bytesSent << "/" << SLOW_HEADER_SIZE << "\n";
        return 0;
    }else{
        cout << "Mensagem enviada sem problemas\n";

        this->nextSeqNumToSend = connectHeader.seqNum + 1;
        return 1;
    }
}

bool Peripheral::waitSetupMessage(){
    if(sockFileDescriptor < 0){
        cout << "Sem socket\n";
        return false;
    }

    uint8_t receiveBuffer[SLOW_HEADER_SIZE+MAX_DATA_SIZE];
    struct sockaddr_in senderAddress;
    socklen_t senderAddressLenght = sizeof(senderAddress);

    // recvfrom espera receber dados ou dar erro
    ssize_t bytesReceived = recvfrom(sockFileDescriptor, receiveBuffer, SLOW_HEADER_SIZE+MAX_DATA_SIZE, 0,
        (struct sockaddr *)&senderAddress, &senderAddressLenght);

    if(bytesReceived < 0){
        perror("recvfrom");
        cout << "Errro ao receber os dados da central\n";
        return false;
    }

    if(bytesReceived < SLOW_HEADER_SIZE){
        cout << "Pacote recebido tem menos bytes que o esperado para um header SLOW (32)\n";
        cout << "Foram recebidos: " << bytesReceived << '\n';
        return false;
    }

    if(bytesReceived > SLOW_HEADER_SIZE){
        size_t payload_length = bytesReceived - SLOW_HEADER_SIZE;
        std::cout << "-----------------------------------------------------------\n";
        std::cout << ">>> MENSAGEM DE ERRO/DADOS DO CENTRAL (Payload): <<<\n";
        // Imprime como string. Adiciona um terminador nulo para segurança se não for uma string bem formada.
        // Ou imprima byte a byte se não tiver certeza que é uma string.
        std::string error_message_from_central;
        error_message_from_central.reserve(payload_length);
        for(size_t i = 0; i < payload_length; ++i) {
            char c = static_cast<char>(receiveBuffer[SLOW_HEADER_SIZE + i]);
            if (isprint(c) || c == '\n' || c == '\r' || c == '\t') { // Apenas caracteres imprimíveis
                error_message_from_central += c;
            } else {
                error_message_from_central += '.'; // Substitui não imprimíveis
            }
        }
        std::cout << error_message_from_central << std::endl;
        std::cout << "-----------------------------------------------------------\n";
    }

    // lendo o header recebido
    SlowHeader setupHeader;
    deserializationForSlowHeader(setupHeader, receiveBuffer);

    Flags receivedFlags = setupHeader.getFlags();

    cout << receivedFlags.toByte() << '\n';

    if(setupHeader.ackNum == 0){
        if(receivedFlags.AR){
            cout << "Conexão aceita pela central\n";

            this->currentSessionId = setupHeader.sid;
            this->centralSttl = setupHeader.getSttl();
            this->centralIniSeqNum = setupHeader.seqNum;
            this->centralWindowSize = setupHeader.window;
            this->nextSeqNumToSend = setupHeader.seqNum+1;

            this->sessionON = true;

            return true;

        }else{ // conexao nao aceita por A/R esta falsa

            // Em Peripheral::sendConnectMessage(), antes de sendto()
std::cout << "DEBUG: Enviando pacote Connect (Hex): ";
for (int i = 0; i < SLOW_HEADER_SIZE; ++i) { // SLOW_HEADER_SIZE deve ser 32
    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)receiveBuffer[i] << " ";
}
std::cout << std::dec << std::endl; // Volta para decimal para outros couts

            cout << "Conexão rejeitada pela central\n";
            this->sessionON = false;
            this->currentSessionId = SID::Nil();
            return false;
        }
    }else{
        cout << "AckNum inválido recebido da Setup Message\n";
        return false;
    }

}

bool Peripheral::sendDataMessage(){
    if(sockFileDescriptor < 0 || !sessionON){
        cout << "Foi tentado enviar Data, porém o socket não está inicializado ou sessão não está ativa\n";
        return false;
    }

    SlowHeader dataHeader;

    dataHeader.sid = this->currentSessionId;
    dataHeader.setSttl(this->centralSttl);

    Flags dataFlags;
    //dataFlags.ACK = 1;
    //dataFlags.MB = 1;
    dataHeader.setFlags(dataFlags);

    dataHeader.seqNum = this->nextSeqNumToSend;
    dataHeader.ackNum = centralIniSeqNum;

    dataHeader.window = 5 * 1440;

    dataHeader.fid = 0;
    dataHeader.fo = 0;

    uint8_t sendBuffer[SLOW_HEADER_SIZE];
    serializationOfSlowHeader(dataHeader, sendBuffer);

    std::cout << "DEBUG: Enviando pacote Data (Hex): ";
for (int i = 0; i < SLOW_HEADER_SIZE; ++i) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)sendBuffer[i] << " ";
}
std::cout << std::dec << std::endl;

    ssize_t bytesSent = sendto(sockFileDescriptor, sendBuffer, SLOW_HEADER_SIZE, 0,
         (const sockaddr *) &centralAddress, sizeof(centralAddress));

    if (bytesSent < 0) {
        perror("sendData");
        cout << "ERRO ao enviar a mensagem Data.\n";
        return false;
    } else if (bytesSent < SLOW_HEADER_SIZE) {
        cout << "WARNING: Nem todos os bytes da mensagem Data foram enviados\n"
                  << bytesSent << "/" << SLOW_HEADER_SIZE << "\n";
        return false; 
    } else {
        cout << "Mensagem Data enviada com sucesso (" << bytesSent << " bytes).\n";
        
        this->nextSeqNumToSend++; // como deu certo ai sim aumentamos o proximo numero de sequencia
        return true;
    }
}

AckStatus Peripheral::waitAck(){
    if(sockFileDescriptor < 0 || !sessionON){
        cout << "Foi tentado receber o ACK, porém o socket não está inicializado ou sessão não está ativa\n";
        return AckStatus::RECV_ERROR;
    }

    uint8_t receiveBuffer[MAX_DATA_SIZE+SLOW_HEADER_SIZE];
    struct sockaddr_in senderAddress;
    socklen_t senderAddressLenght = sizeof(senderAddress);

    // recvfrom espera receber dados ou dar erro
    ssize_t bytesReceived = recvfrom(sockFileDescriptor, receiveBuffer, SLOW_HEADER_SIZE+MAX_DATA_SIZE, 0,
        (struct sockaddr *)&senderAddress, &senderAddressLenght);

    if (bytesReceived < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            cout << "DEU PAU AQUI\n";
            return AckStatus::TIMEOUT;
        } else {
            perror("recvfrom - waitAck");
            cout << "ERRO SISTEMA ao receber ACK.\n";
            return AckStatus::RECV_ERROR;
        }
    }

    if(bytesReceived < SLOW_HEADER_SIZE){
        cout << "Pacote recebido tem menos bytes que o esperado para um header SLOW (32)\n";
        cout << "Foram recebidos: " << bytesReceived << '\n';
        return AckStatus::INVALID_PACKET;
    }

    // lendo o header recebido
    SlowHeader ackHeader;
    deserializationForSlowHeader(ackHeader, receiveBuffer);

    Flags receivedFlags = ackHeader.getFlags();

    // vamos checar se chegou tudo certo

    //sid
    for(int i=0;i<16;i++){
        if(ackHeader.sid.byte[i]!=this->currentSessionId.byte[i]){
            cout << "SID recebido não corresponde ao SID da sessão\n";
            return AckStatus::INVALID_PACKET;
        }
    }

    //sttl
    //recebemos um sttl atualizado :D
    // mas antes devemos checar as flags !
    // flags
    if(!receivedFlags.ACK || receivedFlags.AR || receivedFlags.C || receivedFlags.MB || receivedFlags.R){
        cout << "Flags do ACK inválidas\n";
        return AckStatus::INVALID_PACKET;
    }
    if(ackHeader.ackNum != this->nextSeqNumToSend-1){
        cout << "AckNum não corresponde ao SeqNum esperado\n";
        return AckStatus::INVALID_PACKET;
    }
    //agora podemos settar o sttl;

    this->centralSttl = ackHeader.getSttl();

    this->lastCentralSeqNum = ackHeader.seqNum;
    this->centralWindowSize = ackHeader.window;

    return AckStatus::ACK_OK;
}

bool Peripheral::sendDisconnectMessage(){
    if(sockFileDescriptor < 0 || !sessionON){
        cout << "Foi tentado enviar disconnect, porém o socket não está inicializado ou sessão não está ativa\n";
        return false;
    }

    SlowHeader disconnectHeader;

    disconnectHeader.sid = this->currentSessionId;

    disconnectHeader.setSttl(this->centralSttl);

    Flags disconnectFlags;

    disconnectFlags.ACK = 0;
    disconnectFlags.C = 0;
    disconnectFlags.R = 0; // ambos c e r ligados signica disconnect
    disconnectFlags.AR = 0;
    disconnectFlags.MB = 0;

    disconnectHeader.setFlags(disconnectFlags);

    disconnectHeader.seqNum = this->nextSeqNumToSend;
    disconnectHeader.ackNum = this->lastCentralSeqNum;

    disconnectHeader.window = 0;
    disconnectHeader.fid = 0;
    disconnectHeader.fo = 0;

    // tamanho do slowheader que é 32 bytes
    uint8_t sendBuffer[SLOW_HEADER_SIZE];

    serializationOfSlowHeader(disconnectHeader, sendBuffer);

    //enviar pela rede usando sendto()
    ssize_t bytesSent = sendto(sockFileDescriptor, sendBuffer, SLOW_HEADER_SIZE, 0,
        (const struct sockaddr *)&centralAddress, sizeof(centralAddress));
    
    if(bytesSent < 0){
        perror("sendto");
        cout << "Erro no envio de Disconnect\n";
        return 0;
    }else if(bytesSent != SLOW_HEADER_SIZE){
        cout << "WARNING: NEM TODOS OS BYTES FORAM ENVIADOS\n";
        cout << bytesSent << "/" << SLOW_HEADER_SIZE << "\n";
        return 0;
    }else{
        cout << "Mensagem de disconnect enviada sem problemas\n";
        this->nextSeqNumToSend++;
        return 1;
    }
}

void Peripheral::storeSession() {
    if (sessionON) { // Só armazena se a sessão atual estiver ativa e configurada
        prevSessionInfo.sid = this->currentSessionId;
        prevSessionInfo.sttl = this->centralSttl;
        prevSessionInfo.lastCentralSeqNum = this->lastCentralSeqNum; // O último seqNum que o central usou
        prevSessionInfo.valid = true;
        cout << "Informações da sessão atual armazenadas para possível revive.\n";
    } else {
        cout << "Nenhuma sessão ativa para armazenar para revive.\n";
    }
}

bool Peripheral::canRevive(){
    return prevSessionInfo.valid;
}

// peripheral.cpp
bool Peripheral::zeroWayConnect(const string& data) {
    if (sockFileDescriptor < 0) {
        cout << "ERRO (0-way): Socket não inicializado.\n";
        return false;
    }
    if (sessionON) {
        cout << "Erro Uma sessão já está ativa. Desconecte primeiro.\n";
        return false;
    }
    if (!prevSessionInfo.valid) {
        cout << "WArning: Nenhuma informação de sessão anterior válida para tentar reestabelecer conexão.\n";
        return false;
    }

    // Verificar se o payload não é muito grande (sem fragmentação por enquanto)
    if (data.size() > MAX_DATA_SIZE) {
        cout << "Erros dados muito longo " << data.size()
                  << " bytes. Máximo: " << MAX_DATA_SIZE << "\n";
        return false;
    }

    cout << "Tentando 0-Way Connect (Revive) para SID anterior...\n";

    SlowHeader reviveHeader;
    reviveHeader.sid = prevSessionInfo.sid;
    reviveHeader.setSttl(prevSessionInfo.sttl); // Usar o STTL da sessão anterior

    Flags revive_flags;
    revive_flags.R = true;  // Flag Revive 
    revive_flags.ACK = false; 
    // mb = 0 se n tem fragmentaçao
    reviveHeader.setFlags(revive_flags);

    reviveHeader.seqNum = this->nextSeqNumToSend; 
    reviveHeader.ackNum = prevSessionInfo.lastCentralSeqNum; // ACK para o último seqnum do central da sessão anterior
    reviveHeader.window = 5 * 1440; 
    // fid e fo = 0 por padrão

    uint8_t sendBuffer[SLOW_HEADER_SIZE+MAX_DATA_SIZE];
    serializationOfSlowHeader(reviveHeader, sendBuffer); // coloca o header no buffer

    size_t dataTam = data.size();
    if (dataTam > 0) {
        memcpy(&sendBuffer[SLOW_HEADER_SIZE], data.c_str(), dataTam); // coloca os dados
    }
    size_t totalSize = SLOW_HEADER_SIZE + dataTam;

    // Enviar a mensagem "Data (revive)"
    ssize_t bytes_sent = sendto(sockFileDescriptor, sendBuffer, totalSize, 0,
                                (const struct sockaddr *)&centralAddress, sizeof(centralAddress));

    if (bytes_sent < 0) {
        perror("sendto - 0-way connect");
        cout << "erro ao enviar mensagem Data de revive.\n";
        return false;
    } else if (bytes_sent != totalSize) {
        cout << "Warning: Nem todos os bytes da mensagem Data de revive foram enviados.\n";
        return false;
    }

    // foi enviado data entao incremente nextseqnum
    this->nextSeqNumToSend++; // Incrementar para o próximo pacote *novo*

    // Esperar pela resposta (Ack com A/R ou Failed)
    // Vamos reutilizar uma lógica similar a waitAck, mas com validação específica
    uint8_t responseBuffer[MAX_DATA_SIZE+SLOW_HEADER_SIZE];
    struct sockaddr_in senderAddress;
    socklen_t senderAddress_len = sizeof(senderAddress);

    // Usando recvfrom com timeout 
    ssize_t bytesReceived = recvfrom(sockFileDescriptor, responseBuffer, MAX_DATA_SIZE+SLOW_HEADER_SIZE, 0,
                             (struct sockaddr *)&senderAddress, &senderAddress_len);

    if (bytesReceived < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            cout << "TIMED OUT ON 0way Sem resposta do central para a tentativa de revive.\n";
        } else {
            perror("recvfrom - 0-way response");
            cout << "Erro ao receber resposta do central.\n";
        }
        return false;
    }

    if (bytesReceived < SLOW_HEADER_SIZE) {
        cout << "Erro Pacote de resposta muito pequeno (" << bytesReceived << " bytes).\n";
        return false;
    }

    SlowHeader responseHeader;
    deserializationForSlowHeader(responseHeader, responseBuffer);
    Flags responseFlags = responseHeader.getFlags();

    // Verificar se é uma mensagem "Failed" (rejeição explícita do revive)
    // SID Nil, sttl 0, flags Reject (A/R=0), seqnum 0, acknum 0
    if (responseFlags.AR == false && responseHeader.sid.isEqual(SID::Nil()) && responseHeader.getSttl() == 0 && responseHeader.seqNum == 0 && responseHeader.ackNum == 0) {
        cout << "0-Way Connect REJEITADO (mensagem Failed recebida do central).\n";
        this->sessionON = false;
        this->currentSessionId = SID::Nil(); // Garantir que não há SID de sessão
        prevSessionInfo.valid = false; // Invalidar info da sessão anterior, pois foi rejeitada
        return false;
    }

    // Verificar se é um "Ack" aceitando o revive
    // A/R deve ser 1 (accepted).
    uint32_t expectedAckNum = this->nextSeqNumToSend - 1; // O seqNum do nosso Data(revive)

    if (responseFlags.ACK && responseFlags.AR && // ACK e Accept
        responseHeader.sid.isEqual(prevSessionInfo.sid) && // SID deve ser o da sessão que tentamos reviver
        responseHeader.ackNum == expectedAckNum) {

        cout << "0-Way Connect ACEITO! Sessão reviveu.\n";
        this->currentSessionId = responseHeader.sid; // Deve ser o mesmo que prevSessionInfo.sid
        this->centralSttl = responseHeader.getSttl();
        this->lastCentralSeqNum = responseHeader.seqNum;
        this->centralWindowSize = responseHeader.window;
        this->sessionON = true;
        prevSessionInfo.valid = true;

        cout << "   STTL: " << this->centralSttl << "\n";
        cout << "   Próximo SeqNum do Central: " << this->lastCentralSeqNum << "\n";
        cout << "   Janela do Central: " << this->centralWindowSize << "\n";
        return true;
    }

    // so é true se entrou no if de aceito com todas as coisas corretas
    cout << "Erro 0way Resposta do central para revive não reconhecida ou inválida.\n";
    this->sessionON = false; 
    return false;
}

uint32_t lastAckNumFromCentral; // ackNum do último ACK recebido
uint32_t lastWindowFromCentral; // window do último ACK recebido

// Atualize essas variáveis ao processar o ACK:
void Peripheral::processAck(SlowHeader ackHeader) {
    this->lastAckNumFromCentral = ackHeader.ackNum;
    this->lastWindowFromCentral = ackHeader.window;

    cout << "window: " << this->lastWindowFromCentral << endl;
}

bool Peripheral::canSendFragment(size_t fragmentSize) {
    uint32_t bytesInFlight = this->nextSeqNumToSend - this->lastAckNumFromCentral;

    cout << "bytesInFlight =  " << this->nextSeqNumToSend << " - " << this->lastAckNumFromCentral << endl;

    return (bytesInFlight + fragmentSize <= this->lastWindowFromCentral);
}
