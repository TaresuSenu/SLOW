#ifndef PERIPHERAL_H
#define PERIPHERAL_H

#include "slow.h"

#include <sys/types.h>   // Tipos de dados para sockets
#include <sys/socket.h>  // Definições principais de sockets (socket, sendto, recvfrom)
#include <netinet/in.h>  // Estruturas de endereço de internet (struct sockaddr_in)
#include <arpa/inet.h>   // Funções para manipulação de endereços IP (inet_pton)
#include <netdb.h>       // Para resolução de nomes de host (gethostbyname, getaddrinfo)
#include <unistd.h>      // Para close() do socket

enum class AckStatus {
    ACK_OK,         // ACK correto recebido
    TIMEOUT,        // recvfrom timedout
    INVALID_PACKET, // Pacote recebido, mas não é o ACK esperado ou é inválido
    RECV_ERROR      // Outros
};

struct PreviousSessionInfo {
    SID sid = SID::Nil();
    uint32_t sttl = 0;
    uint32_t lastCentralSeqNum = 0; // O último seqNum que recebemos do central na sessão anterior
    bool valid = false; // Indica se há informação válida de sessão anterior
};

class Peripheral{
    public:
        Peripheral();
        ~Peripheral();

        bool initNetwork(const char * hostName, int port);
        bool connect();
        bool disconnect();
        bool sendData(const string & data);
        bool sendFragmentedData(const string & data, int fid, int fo, bool MB);
        void processAck(SlowHeader ackHeader);
        bool canSendFragment(size_t fragmentSize);
        bool zeroWayConnect(const string & data);
        void storeSession();
        bool canRevive();
    private:
    int sockFileDescriptor;
    struct sockaddr_in centralAddress;

    int lastAckNumFromCentral;
    int lastWindowFromCentral;

    SlowHeader packet;
    SlowHeader lastReceivedAckHeader;
    SID currentSessionId;
    bool sessionON;
    uint32_t nextSeqNumToSend = 0;

    uint32_t centralSttl; 
    uint32_t centralIniSeqNum;
    uint32_t lastCentralSeqNum;
    uint16_t centralWindowSize;


    PreviousSessionInfo prevSessionInfo;

    bool sendConnectMessage();
    bool waitSetupMessage(); // espera a mensagem setup da central
    bool sendDataMessage();
    AckStatus waitAck();
    bool sendDisconnectMessage();
};



#endif
