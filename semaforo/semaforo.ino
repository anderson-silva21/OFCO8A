#include <SoftwareSerial.h>
#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.h"

const char *CMD_AT = "AT\r\n";
const char *CMD_AT_JOIN = "AT+JOIN\r\n";
const char *CMD_AT_VERJOIN = "AT+NJS=?\r\n";
const char *CMD_AT_SEND = "AT+SEND="; // enviar em texto/base64. A inserção de \r\n está no código...
const char *CMD_AT_SENDB = "AT+SENDB="; // enviar em hexa. A inserção de \r\n está no código...
const char *CMD_AT_RECV = "AT+RECV=?\r\n"; //receber em texto/base64
const char *CMD_AT_RECVB = "AT+RECVB=?\r\n"; //receber em hexa
const char *STATUS_AT_OK = "OK";
const char *STATUS_AT_JOINED = "JOINED";
const char *STATUS_AT_VER_JOINED = "1";
const char *STATUS_AT_VER_NOTJOINED = "0";
const char *STATUS_AT_ERROR = "AT_ERROR";

SoftwareSerial serialAT;

const byte tx=12; //D6 impresso na placa
const byte rx=13; //D7 impresso na placa
const int porta_enviar=2; //INDICAR AQUI QUAL FPORT DA MSG LORA A SER USADA...

int valor=0;
char comando_send_com_valor[61]=""; //máximo 51 bytes dados LoraWAN e a string deverá conter o comando (AT+SEND=) já tem 8 bytes...

int enviarcomandoAT(const char *cmd, int cmd_len){  

      char respostaraw[100]=""; //resposta do modem
      char resposta[100]=""; //resposta filtrada do modem
      int tam_resposta=0; //tamanho da resposta
      byte reenviar=1; //se precisa ou não reenviar comando (conforme status do modem)          
      
      while(reenviar==1){
        memset(respostaraw, 0, 100);
        memset(resposta, 0, 100);
        reenviar=0;
        serialAT.flush();
        
        if(serialAT.availableForWrite()){
            
            serialAT.write(cmd,cmd_len); //executar comando AT já montado...
            Serial.print("Comando enviado: ");
            Serial.println(cmd);
            if(strstr(cmd,CMD_AT_SEND)!=NULL || strstr(cmd,CMD_AT_SENDB)!=NULL){
              Serial.println("...aguardar RX windows 1 e 2 para RECV ou RECVB!");
              delay(2500); //tempo para RX Window_1 (1s) e Window_2 (2s) após executar comando SEND
            }
            else if(strcmp(cmd,CMD_AT_JOIN)==0)
                    delay(10000); //join +/- 5 segundos é necessário aguardar
                 else{
                    //Serial.println("Entrou no geral...");
                    delay(1000); //demais comandos, sendo um tempo aproximado das respostas ao comando AT (OK ou ERROR)
                 }
              
            tam_resposta = serialAT.read(respostaraw, 100); //Obter a resposta do modem. Há um Timeout nesse "read" de até 1000ms, documentado no SoftwareSerial.h para Stream::timeout() (https://www.arduino.cc/reference/en/language/functions/communication/stream/streamsettimeout/)
            
            if(tam_resposta>0){ //houve resposta do modem. Se não houve, será reenviado o comando.
                int new_i=0;
                int parte=0;
                for(int i=0;i<tam_resposta;i++){ //analisar as linhas de resposta ao comando enviado...
                   if(respostaraw[i]>0x20 && respostaraw[i]<0x80){ //filtra caracteres válidos de texto
                      resposta[new_i++]=respostaraw[i]; //montar a resposta do modem
                   }
                   if(respostaraw[i]=='\n'){ //chegou ao final da resposta (ou parte dela) para analisar...
                      resposta[new_i]='\0';                      

                      if(strcmp(resposta,STATUS_AT_ERROR)==0){
                        Serial.println("Entrou em AT_ERROR");
                        Serial.println("Reenviar comando...");
                        reenviar=1; //reenviar comando...
                        break; //interromper o laço for para reenviar comando...
                      }
    
                      if(strcmp(resposta,STATUS_AT_JOINED)==0){
                        Serial.println("Entrou em AT_JOINED");
                        //return 0;            
                      }
                      if(strcmp(cmd,CMD_AT_VERJOIN)==0 && strcmp(resposta,STATUS_AT_VER_NOTJOINED)==0){
                        Serial.println("Entrou em AT_VER_NOTJOINED");
                        return -1;
                      }          
                      if(strcmp(cmd,CMD_AT_VERJOIN)==0 && strcmp(resposta,STATUS_AT_VER_JOINED)==0){
                        Serial.println("Entrou em AT_VER_JOINED");
                        return 0;
                      }

                      if(strcmp(resposta,STATUS_AT_OK)==0){              
                        Serial.println("Entrou em STATUS_AT_OK");
                      }
    
                      if((strcmp(cmd,CMD_AT_RECV)==0 || strcmp(cmd,CMD_AT_RECVB)==0) && strcmp(resposta,STATUS_AT_OK)!=0 && strlen(resposta)>0){
                        Serial.println("Entrou em STATUS_AT_RECV ou STATUS_AT_RECVB:");
                        Serial.print("-TamResposta: ");
                        Serial.println(strlen(resposta));
                        Serial.print("-Resposta <");
                        Serial.print(parte++); //qual parte da resposta veio do modem (existe retorno do Modem com mais de uma linha ou itens de resposta)
                        Serial.print("> : ");
                        Serial.println(resposta); //mostrar para debug
                        //resposta conterá dados do Downlink do network server, caso necessitar...
                      }
                      else if (strcmp(resposta,STATUS_AT_OK)!=0){ //mostrar todas as respostas AT que não forem erro ou do RECV
                        Serial.print("Resposta <");
                        Serial.print(parte++); //qual parte da resposta veio do modem (existe retorno do Modem com mais de uma linha ou itens de resposta)
                        Serial.print("> : ");
                        Serial.println(resposta); //mostrar para debug
                      }                      
                      new_i=0;
                      memset(resposta, 0, 100); //analisar outra resposta de respostaraw...
                   }
                }                
            }//fim tam_resposta
            else{
              Serial.println("Sem resposta do modem! Tentando novamente...");
              reenviar=1;
            }
        }
      }
      Serial.println("Sucesso no comando!");
      return 0;
}

/* TIMER INTERRUPT */

#define USING_TIM_DIV1                false           // for shortest and most accurate timer
#define USING_TIM_DIV16               false           // for medium time and medium accurate timer
#define USING_TIM_DIV256              true            // for longest timer but least accurate. Default

// Init ESP8266 only and only Timer 1
ESP8266Timer ITimer;

void IRAM_ATTR TimerHandler()
{
  Serial.println("verde");
}

#define TIMER_INTERVAL_MS        1000
#define TIMER_FREQ_HZ        80000

void setup() {

  Serial.begin(9600);  //apensar para debug...  

    // Interval in microsecs
  if (ITimer.attachInterruptInterval(TIMER_INTERVAL_MS * 1000, TimerHandler)) {
    lastMillis = millis();
    Serial.print(F("Starting  ITimer OK, millis() = ")); Serial.println(lastMillis);
  } else {
    Serial.println(F("Can't set ITimer correctly. Select another freq. or interval"));
  }

  // Frequency in float Hz
  if (ITimer.attachInterrupt(TIMER_FREQ_HZ, TimerHandler)) {
    Serial.println("Starting  ITimer OK, millis() = " + String(millis()));
  } else {
    Serial.println("Can't set ITimer. Select another freq. or timer");
  }

  serialAT.begin(9600, SWSERIAL_8N1, rx, tx, false, 256); //adaptado de exemplos do diretorio do pacote SoftwareSerial na pasta esp8266 (arduino-esp8266)
 
  delay(1000);  
  Serial.print("Esperando Join: ");
  while(enviarcomandoAT(CMD_AT_VERJOIN,strlen(CMD_AT_VERJOIN))==-1){   
    delay(1000);
    Serial.print(".");
  }
  Serial.println(" Feito Join!!!");  
  valor = 0;
}



void loop() {        
 
  String tmpvalor = CMD_AT_SEND + String(porta_enviar) + ":" + String(valor) + "\r\n"; //comando montado de SEND conforme datasheet  
  //valor contém o dado a ser enviado ao network server. Atenção aqui para ser um tamanho pequeno, pois não pode ultrapassar 51 bytes de payload.
  //Sempre usar o mínimo possível codificado para enviar ao servidor!!!!
  tmpvalor.toCharArray(comando_send_com_valor,sizeof(comando_send_com_valor));
 
  Serial.print("Valor Medido: ");
  Serial.println(valor);
  enviarcomandoAT(comando_send_com_valor,strlen(comando_send_com_valor)); //enviar valor via Uplink...  
  enviarcomandoAT(CMD_AT_RECV,strlen(CMD_AT_RECV)); //existe algum Downlink a ser processado?
 
  valor++;
  
  delay(20000); //aguardar 20s (tempo mínimo entre transmissoes AU915 - se puder ser um intervalo maior deve-se adotar...) para a próxima transmissão
  //Ao invés de delay() o ideal é lidar com interrupção de tempo, watchdogs e recurso de millis(), pois o delay() interrompe o microcontrolador de fazer outra coisa (ex: realizar coleta de valores dos sensores...)
}