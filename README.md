# Boya Climática Inteligente
Autor: Gonzalo Torán Mosulén

![Circuito de la Boya](images/CircuitoActividad2.png)

## Descripción del proyecto

Este proyecto simula una boya meteorológica autónoma, capaz de medir condiciones ambientales como temperatura, humedad, viento y luz solar. Además, simula una batería que se recarga con energía solar y consume energía dependiendo de los componentes en uso. Está implementado sobre Arduino UNO y diseñado en el entorno de simulación Wokwi.

Enlace al proyecto: https://wokwi.com/projects/432357065938941953

## Componentes utilizados

- Arduino Uno
- Sensor DHT22 (temperatura y humedad)
- Sensor LDR (luz)
- Potenciómetro (veleta - dirección del viento)
- Pulsador (anemómetro - velocidad del viento)
- LCD 16x2
- LED rojo (calefactor)
- LED azul (enfriador)
- LED blanco (luz de emergencia)
- 74HC595 (registro de desplazamiento)
- LEDs adicionales (luces de posición)
- Botón de usuario

## Funcionalidades

- Lectura de sensores: temperatura, humedad, luz, viento (dirección y velocidad).
- Control de actuadores según condiciones climáticas.
- Gestión de batería simulada, incluyendo carga y descarga.
- Modo de ahorro de energía: pantalla apagada cuando no se necesita.
- Visualización de información relevante en una pantalla LCD.

## Fragmento de código representativo

```cpp
void controlTemperatura() {
  if (temperatura < limiteInferior) {
    digitalWrite(CALEFACTOR_PIN, HIGH);
    digitalWrite(ENFRIADOR_PIN, LOW);
    carga_On = false;
  } else if (temperatura > limiteSuperior) {
    digitalWrite(CALEFACTOR_PIN, LOW);
    digitalWrite(ENFRIADOR_PIN, HIGH);
    carga_On = false;
  } else {
    digitalWrite(CALEFACTOR_PIN, LOW);
    digitalWrite(ENFRIADOR_PIN, LOW);
    carga_On = true;
  }
}
```

## Estructura del repositorio

- `boya_climatica.ino`: Código fuente principal
- `diagram.json`: Esquema del circuito en Wokwi
- `images/CircuitoActividad2.png`: Imagen del circuito
- `README.md`: Documentación del proyecto

## Cómo probarlo

1. Abrir el enlace de Wokwi.
2. Usar el botón verde para activar la pantalla.
3. Gira el potenciómetro para cambiar la dirección del viento.
4. Cambiar la luz del LDR y observar el comportamiento.
5. Pulsar el botón azul para simular ráfagas de viento.

## Conclusión

Este proyecto demuestra cómo una solución embebida puede integrarse con múltiples sensores y actuadores, gestionar su propio consumo energético y presentar información de forma clara al usuario. La simulación permite evaluar su comportamiento en condiciones variables.
