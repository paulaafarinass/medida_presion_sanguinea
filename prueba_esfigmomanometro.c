#include <xc.h>

#pragma config LVP=OFF
#pragma config WDTEN=OFF
#pragma config FOSC=INTIO67
#pragma config PBADEN=OFF
#pragma config CCP2MX = PORTC 


#define EV LATDbits.LATD4 
#define P_MAX 800         
#define P_MIN 140         
#define UMBRAL_PULSO 10   


const unsigned fin_on = 3;
const unsigned fin_off = 80; 
unsigned const muestreo = 0xC568; 
unsigned int cuenta = 0;

char mensaje_T[7] = {'*','P','0','0','0','0','*'};
char mensaje_R[7];
char *punt_T = &mensaje_T[1];
char *punt_R = &mensaje_R[0];
char primero  = 0;
unsigned int aux1      = 0;
unsigned int resultado = 0;


volatile unsigned int medidas = 0;       
volatile unsigned int presion_base = 0;  
volatile unsigned char estado = 0; 
unsigned char turno_envio = 0;


volatile unsigned int val_presion = 0;
volatile unsigned int sistolica = 0;
volatile unsigned int diastolica = 0;
volatile unsigned int muestras_sin_pulso = 0;
volatile unsigned int timer_estabilizacion = 0;
volatile unsigned int timer_estado_5 = 0;

unsigned int cad_a_mmhg(unsigned int valor_cad) {
    float mmhg;
    mmhg = (valor_cad * 0.276) - 23.58;

    if (mmhg < 0) {
        return 0;
    }

    return (unsigned int)mmhg;
}


void __interrupt(high_priority) interrupciones(void) {
    int i = 0;

   
    if ((INTCONbits.INT0IF == 1) && (INTCONbits.INT0IE == 1)) {
        INTCONbits.INT0IE = 0;   
        INTCONbits.INT0IF = 0;        
        TMR3H = 0xd8; 
        TMR3L = 0xf0;
        PIE2bits.TMR3IE  = 1;
        T3CONbits.TMR3ON = 1;    
    }

   
    if ((PIR2bits.TMR3IF == 1) && (PIE2bits.TMR3IE == 1)) {
        PIR2bits.TMR3IF  = 0;
        PIE2bits.TMR3IE  = 0;
        T3CONbits.TMR3ON = 0;
        
        if (PORTBbits.RB0 == 0) { 
            if (estado == 0) { 
                estado = 1; 
                sistolica = 0; 
                diastolica = 0; 
            } else { 
                estado = 0; 
            }
        }
        
        INTCONbits.INT0IE = 1; 
    }

    if ((INTCONbits.TMR0IE == 1) && (INTCONbits.TMR0IF == 1)) {
        TMR0H = (muestreo >> 8); 
        TMR0L = muestreo;
        INTCONbits.TMR0IF = 0;

        if (estado == 3) {
            cuenta++;
            if (cuenta < fin_on) { 
                EV = 1; 
            } else { 
                EV = 0; 
                if (cuenta >= fin_off) {
                    cuenta = 0; 
                }
            }
        } else if ((estado == 4) || (estado == 0)) { 
            EV = 1; 
        } else { 
            EV = 0; 
        }

        if (turno_envio == 0) {
            ADCON0bits.CHS = 0b0000;
        } else {
            ADCON0bits.CHS = 0b0001;
        }
        
        ADCON0bits.GO_nDONE = 1;
    }

   
    if ((PIR1bits.ADIF == 1) && (PIE1bits.ADIE == 1)) {
        PIR1bits.ADIF = 0;
        resultado = (256 * ADRESH) + ADRESL;

        if (turno_envio == 0) {
            if (estado == 1) { 
                CCPR2L = (resultado >> 2); 
                CCP2CONbits.DC2B = resultado; 
            }
        } 
      
        else {
            val_presion = resultado;
            
            switch (estado) {
                case 0: 
                    CCPR2L = 0; 
                    CCP2CONbits.DC2B = 0; 
                    break;

                case 1: 
                    if (val_presion >= P_MAX) { 
                        CCPR2L = 0; 
                        CCP2CONbits.DC2B = 0; 
                        estado = 2; 
                        timer_estabilizacion = 0; 
                    } 
                    break;

                case 2: 
                    timer_estabilizacion++;
                    if (timer_estabilizacion > 66) { 
                        estado = 3; 
                        presion_base = 0; 
                        medidas = 0; 
                        muestras_sin_pulso = 0; 
                    } 
                    break;

                case 3: 
                    if (val_presion < P_MIN) { 
                        estado = 4; 
                        break; 
                    }
                    
                    if (cuenta > 10) {
                        medidas++;
                        
                        if (presion_base == 0) {
                            presion_base = val_presion;
                        }
                        
                        if (val_presion <= presion_base) {
                            presion_base = val_presion;
                        }

                        
                        if ((medidas > 200) && (EV == 0)) {
                            if (val_presion > (presion_base + UMBRAL_PULSO)) {
                                if (sistolica == 0) {
                                    sistolica = val_presion;
                                }
                                diastolica = val_presion;
                                muestras_sin_pulso = 0; 
                            } else {
                                if (sistolica != 0) {
                                    muestras_sin_pulso++;
                                }
                            }
                        }
                    }
                    
                    if ((sistolica != 0) && (muestras_sin_pulso >= 133)) {
                        estado = 4; 
                    }
                    break;

                case 4: 
                    if (val_presion < P_MIN) { 
                        estado = 5; 
                        timer_estado_5 = 0; 
                    } 
                    break;

                case 5: 
                    timer_estado_5++;
                    if (timer_estado_5 > 66) {
                        estado = 0; 
                    }
                    break;
            }
        }

        
        if (PIE1bits.TXIE == 0) {
            if (turno_envio == 0) { 
                mensaje_T[1] = 'P'; 
                aux1 = cad_a_mmhg(val_presion); 
                turno_envio = 1; 
            } else if (turno_envio == 1) { 
                mensaje_T[1] = 'E'; 
                aux1 = estado; 
                turno_envio = 2; 
            } else if (turno_envio == 2) { 
                mensaje_T[1] = 'H'; 
                aux1 = cad_a_mmhg(sistolica); 
                turno_envio = 3; 
            } else { 
                mensaje_T[1] = 'L'; 
                aux1 = cad_a_mmhg(diastolica); 
                turno_envio = 0; 
            }
            
            for (i = 0; i < 4; i++) { 
                mensaje_T[5-i] = (aux1 % 10) + 0x30; 
                aux1 = aux1 / 10; 
            }
            
            TXREG = mensaje_T[0]; 
            punt_T = &mensaje_T[1]; 
            PIE1bits.TXIE = 1;
        }
    }

    
    if ((PIR1bits.TXIF == 1) && (PIE1bits.TXIE == 1)) {
        TXREG = *punt_T;
        if (*punt_T == '*') {
            PIE1bits.TXIE = 0;
        } else {
            punt_T++;
        }
    }

    
    if (RCSTAbits.OERR == 1) { 
        RCSTAbits.CREN = 0; 
        RCSTAbits.CREN = 1; 
    } 
    
    if ((PIR1bits.RCIF == 1) && (PIE1bits.RCIE == 1)) {
        if (primero == 0) {
            if (RCREG == '*') { 
                primero = 1; 
                punt_R = &mensaje_R[0]; 
                *punt_R = RCREG;
                punt_R++;
            }
        } else {
            *punt_R = RCREG;
            if (*punt_R == '*') {
                if (mensaje_R[1] == 'S') { 
                    if (estado == 0) { 
                        estado = 1; 
                        sistolica = 0; 
                        diastolica = 0; 
                    } else { 
                        estado = 0; 
                    }
                }
                primero = 0;
            } else {
                punt_R++;
                if (punt_R >= &mensaje_R[6]) {
                    primero = 0;
                }
            }
        }
    }
}


void usart_ini(void) {
    BAUDCONbits.BRG16 = 1; 
    TXSTAbits.BRGH = 1;
    SPBRG = 0x81; 
    SPBRGH = 0x06;
    
    TRISCbits.TRISC6 = 1; 
    TRISCbits.TRISC7 = 1;
    
    TXSTAbits.SYNC = 0; 
    RCSTAbits.SPEN = 1; 
    TXSTAbits.TXEN = 1; 
    RCSTAbits.CREN = 1;
    
    IPR1bits.RCIP = 1; 
    IPR1bits.TXIP = 1; 
    PIR1bits.RCIF = 0; 
    PIE1bits.RCIE = 1;
}

void ini_PORTs(void) {
    TRISD = 0x00; 
    LATD = 0x00; 
    
    TRISB = 0xFF; 
    WPUB = 0xFF; 
    
    ANSELHbits.ANS12 = 0; 
    
    INTCON2bits.RBPU = 0;
    INTCONbits.INT0IE = 1; 
    INTCONbits.INT0IF = 0; 
    INTCON2bits.INTEDG0 = 0; 
}

void ini_TMR3(void) {
    T3CON = 0; 
    T3CONbits.RD16 = 1; 
    T3CONbits.T3CKPS = 3; 
    IPR2bits.TMR3IP = 1;
}

void ini_TMR0(void) {
    T0CON = 0x03; 
    T0CONbits.T08BIT = 0; 
    T0CONbits.T0CS = 0; 
    T0CONbits.PSA = 0;
    
    INTCON2bits.TMR0IP = 1; 
    INTCONbits.TMR0IF = 0; 
    INTCONbits.TMR0IE = 1;
    
    T0CONbits.TMR0ON = 1; 
}

void ini_CAD(void) {
    ADCON0 = 0; 
    ADCON0bits.CHS = 0; 
    ADCON0bits.ADON = 1;
    
    ADCON1 = 0; 
    ADCON1bits.VCFG0 = 0; 
    ADCON1bits.VCFG1 = 1; 
    
    ADCON2 = 0; 
    ADCON2bits.ADFM = 1; 
    ADCON2bits.ACQT = 0b101; 
    ADCON2bits.ADCS = 0b110;
    
    TRISAbits.RA0 = 1; 
    ANSELbits.ANS0 = 1; 
    TRISAbits.RA1 = 1; 
    ANSELbits.ANS1 = 1; 
    
    PIR1bits.ADIF = 0; 
    IPR1bits.ADIP = 1; 
    PIE1bits.ADIE = 1;
}

void ini_CVREF(void) {
    CVRCON = 0; 
    CVRCONbits.CVRR = 1; 
    CVRCONbits.CVR = 13; 
    CVRCONbits.CVREN = 1;
}

void ini_PWM(void) {
    TRISCbits.RC1 = 0; 
    
    CCP2CONbits.CCP2M = 0b1111; 
    CCP2CONbits.DC2B = 0; 
    
    CCPR2L = 0; 
    
    T2CONbits.T2CKPS = 0b11; 
    PR2 = 255; 
    
    T2CONbits.TMR2ON = 1; 
}

void main(void) {
    OSCCONbits.IRCF = 7; 
    OSCTUNEbits.PLLEN = 1; 
    OSCCONbits.SCS = 0;
    
    RCONbits.IPEN = 0; 
    INTCONbits.GIEH = 1; 
    INTCONbits.GIEL = 1;

    
    ini_PORTs(); 
    ini_TMR3(); 
    ini_CAD(); 
    ini_PWM();
    ini_TMR0(); 
    usart_ini(); 
    ini_CVREF();

    // Bucle infinito
    while (1) { 
        Nop(); 
    }
}