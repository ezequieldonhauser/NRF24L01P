//----------------------------------------------------------------------------//
// NRF24L01P.C                                                                //
// Ezequiel Donhauser                                                         //
//                                                                            //
// esse arquivo contem as configuraçoes do modulo NORDIC NRF24L01+            //
// funçoes: recebe_dados(); | envia_dados(); | config_nrf24()                 //
// Configuraçoes: CANAL(1-126) | BUFFER(1-32) | ENDTX(1-255) | ENDRX(1-255)   //
//                                                                            //
// OBS: os canais devem ser o mesmo nos modulos que se comunicarão, o buffer  //
// tambem deve ser o mesmo validos de 1a32 Bytes, ENDTX e ENDRX tambem devem  //
// ser os mesmos(endereço), a potencia de transmissão foi configurada para    //
// 0dbm(maxima), taxa de transferencia em 250Kbps, autoACK ativado, e nenhuma //
// retransmissão de pacotes, para outras configuraçoes consulte o datasheet.  //
//----------------------------------------------------------------------------//

//configure aqui os parametros desejados
#define CANAL    107 // 1 a 126
#define BUFFER   10  // 1 a 32
#define ENDTX    75  // 1 a 255
#define ENDRX    75  // 1 a 255

//configuraçoes dos PINOS
#define     IRQ         PIN_B0     //PIC(RB0)  <->  MODULO(IRQ)
#define     CSN         PIN_C1     //PIC(RC1)  <->  MODULO(CSN)
#define     CE          PIN_C2     //PIC(RC2)  <->  MODULO(CE)
#define     IRQ_TR      TRISB,0
#define     CSN_TR      TRISC,1
#define     CE_TR       TRISC,2
#BYTE       TRISA       =0x85
#BYTE       TRISB       =0x86
#BYTE       TRISC       =0x87
#BYTE       INTCON      =0x00B

//variaveis globais
static int16       espera_ack;
static int1        ACK;
static int8        RECEBE[BUFFER];
static int8        ENVIA [BUFFER];

//função que configura modulo
void config_nrf24(){

   bit_clear(CSN_TR);
   bit_set(IRQ_TR);
   bit_clear(CE_TR);

   //configura modulo SPI
   setup_spi(SPI_MASTER|SPI_L_TO_H|SPI_XMIT_L_TO_H
   |SPI_CLK_DIV_4|SPI_SAMPLE_AT_END);
   
   output_low(CE);

   //RX_ADDR_P0 - configura endereço de recepção PIPE0
   output_low(CSN);
   spi_write(0x2A);
   spi_write(ENDRX);
   spi_write(0xC2);
   spi_write(0xC2);
   spi_write(0xC2);
   spi_write(0xC2);
   output_high(CSN);
   
   //TX_ADDR - configura endereço de transmissão
   output_low(CSN);
   spi_write(0x30);
   spi_write(ENDTX);
   spi_write(0xC2);
   spi_write(0xC2);
   spi_write(0xC2);
   spi_write(0xC2);
   output_high(CSN);

   //EN_AA - habilita autoACK no PIPE0
   output_low(CSN);
   spi_write(0x21);
   spi_write(0x01);
   output_high(CSN);

   //EN_RXADDR - ativa o PIPE0
   output_low(CSN);
   spi_write(0x22);
   spi_write(0x01);
   output_high(CSN);

   //SETUP_AW - define o endereço com tamanho de 5 Bytes
   output_low(CSN);
   spi_write(0x23);
   spi_write(0x03);
   output_high(CSN);

   //SETUP_RETR - configura para nao retransmitir pacotes
   output_low(CSN);
   spi_write(0x24);
   spi_write(0x00);
   output_high(CSN);

   //RF_CH - define o canal do modulo (TX e RX devem ser iguais)
   output_low(CSN);
   spi_write(0x05);
   spi_write(CANAL);
   output_high(CSN);

   //RF_SETUP - ativa LNA, taxa em 250K, e maxima potencia 0dbm
   output_low(CSN);
   spi_write(0x26);
   spi_write(0b00100110);
   output_high(CSN);

   //STATUS - reseta o resgistrador STATUS
   output_low(CSN);
   spi_write(0x27);
   spi_write(0x70);
   output_high(CSN);

   //RX_PW_P0 - tamanho do buffer PIPE0
   output_low(CSN);
   spi_write(0x31);
   spi_write(BUFFER);
   output_high(CSN);

   //CONFIG - coloca em modo de recepção, e define CRC de 2 Bytes
   output_low(CSN);
   spi_write(0x20);
   spi_write(0x0F);
   output_high(CSN);
   
   //tempo para sair do modo standby entrar em modo de recepçao
   delay_ms(2);
   output_high(CE);
   delay_us(150);
   
   //configura interrupção no pino RB0
   disable_interrupts(global);
   enable_interrupts(int_ext);
   ext_int_edge( H_TO_L );
   bit_set(IRQ_TR);
   enable_interrupts(global);
}

//função que transmite os dados
int1 envia_dados(){
   
   int8 i;
   int8 status;

   output_low(CE);

   //STATUS - reseta registrador STATUS
   output_low(CSN);
   spi_write(0x27);
   spi_write(0x70);
   output_high(CSN);

   // W_TX_PAYLOAD - envia os dados para o buffer FIFO TX 
   output_low(CSN);
   spi_write(0xA0);
   for (i=0;i<BUFFER;i++)spi_write(ENVIA[i]);
   output_high(CSN);

   //CONFIG - ativa modo de transmissão
   output_low(CSN);
   spi_write(0x20);
   spi_write(0x0E);
   output_high(CSN);

   //pulso para transmitir os dados
   output_high(CE);
   delay_us(15);
   output_low(CE);

   espera_ack=0;

   while(input(IRQ)==1){
      espera_ack++;
      //espera 5ms, pela recepçao do pacote ACK
      if(espera_ack==400){
      break;
      }
   }

   //STATUS - leitura do registrador
   output_low(CSN);
   spi_write(0x07);
   status=spi_read(0);
   output_high(CSN);
   
   //STATUS - limpa registrador
   output_low(CSN);
   spi_write(0x27);
   spi_write(0x70);
   output_high(CSN);

   //TX_FLUSH - limpa o buffer FIFO TX
   output_low(CSN);
   spi_write(0xE1);
   output_high(CSN);

   //CONFIG - configura para modo de recepção
   output_low(CSN);
   spi_write(0x20);
   spi_write(0x0F);
   output_high(CSN);

   output_high(CE);

   delay_us(150);

   //senão recebeu ACK em 5ms retorna 0
   if(espera_ack==500){
   clear_interrupt(int_ext);
   enable_interrupts(GLOBAL);
   return(0);
   }
   //se recebeu ACK retorna 1
   else{
   enable_interrupts(GLOBAL);
   clear_interrupt(int_ext);
   return(1);
   }
}

//função que recebe os dados e joga num vetor
int1 recebe_dados(){

   int8 i;
   int8 status;
   
   //desabilita interrupção
   disable_interrupts(GLOBAL);
   
   //STATUS - leitura do registrador
   output_low(CSN);
   spi_write(0x07);
   status=spi_read(0);
   output_high(CSN);
   
   //STATUS - limpa registrador
   output_low(CSN);
   spi_write(0x27);
   spi_write(0x70);
   output_high(CSN);
   
   //verifica o bit de recepção de dados
   if(bit_test(status,6)==0){
   return(0);
   }
   
   //R_RX_PAYLOAD - recebe os dados do buffer FIFO RX
   output_low(CSN);
   spi_write(0x61);
   for(i=0;i<BUFFER;i++)RECEBE[i]=spi_read(0);
   output_high(CSN);
   
   //habilita interrupção
   clear_interrupt(int_ext);
   enable_interrupts(GLOBAL);
   
   return(1);
}

//Interrupção pino RB0
#int_ext 
void interrupcao(){ 
   
   //função recebe dados
   ACK=recebe_dados();
} 
