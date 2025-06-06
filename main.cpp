#include "peripheral.h"

int main(){
    Peripheral peripheral;

    // inicialzaçao do peripheral 
    if(!peripheral.initNetwork("slow.gmelodie.com", 7033)){
        cout << "Falha ao iniciar o Peripheral\n";
        return 1;
    }

    if(peripheral.connect()){
        while(1){
            cout << "Digite 'data' para enviar uma mensagem, 'disconnect' para desconectar, 'revive' para 0-way, ou 'end' para sair.\n";
            string operation;
            cin >> operation; 

            if(operation == "disconnect"){
                if(peripheral.disconnect()){
                    cout << "Desconectado com êxito.";
                }else{
                    cout << "erro ao enviar a mensagem de disconnect ou validaçao do ack\n";
                }
            }else if(operation == "data"){
                string data; // Use um nome diferente de 'data' se 'data' for um membro da classe ou struct

                if (cin.peek() == '\n') { // Verifica se há um newline pendente
                    cin.ignore();
               }
               cout << "Digite os dados a enviar: ";
               getline(cin, data);

                if(peripheral.sendData(data)){
                    cout << "Dados enviados com êxito.";
                }else{
                    cout << "erro ao enviar os dados ou validaçao do ack\n";
                }
            }else if(operation == "revive"){
                // so pra ter algo 
                peripheral.storeSession();
                peripheral.disconnect();
                //

                if (peripheral.canRevive()) { // Se há dados de sessão anterior
                    cout << "Tentando 0-Way connect (revive)...\n";
                    string reviveData;
                    if (cin.peek() == '\n') { // Verifica se há um newline pendente
                         cin.ignore();
                    }
                    cout << "Digite dados para enviar com o revive: ";
                    getline(cin, reviveData);

                    if (peripheral.zeroWayConnect(reviveData)) {
                        cout << "0-Way Connect (Revive) BEM-SUCEDIDO!\n";
                    } else {
                        cout << "Falha na tentativa de 0-Way Connect (Revive).\n";
                    }
                } else {
                    cout << "Nenhuma informação de sessão anterior para tentar revive.\n";
                }
            }else if(operation == "end"){
                peripheral.storeSession();
                peripheral.disconnect();
                break;
            }

        }
    }else{
        cout << "Falha na tentativa de conexão\n";
    }

    return 0;
}