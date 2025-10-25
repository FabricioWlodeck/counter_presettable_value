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
#define ALARM_TRANSISTOR   PD4		      // pulsador DP, incrementa el numero de gaseosas


// VARIABLES GLOBALES
// -----------------------------------------------------------------------------------------------
volatile uint16_t presettable_value = 0;
volatile uint16_t DP_value = 0;
volatile uint16_t cont_drinks = 0;


volatile uint8_t P1_flag = 0;
volatile uint8_t P2_flag = 0;
volatile uint8_t DT_flag = 0;

volatile uint8_t P2_debounce = 0; // este flag me permitira saber si tengo que calcular el antirrebote para P2
volatile uint8_t P2_off_flag = 0; // flag para saber si apagar a los 5seg
volatile uint8_t DP_permition = 1; // flag que me permite controlar cuando se puede aumentar las gaseosas con DP
volatile uint16_t number_toShow = 0; // flag que me permite controlar cuando se puede aumentar las gaseosas con DP



// esto por ahora no lo ocupo
volatile uint8_t display_delay_unit = 0; // este flag me permitira saber si tengo que generar 10ms de encendido para los displays
volatile uint8_t display_delay_tens = 0; // este flag me permitira saber si tengo que generar 10ms de encendido para los displays
volatile uint8_t display_delay_hundreds = 0; // este flag me permitira saber si tengo que generar 10ms de encendido para los displays

volatile uint8_t show_displays = 0; 
volatile uint8_t show_display_unit = 0; 
volatile uint8_t show_display_tens = 0; 
volatile uint8_t show_display_hundreds = 0;

volatile uint8_t mux_state = 0;
volatile uint8_t mux_counter = 0;


volatile uint16_t timer_P2_debounce = 0; //variable para contabilizar el tiempo de antirrebote de P2
volatile uint16_t timer_P2_off = 0; //variable para contabilizar el tiempo para apagar con P2


volatile uint8_t timer_delay_displays = 0; //variable para contabilizar el tiempo de encendido de los displays
volatile uint8_t delay_display_flag = 0; // si tengo que contabilizar delay o no


volatile uint8_t last_state_P1 = 0;
volatile uint8_t last_state_P2 = 0;
volatile uint8_t last_state_DT = 0;

volatile uint8_t unit = 0;
volatile uint8_t tens = 0;
volatile uint8_t hundreds = 0;

volatile uint16_t aux_numer = 536; // aca estoy agregando de forma auxiliar el valor a multiplexar

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


  PORTB = 0b00001000; //pruebo todo los segmentos escribiendo un 8
  PORTD = 0b11110000; //para los transistores de los displays
  _delay_ms(500);
  PORTB = 0b00001000; //pruebo todo los segmentos escribiendo un 8
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

void calculate_digits(uint16_t drinks_number){
  number_toShow = drinks_number;
  if(drinks_number<10){
    //solo LSB display
    unit = drinks_number;

    //probar esto:
    tens = 0;
    hundreds = 0;
    /* unit = 6; */

  } else{
    if(drinks_number<100){
      // 2 displays, unidad y decena
      unit = drinks_number % 10;
      tens = drinks_number / 10;
      
      /* unit = 6;
      tens = 3; */
    }else{
      if (drinks_number<1000){
        // 3 displays, unidad, decena y centena
        unit = drinks_number % 10;
        tens = (drinks_number / 10) % 10;
        hundreds = (drinks_number / 100) % 10;

        /* unit = 6;
        tens = 3;
        hundreds = 5; */

      }else{
        unit = 0;
        tens = 0;
        hundreds = 0;
      }
    }
  }
};

void show_display(uint8_t drinks_number){
  // codigo auxiliar
  // -----------------------------------------------------------------------------------------------
  /* PORTB = 0b00000001; //para el deco
  PORTD = 0b11110000; //para los transistores de los displays
  _delay_ms(250);
  PORTB = 0b00000001; //para el deco
  PORTD = 0b00000000; //para los transistores de los displays
  _delay_ms(250); */

  // CODIGO PERMANENTE
  // -----------------------------------------------------------------------------------------------

  // Apagamos TODOS para ser seguros y evitar errores.
  /* PORTD &= (~(0 << TRANSISTOR_UNIT) | ~(0 << TRANSISTOR_TENS) | ~(0 << TRANSISTOR_HUNDREDS)); */

  if(drinks_number<10){
    // prendo solo display de unidad
    PORTB = unit; 
    sbi(PORTD, TRANSISTOR_UNIT);
    /* delay_display_flag = 1; */  // veremos si puedo ocupar
    
    if(is_high(PORTD, TRANSISTOR_UNIT) && !show_displays){
      cbi(PORTD, TRANSISTOR_UNIT);
      show_displays = 1;
    }

  } else{
    if(drinks_number<100){
      // 2 displays, unidad y decena
      // prendo display unidad y decena

      //prendo y apagado unidad
      PORTB = unit; 
      sbi(PORTD, TRANSISTOR_UNIT);
      delay_display_flag = 1;
      if(is_high(PORTD, TRANSISTOR_UNIT) && !show_displays){
        cbi(PORTD, TRANSISTOR_UNIT);
        show_displays = 1;
      }
      
      //prendo y apagado decena
      PORTB = tens; 
      sbi(PORTD, TRANSISTOR_TENS);
      delay_display_flag = 1;
      if(is_high(PORTD, TRANSISTOR_TENS) && !show_displays){
        cbi(PORTD, TRANSISTOR_TENS);
        show_displays = 1;
      }
    } else{
      if (drinks_number<1000){
        //Prendo 3 displays, unidad, decena y centena

        //prendo y apagado unidad
        PORTB = unit; 
        sbi(PORTD, TRANSISTOR_UNIT);
        delay_display_flag = 1;
        if(is_high(PORTD, TRANSISTOR_UNIT) && !show_displays){
          cbi(PORTD, TRANSISTOR_UNIT);
          show_displays = 1;
        }
        
        //prendo y apagado decena
        PORTB = tens; 
        sbi(PORTD, TRANSISTOR_TENS);
        delay_display_flag = 1;
        if(is_high(PORTD, TRANSISTOR_TENS) && !show_displays){
          cbi(PORTD, TRANSISTOR_TENS);
          show_displays = 1;
        }

        //prendo y apagado centena
        PORTB = hundreds; 
        sbi(PORTD, TRANSISTOR_HUNDREDS);
        delay_display_flag = 1;
        if(is_high(PORTD, TRANSISTOR_HUNDREDS) && !show_displays){
          cbi(PORTD, TRANSISTOR_HUNDREDS);
          show_displays = 1;
        }
      }else{
        PORTD &= (~(0 << TRANSISTOR_UNIT) | ~(0 << TRANSISTOR_TENS) | ~(0 << TRANSISTOR_HUNDREDS)); 
      }
    }
  }


  if(drinks_number<10){
    // solo para display unidad

    // prendo display de unidad
    PORTB = unit; 
    sbi(PORTD, TRANSISTOR_UNIT);
    delay_display_flag = 1;  // veremos si puedo ocupar
    
    if(is_high(PORTD, TRANSISTOR_UNIT) && !show_displays){
      cbi(PORTD, TRANSISTOR_UNIT);
      show_displays = 1;
    }

  } else{
    if(drinks_number<100){
    // Para display unidad y decena

    // prendo display de unidad
    PORTB = unit; 
    sbi(PORTD, TRANSISTOR_UNIT);
    delay_display_flag = 1;  
    
    if(is_high(PORTD, TRANSISTOR_UNIT) && !show_displays){
      cbi(PORTD, TRANSISTOR_UNIT);
      show_displays = 1;
    }

    PORTB = unit; 
    sbi(PORTD, TRANSISTOR_UNIT);
    delay_display_flag = 1;
    if(is_high(PORTD, TRANSISTOR_UNIT) && !show_displays){
      cbi(PORTD, TRANSISTOR_UNIT);
      show_displays = 1;
    }
    
    //prendo y apagado decena
    PORTB = tens; 
    sbi(PORTD, TRANSISTOR_TENS);
    delay_display_flag = 1;
    if(is_high(PORTD, TRANSISTOR_TENS) && !show_displays){
      cbi(PORTD, TRANSISTOR_TENS);
      show_displays = 1;
    }
      
    }else{
      if (drinks_number<1000){
      // Para display unidad, decena y centana!!
        

      }else{
      // Para en caso de que el valor sea mayor a 1000 y no pueda mostrarlo

      }
    }
  }
};

//Con este TIMER0 CTC manejo P2
ISR (TIMER0_COMPA_vect){								// RSI por comparacion del Timer0 con OCR0A (interrumpe cada 1 ms).
	if(P2_debounce){								// si hubo flanco bajo en P2
		timer_P2_debounce++;										// contador ++ para tiempo antirrebote
		if (timer_P2_debounce == 10){ 								// si contador = 10 (10ms)
			if (is_low(PIND, P2_ENTER_RESTART)){ //si esta presionado P2 despues de 10ms...
        /* P2_flag = !P2_flag; */  // togleo flag de estado, ahora dejo en 0 ya que ya efectuo el antirrebote
        P2_flag = 1; //lo prendo directamente
      } 
			P2_debounce = 0;						// reset flag antirebote
			EIMSK |= (1 << INT0);								// vuelvo a habilitar "SOLO!!" interrupcion INT0
			timer_P2_debounce=0; // vuelvo a dejar en 0 el tiempo de antirrebote para P2
		}
	}
  if(P2_off_flag){
    timer_P2_off++;
    if (timer_P2_off == 5000){ 								// si contador = 10 (10ms)
			if (is_low(PIND, P2_ENTER_RESTART)){ //si esta presionado P2 despues de 10ms...
        if(P2_flag){ // si estaba prendido y lo apago reseteo el valor preseteable
          P2_flag = 0;

          DP_value = 0;
          presettable_value = 0;
          unit=0;
          tens=0;
          hundreds=0;
        }
        
      }
			timer_P2_off = 0;						// reset flag de apagado de P2
      P2_off_flag = 0; //lo prendo directamente

		}
  }
}

//Con este TIMER1 Free-running (NORMAL) manejo multiplexacion de displays
ISR (TIMER1_OVF_vect){//	RSI p/desbordam. del Timer1 (cuando llega a 0xFF, esto es c/2ms).
  /* TCNT1 = VPC1_2MS; //	Cada vez que interrumpe, precarga el contador del Timer1.
  // PASO 1: Controlar el tiempo de encendido del display actual
  if(is_high(PORTD, TRANSISTOR_UNIT) || is_high(PORTD, TRANSISTOR_TENS) || is_high(PORTD, TRANSISTOR_HUNDREDS)){
    timer_delay_displays++; 
    // Solo un display está activo en este momento. Cuando se alcanza el tiempo:
    if (timer_delay_displays >= 10 ) { // esto me dara un tiempo de 20ms
      show_displays = 0;
      timer_delay_displays = 0;
      delay_display_flag = 0; // significa que si no tengo que mostrar mas nada ni siquiera se moleste de ir contando cada 2ms
    }
  } */

  TCNT1 = VPC1_2MS; 
  uint16_t drinks_number =  number_toShow;

  mux_counter++;
  if (mux_counter >= 2) { //40ms
    
      PORTD &= ~(_BV(TRANSISTOR_UNIT) | _BV(TRANSISTOR_TENS) | _BV(TRANSISTOR_HUNDREDS));

      // b) ROTAR ESTADO: 0 -> 1 -> 2 -> 0 ...
      mux_state++;
      if (mux_state > 2) {
          mux_state = 0; // Vuelve a Unidad
      }
      
      //Reiniciar el contador 
      mux_counter = 0; 
  }

  // Esto mantiene el display encendido hasta el próximo cambio de estado.
  
  switch (mux_state) {
      case 0: // UNIDAD (LSB)
          PORTB = unit; 
          sbi(PORTD, TRANSISTOR_UNIT); // Activa el transistor unidad
          cbi(PORTD, TRANSISTOR_TENS); // desactiva el decena
          cbi(PORTD, TRANSISTOR_HUNDREDS); // desactiva el centena
          break;
      
      case 1: // DECENA
          if (drinks_number < 10 ) {
            cbi(PORTD, TRANSISTOR_UNIT); 
            cbi(PORTD, TRANSISTOR_TENS); 
            cbi(PORTD, TRANSISTOR_HUNDREDS); 
          }else{ // predo solo si hace falta o sea si el numero es mayor o igual a 10
            PORTB = tens;
            cbi(PORTD, TRANSISTOR_UNIT); // desactiva el transistor unidad
            sbi(PORTD, TRANSISTOR_TENS); // Activa el decena
            cbi(PORTD, TRANSISTOR_HUNDREDS); // desactiva el centena
          }
          break;
      
      case 2: // CENTENA
          if (drinks_number < 100) { 
              cbi(PORTD, TRANSISTOR_UNIT); 
              cbi(PORTD, TRANSISTOR_TENS); 
              cbi(PORTD, TRANSISTOR_HUNDREDS); 
          }else{// predo solo si hace falta o sea si el numero es mayor o igual a 100
            PORTB = hundreds; 
            cbi(PORTD, TRANSISTOR_UNIT); // desactiva el transistor unidad
            cbi(PORTD, TRANSISTOR_TENS); // desactiva el decena
            sbi(PORTD, TRANSISTOR_HUNDREDS); // Activa el centena
            
          }
          break;
  }

}				


// Interrupcion externa para P2
ISR(INT0_vect) {                       
  // Rutina de interrupción externa INT0 para P2
  //Si el boton 2 no esta presionado entro
  /* show_display(); */
  if(is_low(PIND,P2_ENTER_RESTART)){
    EIMSK &= ~(1 << INT0); //dehabilito SOOOOLO!!!! la interrupcion INT0
    P2_debounce = 1; // Ahora debo realiza el antirebote para P2
    P2_off_flag = 1;
    timer_P2_debounce=0;
    timer_P2_off = 0;

  }
}

int main(void){
  // INICIALIZACION
  //-----------------------------------------------------------------------------------------------
  timer0_config();
  timer1_config();
  initialization();
  startupSequence();


  //BUCLE PRINCIPAL
  //-----------------------------------------------------------------------------------------------
  while(1){
    // Bucle Principal - Conteo Gaseosas
    //  Programacion valor preseteado
    // -----------------------------------------------------------------------------------------------
    if(P2_flag == 0){
      // ===========================================
      //  MODO CONFIGURACIÓN (Muestra presettable_value)
      // ===========================================
      // Boton P1 por pooling/encuesta en la etapa de configuracion
      calculate_digits(presettable_value);
      if(is_low(PINC, P1_INCREMENT_PRESET)){ 
          _delay_ms(DEBOUNCE_DELAY);  // esperar para evitar el rebote
          if(is_low(PINC, P1_INCREMENT_PRESET) && last_state_P1 == 0){ //si sigue abajo despues del delay y su estado anterior fue bajo pongo en alto
            last_state_P1 = 1;
            presettable_value++;
            

            // si llega a 1000 que reinicie en 0
            if(presettable_value>=1000){
              presettable_value= 0;
            }
          }
          calculate_digits(presettable_value);
      }else { // restablecer el estado cuando el boton no se esta presionando
          last_state_P1 = 0;  
      }
    } else{
      // ===========================================
      // B. MODO CONTEO (Muestra DP_value)
      // ===========================================
      calculate_digits(DP_value);
      if(is_low(PINC, DT_DRINK) ){ 
          _delay_ms(DEBOUNCE_DELAY);  // esperar para evitar el rebote
          if(is_low(PINC, DT_DRINK) && last_state_DT == 0){ //si sigue abajo despues del delay y su estado anterior fue bajo pongo en alto
            last_state_DT = 1;
            DP_value++;
            calculate_digits(presettable_value);

            // si llega a 1000 que reinicie en 0
            if(DP_value>=1000){
              DP_value= 0;
            }
          }
      }else { // restablecer el estado cuando el boton no se esta presionando
          last_state_DT = 0;  
      }

      if(DP_value == presettable_value && presettable_value != 0){
        sbi(PORTD, ALARM_TRANSISTOR);
        DP_permition = 0;
        _delay_ms(5000);
        cbi(PORTD, ALARM_TRANSISTOR);
        DP_permition= 1;
        DP_value = 0;
      }
    }
  }
};