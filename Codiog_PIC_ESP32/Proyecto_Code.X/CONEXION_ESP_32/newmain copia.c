/*
 * File:   newmain.c
 * Author: hectoralv
 *
 * Programa para comunicaci�n UART entre PIC18F4550 y ESP32
 * Control de un LED en RB0 desde el monitor serial del ESP32
 */

 #include <xc.h>
 #include <stdio.h>
 
 // Configuraci�n de fusibles
 #pragma config FOSC = INTOSCIO_EC // Oscilador interno
 #pragma config WDT = OFF         // Watchdog Timer apagado
 #pragma config PWRT = ON         // Power-up Timer encendido
 #pragma config MCLRE = ON        // MCLR habilitado
 #pragma config LVP = OFF         // Low Voltage Programming deshabilitado
 #pragma config XINST = OFF       // Extended Instruction Set deshabilitado
 
 // Definici�n de frecuencia para delays
 #define _XTAL_FREQ 8000000  // 8MHz
 
 // Definici�n de comandos para control del LED
 #define CMD_LED_ON  '1'     // Comando para encender LED
 #define CMD_LED_OFF '0'     // Comando para apagar LED
 
 // Definici�n de pines
 #define LED_PIN PORTBbits.RB0  // LED conectado a RB0
 
 /**
  * Inicializa el m�dulo UART
  */
 void UART_Init(void) {
     // Configurar pines
     TRISC6 = 0;  // RC6/TX como salida
     TRISC7 = 1;  // RC7/RX como entrada
     
     // Configurar UART
     SPBRG = 51;              // 9600 bps @ 8MHz (BRGH=0)
     TXSTAbits.BRGH = 1;      // Baja velocidad
     TXSTAbits.SYNC = 0;      // Modo as�ncrono
     TXSTAbits.TXEN = 1;      // Habilitar transmisi�n
     RCSTAbits.SPEN = 1;      // Habilitar puerto serial
     RCSTAbits.CREN = 1;      // Habilitar recepci�n continua
 }
 
 /**
  * Env�a un byte por UART
  * @param data Byte a enviar
  */
 void UART_Write(char data) {
     while(!TXSTAbits.TRMT);  // Esperar que el buffer est� vac�o
     TXREG = data;            // Enviar dato
 }
 
 /**
  * Env�a una cadena de texto por UART
  * @param text Puntero a la cadena de texto
  */
 void UART_Write_Text(char *text) {
     while(*text) {
         UART_Write(*text++);
     }
 }
 
 /**
  * Lee un byte recibido por UART
  * @return Byte recibido
  */
 char UART_Read(void) {
     while(!PIR1bits.RCIF);  // Esperar dato recibido
     return RCREG;           // Retornar dato
 }
 
 /**
  * Verifica si hay datos disponibles en el buffer de recepci�n
  * @return 1 si hay datos, 0 si no hay datos
  */
 char UART_Data_Ready(void) {
     return PIR1bits.RCIF;
 }
 
 /**
  * Funci�n principal
  */
 void main(void) {
     // Configurar oscilador interno a 8MHz
     OSCCON = 0x70;
     
     // Configurar puerto B
     TRISB = 0x00;      // Todo el puerto B como salida
     PORTB = 0x00;      // Inicializar en 0
     
     // Inicializar UART
     UART_Init();
     
     // Mensaje de inicio
     UART_Write_Text("PIC18F4550 iniciado\r\n");
     UART_Write_Text("Comandos: 1=LED ON, 0=LED OFF\r\n");
     
     char received_char;
     char led_mode = 'A';  // 'A' para modo autom�tico, '1' para encendido, '0' para apagado
     unsigned int counter = 0;
     
     while(1) {
         // Verificar si hay datos recibidos
         if(UART_Data_Ready()) {
             received_char = UART_Read();
             
             // Procesar comando recibido
             if(received_char == CMD_LED_ON) {
                 LED_PIN = 1;  // Encender LED
                 led_mode = '1';  // Modo encendido permanente
                 UART_Write_Text("LED encendido\r\n");
             }
             else if(received_char == CMD_LED_OFF) {
                 LED_PIN = 0;  // Apagar LED
                 led_mode = '0';  // Modo apagado permanente
                 UART_Write_Text("LED apagado\r\n");
             }
             else if(received_char == 'A' || received_char == 'a') {
                 led_mode = 'A';  // Volver al modo autom�tico
                 UART_Write_Text("Modo parpadeo automatico\r\n");
             }
             else {
                 // Comando no reconocido
                 UART_Write_Text("Comando no reconocido. Use 1=ON, 0=OFF, A=Auto\r\n");
             }
         }
         
         // Control del LED seg�n el modo
         if(led_mode == 'A') {
             // Modo autom�tico: parpadeo cada segundo
             if(counter == 0) {
                 LED_PIN = 1;  // Encender LED
             } 
             else if(counter == 100) {
                 LED_PIN = 0;  // Apagar LED
             }
             
             counter++;
             if(counter >= 200) counter = 0;  // Reset contador cada 2 segundos
             
             __delay_ms(10);  // Peque�o delay para el contador
         }
         // En los modos encendido/apagado permanente, no hacemos nada con el LED
         // ya que ya se estableci� su estado al recibir el comando
         else {
             __delay_ms(10);  // Peque�o delay para no saturar el CPU
         }
     }
     
     return;
 }