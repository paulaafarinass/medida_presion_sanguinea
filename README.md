# Sistema de Medición de Presión Sanguínea (Tensiómetro en C) 🩸

Este repositorio contiene el firmware escrito en C para un microcontrolador (familia PIC), diseñado para automatizar el ciclo de medición de un esfigmomanómetro no invasivo. El sistema controla la electroválvula de inflado/desinflado y procesa la señal de presión analógica para estimar la presión sistólica y diastólica.

> **Nota:** Este proyecto fue desarrollado como práctica académica de sistemas embebidos, partiendo de una estructura base que ha sido adaptada y corregida. Se publica como *Work in Progress (WIP)* para ilustrar el control de hardware mediante máquinas de estados finitas.

## ⚙️ Arquitectura del Firmware

El sistema está gobernado íntegramente por interrupciones de hardware y se basa en una **Máquina de Estados de 6 niveles (`estado = 0` a `5`)**:

1. **Estado 0 (Reposo):** Electroválvula apagada. A la espera de inicio por pulsador (INT0) o comando serie (UART).
2. **Estado 1 (Inflado):** Activación continua de la electroválvula hasta alcanzar la presión máxima de seguridad (`P_MAX`).
3. **Estado 2 (Estabilización):** Pausa temporal (`timer_estabilizacion`) para evitar picos transitorios tras apagar la bomba.
4. **Estado 3 (Desinflado y Medición):** Desinflado controlado mediante PWM simulado por contadores. Se evalúa continuamente la entrada analógica en busca del pulso cardíaco para registrar la presión sistólica y diastólica.
5. **Estado 4 (Vaciado Rápido):** Apertura total de la válvula al detectar que las pulsaciones han cesado y se han tomado los datos, hasta alcanzar la presión mínima (`P_MIN`).
6. **Estado 5 (Reinicio):** Transición de seguridad antes de volver al reposo.

## 🔌 Periféricos y Recursos Utilizados

- **Módulo ADC:** Conversión de la señal analógica del sensor de presión.
- **Módulo PWM:** Control de motores/válvulas en fases iniciales.
- **Timers (TMR0, TMR3):** Base de tiempos para el muestreo del ADC (Timer0) y el rebote de botones (Timer3).
- **USART:** Transmisión de telemetría en tiempo real (estado, presión actual, sistólica y diastólica) hacia un ordenador mediante tramas formateadas `*P0000*`.

## ⚠️ Limitaciones Conocidas y Futuras Mejoras (Known Issues)

Debido a su naturaleza académica y orientada a la simulación, el código presenta áreas de mejora para una implementación física en entornos reales:

* **Sobrecarga de la ISR:** Actualmente, el cálculo de las presiones, la gestión de la máquina de estados y la codificación ASCII para la USART ocurren dentro de la rutina de servicio de interrupción (ISR). Para uso en producción, esta lógica debería delegarse al bucle principal (`main`), dejando la ISR solo para capturar datos y levantar *flags*.
* **Algoritmo de detección de picos:** La detección de las presiones sistólica y diastólica se basa en un umbral fijo sobre la presión base (`UMBRAL_PULSO`). En un entorno físico, el ruido analógico generaría falsos positivos. Sería necesario implementar un filtro digital (FIR/IIR) o un detector de envolventes de los pulsos oscilométricos.
* **Temporizaciones acopladas:** Los tiempos de estabilización e inflado están *hardcodeados* mediante contadores. Cambiar la frecuencia de reloj del microcontrolador desajustaría el sistema.

## 💻 Compilación

Desarrollado para el compilador **XC8** de Microchip. 
