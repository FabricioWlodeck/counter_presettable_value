// ------------------- DEFINICIONES -------------------
#define F_CPU		16000000UL		//	Define etiqueta Fcpu = 16 MHz (p/calc. de retardos).

// ------------------- INCLUSION DE ARCHIVOS -------------------
#include <avr/io.h>					//	Contiene definiciones est�ndares (puertos, etc.)
#include <avr/interrupt.h>			//	Contiene macros para manejo de interrupciones.
#include <util/delay.h>				//	Contiene macros para generar retardos.

// ------------------- FUNCIONES -------------------
void initialization(){
  DDRB = 0b001111; //define todos los puertos B (son 6)como salidas
  PORTB = 0x00;
  DDRD = 0b11110000; //define todos los puertos B (son 6)como salidas
  PORTD = 0x00;

  DDRC = 0x00;  // Declaro los puertos PDn como entradas
};

void startupSequence(){
  PORTB = 0b00000000; //pongo todo en 0 primero por si acaso
  PORTD = 0b00000000; //pongo todo en 0 primero por si acaso


  PORTB = 0b00000001;
  PORTD = 0b11110000;
  _delay_ms(500);
  PORTB = 0b00000001;
  PORTD = 0b00000000;
  _delay_ms(500);
};


int main(void){
initialization();
startupSequence();

// ------------------- Programacion valor preseteado -------------------


// ------------------- Bucle Principal - Conteo -------------------

// Mientras el pulsador_2 no sea pulsado por un tiempo >=5 seg
while(1){
  /* startupSequence(); */
};

};

/* 
cont -> va aumentando cada segundo ponele
segs = 0


aux0= cont (en el primer instante)
if(cont != aux0){
  segs++;
}

if(segs >= )
*/