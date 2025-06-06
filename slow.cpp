#include "slow.h"

void serializationOf32bits(uint32_t val, uint8_t * buffer){
    for(int i=0;i<4;i++){
        buffer[i] = val & 0xFF;
        val = val >> 8;
    } // elimina os 2 LSBytes e depois pega os novos 2 LSBytes
}

uint32_t deserializationOf4bytes(uint8_t * buffer){
    uint32_t ret = 0;

    for(int i=0;i<4;i++){
        uint32_t buff = buffer[i];
        ret |= (buff<<(i*8));
    }

    return ret;
}

void serializationOf16bits(uint16_t val, uint8_t * buffer){
    for(int i=0;i<2;i++){
        buffer[i] = val & 0xFF;
        val = val >> 8;
    } // elimina os 2 LSBytes e depois pega os novos 2 LSBytes
}

uint16_t deserializationOf2bytes(uint8_t * buffer){
    uint16_t ret = 0;

    for(int i=0;i<2;i++){
        uint16_t buff = buffer[i];
        ret |= (buff<<(i*8));
    }

    return ret;
}

void serializationOfSlowHeader(SlowHeader & header, uint8_t * buffer){
    // serializa SID
    for(int i=0;i<16;i++) buffer[i] = header.sid.byte[i];

    //serializa sttl e flags etc...
    serializationOf32bits(header.sttlAndFlags, &buffer[16]);
    serializationOf32bits(header.seqNum, &buffer[20]);
    serializationOf32bits(header.ackNum, &buffer[24]);

    serializationOf16bits(header.window, &buffer[28]);

    buffer[30] = header.fid;
    buffer[31] = header.fo;
}

void deserializationForSlowHeader(SlowHeader & header, uint8_t * buffer){
    for(int i=0;i<16;i++) header.sid.byte[i] = buffer[i];

    header.sttlAndFlags = deserializationOf4bytes(&buffer[16]);
    header.seqNum = deserializationOf4bytes(&buffer[20]);
    header.ackNum = deserializationOf4bytes(&buffer[24]);

    header.window = deserializationOf2bytes(&buffer[28]);

    header.fid = buffer[30];
    header.fo = buffer[31];
}