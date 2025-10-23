// DEFINICIONES 
// -----------------------------------------------------------------------------------------------
#define F_CPU		16000000UL		//	Define etiqueta Fcpu = 16 MHz (p/calc. de retardos).

// INCLUSION DE ARCHIVOS
// -----------------------------------------------------------------------------------------------
#include <avr/io.h>					//	Contiene definiciones est�ndares (puertos, etc.)
#include <avr/interrupt.h>			//	Contiene macros para manejo de interrupciones.
#include <util/delay.h>				//	Contiene macros para generar retardos.
#include <stdio.h>

// MACROS
// -----------------------------------------------------------------------------------------------
#define sbi(p,b)		p |= _BV(b)                // sbi(p,b) setea el bit b de p.
#define cbi(p,b)		p &= ~(_BV(b))             // cbi(p,b) borra el bit b de p.
#define tbi(p,b)		p ^= _BV(b)                // tbi(p,b) togglea el bit b de p.
#define is_high(p,b)	(p & _BV(b)) == _BV(b)    // is_high(p,b) testea si el bit b de p es 1.
#define is_low(p,b)		(p & _BV(b)) == 0         // is_low(p,b) testea si el bit b de p es 0 (devuelve 1). Devuelve 1 si el bit b de p es cero

// DEFINICIONES
// -----------------------------------------------------------------------------------------------
#define	VPC1_2MS		65411				//	Valor de precarga para el Timer1 (modo Timer NORMAL free-running) interrup cada 2ms
#define	TOP				249								//  valor comparacion mi timer 0 para lograr 1ms.
#define DEBOUNCE_DELAY  10  // Retardo para eliminar el rebote 10 ms
#define TRANSISTOR_UNIT				  PD5
#define TRANSISTOR_TENS				  PD6
#define TRANSISTOR_HUNDREDS			PD7


// BOTONES
// -----------------------------------------------------------------------------------------------
#define P1_INCREMENT_PRESET   PC0      // pulsador P1, incrementa el valor preseteado
#define P2_ENTER_RESTART   PD2		            // pulsador P2, da inicio o fin al modo de conteo
#define DT_DRINK   PC2		      // pulsador DP, incrementa el numero de gaseosas


// VARIABLES GLOBALES
// -----------------------------------------------------------------------------------------------
volatile uint16_t presettable_value = 1;
volatile uint16_t cont_drinks = 0;


volatile uint8_t P1_flag = 0;
volatile uint8_t P2_flag = 0;
volatile uint8_t DT_flag = 0;

volatile uint8_t P2_debounce = 0; // este flag me permitira saber si tengo que calcular el antirrebote para P2
volatile uint8_t display_delay_unit = 0; // este flag me permitira saber si tengo que generar 10ms de encendido para los displays
volatile uint8_t display_delay_tens = 0; // este flag me permitira saber si tengo que generar 10ms de encendido para los displays
volatile uint8_t display_delay_hundreds = 0; // este flag me permitira saber si tengo que generar 10ms de encendido para los displays

volatile uint8_t timer_P2 = 0; //variable para contabilizar el tiempo de antirrebote de P2
volatile uint8_t timer_delay_displays = 0; //variable para contabilizar el tiempo de encendido de los displays



volatile uint8_t last_state_P1 = 0;
volatile uint8_t last_state_P2 = 0;
volatile uint8_t last_state_DT = 0;

volatile uint8_t unit = 0;
volatile uint8_t tens = 0;
volatile uint8_t hundreds = 0;


// FUNCIONES
// -----------------------------------------------------------------------------------------------
void initialization(){
  DDRB = 0b111111; //define todos los puertos B (son 6)como salidas
  PORTB = 0x00;

  DDRD = 0b11110000; //define a los 4 ultimos puertos D como salidas y el resto como entradas
  PORTD = 0x00;


  DDRC  = 0x00; //puerto C como entrada
	PORTC = 0x00; 

  // === Configuración de interrupción externa INT0 ===
  EICRA = 0x02;       // Configurar la interrupcion en el flanco de bajada.
  EIMSK = 0x01;				// Habilitar la interrupcion externa INT0.

  // Habilitar interrupciones globales
  sei();
};

void startupSequence(){
  PORTB = 0b00000000; //pongo todo en 0 primero por si acaso
  PORTD = 0b00000000; //pongo todo en 0 primero por si acaso


  PORTB = 0b00000001; //para el deco
  PORTD = 0b11110000; //para los transistores de los displays
  _delay_ms(500);
  PORTB = 0b00000001; //para el deco
  PORTD = 0b00000000; //para los transistores de los displays
  _delay_ms(500);
};

void timer0_config(){
  TCCR0A = 0x02;									// Modo CTC.
	TCCR0B = 0x03;									// Prescaler N = 64.
	TIMSK0 = 0x02;									// Habilita interrupcion por comparacion en comparador A.
	OCR0A = TOP;									 // Carga el valor de TOP con 249. T = (1+OCR0A)*N/16MHz = 1ms => OCR0A = TOPE = 249.
};

void timer1_config(){
  // === 1. Configurar Modo y Prescaler ===
    // WGM1[3:0] = 0000 -> Modo Normal (Overflow de 0x0000 a 0xFFFF)
    // COM1A[1:0] = 00 -> Pines de comparación desconectados
    TCCR1A = 0x00; 
    
    // CS1[2:0] = 100 -> Prescaler N=256. Inicia TC1.
    TCCR1B = 0x04; 
    
    // === 2. Habilitar Interrupción ===
    // TOIE1 = 1 -> Habilita la interrupción por desbordamiento del TC1.
    TIMSK1 = 0x01; 
    
    // === 3. Precargar el Contador  ===
    //  T = (2^(16)- VPC1_2MS)*N/16MHz = 2ms
    TCNT1 = VPC1_2MS;
};

void calculate_digits(uint8_t drinks_number){
  if(drinks_number<10){
    //solo LSB display
    unit = drinks_number;
    display_delay_unit = 1;

  } else if(drinks_number<100){
    // 2 displays, unidad y decena
    unit = drinks_number % 10;
    tens = drinks_number / 10;


  } else if (drinks_number<1000){
    // 3 displays, unidad, decena y centena
    unit = drinks_number % 10;
    tens = (drinks_number / 10) % 10;
    hundreds = (drinks_number / 100) % 10;
  }
};

void show_display(){
  PORTB = 0b00000001; //para el deco
  PORTD = 0b11110000; //para los transistores de los displays
  _delay_ms(250);
  PORTB = 0b00000001; //para el deco
  PORTD = 0b00000000; //para los transistores de los displays
  _delay_ms(250);


  /* if(drinks_number<10){
    //solo LSB display
    unit = drinks_number;
    display_delay_unit = 1;

  } else if(drinks_number<100){
    // 2 displays, unidad y decena
    unit = drinks_number % 10;
    tens = drinks_number / 10;


  } else if (drinks_number<1000){
    // 3 displays, unidad, decena y centena
    unit = drinks_number % 10;
    tens = (drinks_number / 10) % 10;
    hundreds = (drinks_number / 100) % 10;
  } */
};

ISR (TIMER0_COMPA_vect){								// RSI por comparacion del Timer0 con OCR0A (interrumpe cada 1 ms).
	if(P2_debounce){								// si hubo flanco bajo en P2
		timer_P2++;										// contador ++ para tiempo antirrebote
		if (timer_P2 == 10){ 								// si contador = 10 (10ms)
			if (is_low(PIND, P2_ENTER_RESTART)){ //si esta presionado P2...
        if(P2_flag){ // si estaba prendido y lo apago reseteo el valor preseteable
          presettable_value = 0;
        }
        P2_flag = !P2_flag;  // togleo flag de estado, ahora dejo en 0 ya que ya efectuo el antirrebote
      } 
			P2_debounce = 0;						// reset flag antirebote
			EIMSK = 0x01;								// vuelvo a habilitar interrupcion INT0
			timer_P2=0; // vuelvo a dejar en 0 el tiempo de antirrebote para P2
		}
	}
}

ISR (TIMER1_OVF_vect){//	RSI p/desbordam. del Timer1 (cuando llega a 0xFF, esto es c/2ms).
  TCNT1 = VPC1_2MS; //	Cada vez que interrumpe, precarga el contador del Timer1.

  if(display_delay_unit){				
		PORTB = unit; // prendo la unidad en el deco
		sbi(PORTD, TRANSISTOR_UNIT); //activo el transistor unidad
		timer_delay_displays++;										// contador ++ para tiempo antirrebote
    if (timer_delay_displays == 5){	// si contador = 5 (5 x 2ms = 10ms)
		  cbi(PORTD, TRANSISTOR_UNIT); // apago la unidad
      timer_delay_displays = 0;			//reseteo el tiempo de conteo	
      display_delay_unit=0; // Ahora al estar en 0 cuando evalue esta variable deberia apagar el display
    }
	}

  if(display_delay_tens){		
    PORTB = tens; // prendo la unidad en el deco
    sbi(PORTD, TRANSISTOR_TENS); //activo el transistor unidad				
    timer_delay_displays++;										// contador ++ para tiempo antirrebote
    if (timer_delay_displays == 5){	// si contador = 5 (5 x 2ms = 10ms)
      cbi(PORTD, TRANSISTOR_TENS); // apago la unidad
      timer_delay_displays = 0;				//	Como se alcanz� el tiempo programado, borra el contador del Timer 0.
      display_delay_tens=0; // Ahora al estar en 0 cuando evalue esta variable deberia apagar el display
    }
  }

  if(display_delay_hundreds){	
    PORTB = hundreds; // prendo la unidad en el deco
    sbi(PORTD, TRANSISTOR_HUNDREDS); //activo el transistor unidad									
    timer_delay_displays++;										// contador ++ para tiempo antirrebote
    if (timer_delay_displays == 5){	// si contador = 5 (5 x 2ms = 10ms)
      cbi(PORTD, TRANSISTOR_HUNDREDS); // apago la unidad
      timer_delay_displays = 0;				//	Como se alcanz� el tiempo programado, borra el contador del Timer 0.
      display_delay_hundreds=0; // Ahora al estar en 0 cuando evalue esta variable deberia apagar el display
    }
  }
    // SECUENCIA TERMINADA !!!!!!1
}				

// Interrupcion externa para P2
ISR(INT0_vect) {                       
  // Rutina de interrupción externa INT0 para P2
  //Si el boton 2 no esta presionado entro
  /* show_display(); */
  if(is_low(PIND,P2_ENTER_RESTART)){
    EIMSK = 0x00;
    P2_debounce = 1; // Ahora debo realiza el antirebote para P2
    timer_P2=0;
  }
}

int main(void){
  timer0_config();
  initialization();
  startupSequence();
  /* sei(); */

  //  Programacion valor preseteado
  // -----------------------------------------------------------------------------------------------

  // Boton P1 por pooling/encuesta en la etapa de configuracion

  /* if(is_low(PINC, P1_INCREMENT_PRESET)){ 
      _delay_ms(DEBOUNCE_DELAY);  // esperar para evitar el rebote
      if(is_low(PINC, P1_INCREMENT_PRESET) && last_state_P1 == 0){ //si sigue abajo despues del delay y su estado anterior fue bajo pongo en alto
        last_state_P1 = 1;
        presettable_value++;
      }
  }else { // restablecer el estado cuando el boton no se esta presionando
      last_state_P1 = 0;  
  }

  show_display(presettable_value);

 */

  // Bucle Principal - Conteo
  // -----------------------------------------------------------------------------------------------

  // Mientras el pulsador_2 no sea pulsado por un tiempo >=5 seg
  while(1){
    while(P2_flag){
      calculate_digits(0)
      show_display();
    };
  }
};
