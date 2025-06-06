#ifndef SLOW_H
#define SLOW_H

#include <bits/stdc++.h>

using namespace std;

const int SLOW_HEADER_SIZE = 32;
const int MAX_DATA_SIZE = 1440;

struct SID{
    uint8_t byte[16]; // 

    static SID Nil(){
        SID nilSID; // o nilSID é completamete zerado conforme RFC9562.
        for(int i=0;i<16;i++){
            nilSID.byte[i]=0;
        }
        return nilSID;
    }

    bool isEqual(const SID& other) const {
        for(int i=0; i<16; ++i) {
            if (byte[i] != other.byte[i]) return false;
        }
        return true;
    }

    // uuidv8 primeiros 6 octetos sao custom_a
    // depois 4 bits para ver depois 12 bits para custom_b
    // 2 bits para var e o resto 62 bits para custom_c

};

struct Flags{
    bool C = false;
    bool R = false;
    bool ACK = false;
    bool AR = false;
    bool MB = false;

    uint8_t toByte() {
        uint8_t res = 0;
        res |= C ? (1<<4) : 0;
        res |= R ? (1<<3) : 0;
        res |= ACK ? (1<<2) : 0;
        res |= AR ? (1<<1) : 0;
        res |= MB ? (1<<0) : 0;
        return res;
    }

    static Flags fromByte (uint8_t inp){
        Flags flags;

        flags.C = inp&(1<<4);
        flags.R = inp&(1<<3);
        flags.ACK = inp&(1<<2);
        flags.AR = inp&(1<<1);
        flags.MB = inp&(1<<0);

        return flags;
    }
};

struct SlowHeader{
    SID sid;
    uint32_t sttlAndFlags;
    uint32_t seqNum;
    uint32_t ackNum;
    uint16_t window;
    uint8_t fid;
    uint8_t fo;

    // as funçoes operam em sttl e flags por conta de que o resto é so atribuir

    uint32_t getSttl(){
        return sttlAndFlags & 0xffffffe0;
        // 07FFFFFF é 0000-0111-1111-1111-1111-1111-1111-1111
        //              0    E   F    F    F    F   F    F
    }

    Flags getFlags(){
        return Flags::fromByte((sttlAndFlags&0x1f));
        // queremos 1111-1000-0000-0000-0000-0000-0000-0000
        //           f     1   0    0     0    0    0    0
    }

    void setFlags(Flags flags){
        uint8_t flagByte = flags.toByte();
        sttlAndFlags = getSttl() | flagByte;
    }

    void setSttl(uint32_t sttl){
        sttlAndFlags = (sttlAndFlags&0x1f) | (sttl&0xffffffe0);
        //             deixa so as flags           garante que os bits de flag estejam zerados
    }

    SlowHeader() : sid(SID::Nil()), sttlAndFlags(0), seqNum(0), ackNum(0), window(0), fid(0), fo(0) {

    }

};

struct SlowPacket{
    SlowHeader header;
    uint8_t data[1440];
};

void serializationOf32bits(uint32_t val, uint8_t * buffer);
void serializationOf16bits(uint16_t val, uint8_t * buffer);
void serializationOfSlowHeader(SlowHeader & header, uint8_t * buffer);

uint32_t deserializationOf4bytes(uint8_t * buffer);
uint16_t deserializationOf2bytes(uint8_t * buffer);
void deserializationForSlowHeader(SlowHeader & header, uint8_t * buffer);




#endif