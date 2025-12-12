# Sistema IoT Seguro con MQTT, TLS y Bridge en GL-MT300N-V2  
### Sonda ESP32 ↔ Agregador (Bróker local Mosquitto Bridge) ↔ Webalumnos (Bróker central Mosquitto)

---

## 1. Arquitectura del Sistema

El sistema está formado por **tres niveles**, todos comunicándose mediante **MQTT sobre TLS**:

### 1.1 Sonda (ESP32)
- Sensor simulado mediante potenciómetro.
- Publica datos de medida y alarma.
- Recibe órdenes (suscripción a alarma).
- Conexión segura mediante:
  - **TLS**
  - **Autenticación usuario/contraseña**
  - **Validación del broker mediante certificado CA convertido a `ca.h`**

### 1.2 Agregador (GL-MT300N-V2)
- Broker Mosquitto local con autenticación y TLS.
- Hace de puente (bridge) hacia el broker central webalumnos.
- Envía todos los topics mediante un túnel TLS.

### 1.3 Servidor Central (Webalumnos)
- Broker MQTT de la asignatura.
- Recibe y reenvía datos procedentes de todas las sondas.

---

## 2. Funcionamiento General

1. La **sonda** envía cada 2 s:
   - `sonda/<ID>/tipo/datos` → JSON con medidas
   - `sonda/<ID>/tipo/alarma` → `"true"` o `"false"`

2. El **agregador** recibe estos mensajes y, por el **bridge TLS**, los reenvía a webalumnos.

3. Desde la sonda también nos suscribimos al tópico de alarma: esta vuelve por el bridge y llega a la sonda.
 El ESP32:
   - Enciende el LED si recibe `"true"`
   - Lo apaga cuando reciba `"false"` o cuando baje la medida. (publicación de medida mayor que 1000 --> publicación de alarma true --> suscripción a alarma true --> led encendido)

---

## 3. Código de la Sonda (ESP32)

### 3.1 Características principales
- Conexión WiFi al GLinet.
- Uso de:
  - `SSLClient` → manejo de TLS
  - `ArduinoMqttClient` → MQTT seguro
  - `ArduinoJson` → datos estructurados
- Publicación y suscripción en topics propios.
- LED que indica estado de alarma.

### 3.2 Certificado CA
Archivo generado a través de una CA propia:

pipenv run python3 pycert_bearssl.py convert ca.crt -o ca.h -n


## 4. Estructura de Topics

| Función         | Topic            	     | Descripción                 |
| --------------- | -------------------      | --------------------------- |
| Datos           | `sonda/<ID>/tipo/datos`  | JSON con medida + timestamp |
| Alarma          | `sonda/<ID>/tipo/alarma` | `"true"` / `"false"`        |




## 5. Formato del JSON enviado

La sonda publica:

{
  "medida": 1234,
  "timestamp": 1765305840
}



## 6. Funcionamiento del Loop del ESP32

Cada 2 segundos:

Lee el potenciómetro.

Evalúa si hay alarma (medida > umbral).

Publica "true" o "false" en sonda/<ID>/tipo/alarma.

Enciende/apaga LED según la alarma actual

Publica un JSON en sondas/<ID>/data.

El LED se mantiene encendido mientras la alarma sea true, aunque baje la medida, hasta recibir "false" desde MQTT.



## 7. Configuración del Broker Local (Agregador GLinet)

Todos los archivos están en:

/etc/mosquitto/conf.d/

### 7.1 Autenticación – auth.conf

allow_anonymous false
password_file /etc/mosquitto/passwd

Usuario creado:
mosquitto_passwd -c /etc/mosquitto/passwd sonda1
(Usuario/contraseña: sonda1 : sonda1)

### 7.2 TLS Local – tls.conf

listener 8883
cafile /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate false
tls_version tlsv1.2

### 7.3 Bridge a Webalumnos – bridge.conf

connection webalumnos
address webalumnos.tlm.unavarra.es:10421

topic # both 0

remote_username grupo05
remote_password Hiezoz8foo

try_private false
bridge_cafile /etc/ssl/certs/ca-certificates.crt

### Importante
- Si se activa bridge_cafile con un certificado distinto del aceptado por Webalumnos, el bridge no conecta.
- El broker central utiliza una CA distinta.



## 8. Evidencia de Funcionamiento
#### 8.1 Logs del ESP32

[INFO] Pot=2012 Timestamp=1765228411 

[MQTT] Publicado O2: {"valor":2012,"timestamp":1765228411} 

[MQTT] Publicada ALARMA: true 

[MQTT] Recibido: Topic: /sonda/34/o2/alarma MSG: true 

[MQTT] Recibido: Topic: /sonda/34/o2/alarma MSG: true 

[INFO] Pot=1995 Timestamp=1765228413 

[MQTT] Publicado O2: {"valor":1995,"timestamp":1765228413} 

[INFO] Pot=1997 Timestamp=1765228415 

[MQTT] Publicado O2: {"valor":1997,"timestamp":1765228415}



#### Se muestra lo siguiente en el agregador (bridge mqtt, ejecutandolo con mosquitto -c /etc/mosquitto/mosquitto.conf -v)

Timestamp       Acción             Origen/Destino                   Topic                   Bytes

1765305832      Received PUBLISH   local.GL-MT300N-V2.webalumnos    /sonda/34/o2/alarma     4

1765305832      Sending PUBLISH    Arduino-00001066                 /sonda/34/o2/alarma     4

1765305834      Received PUBLISH   Arduino-00001066                 /sonda/34/o2/datos      36

1765305834      Received PUBLISH   Arduino-00001066                 /sonda/34/o2/alarma     5

1765305834      Sending PUBLISH    local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

1765305834      Sending PUBLISH    local.GL-MT300N-V2.webalumnos    /sonda/34/o2/alarma     5

1765305834      Sending PUBLISH    Arduino-00001066                 /sonda/34/o2/alarma     5

1765305834      Received PUBLISH   local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

1765305834      Received PUBLISH   local.GL-MT300N-V2.webalumnos    /sonda/34/o2/alarma     5

1765305834      Sending PUBLISH    Arduino-00001066                 /sonda/34/o2/alarma     5

1765305836      Received PUBLISH   Arduino-00001066                 /sonda/34/o2/datos      36

1765305836      Sending PUBLISH    local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

1765305836      Received PUBLISH   local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

1765305838      Received PUBLISH   Arduino-00001066                 /sonda/34/o2/datos      36

1765305838      Sending PUBLISH    local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

1765305838      Received PUBLISH   local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

1765305840      Received PUBLISH   Arduino-00001066                 /sonda/34/o2/datos      36

1765305840      Sending PUBLISH    local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

1765305840      Received PUBLISH   local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

1765305842      Received PUBLISH   Arduino-00001066                 /sonda/34/o2/datos      36

1765305842      Sending PUBLISH    local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

1765305842      Received PUBLISH   local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

1765305844      Received PUBLISH   Arduino-00001066                 /sonda/34/o2/datos      36

1765305844      Sending PUBLISH    local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

1765305844      Received PUBLISH   local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

1765305846      Received PUBLISH   Arduino-00001066                 /sonda/34/o2/datos      36

1765305846      Sending PUBLISH    local.GL-MT300N-V2.webalumnos    /sonda/34/o2/datos      36

