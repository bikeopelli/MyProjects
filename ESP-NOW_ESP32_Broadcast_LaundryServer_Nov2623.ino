// Author: Dave Garner
// Date: Dec 1, 2023
// 
// Hardware:           
//          One ESP32(this sketch) using ESP-NOW to communicate with two clients, and Serial2 to
//          recieve 'sentences' from the LD1125H.
//          Two ESP8266's that get ESP-NOW messages to turn on/off the warning LED's.
//          HLK-LD1125H DC3.3-5V 24Ghz Millimetre Wave Radar Module Human Presence Sensor
//
// Envirnonment:
//          A laundry room situated between an interior hallway and the garage with a door
//          on either. The interior of the laundry room is about 60" in length. Above 
//          each door (on the outside) are two LED's. If the laundry is occupied those LED's 
//          flash on/off for the purpose of warning anyone approaching either door that
//          the room is accupied.
//         
// Method: 1. Flash LED's over the outside of both doors to the laundry room
//             providing a warning that the laundy room is occupied (or not)
//
//             This sketch communicates with two (one for each door) ESP8266's
//             through ESP-NOW. Those  ESP8266's flash the LED's. 
  
//           Input takes the form:   "mov, dis=x.zz" where x.zz is the float distance in meters
//                                or "occ, dis=x.zz" where x.zz            
//                  
//           My previous attempts, with other sensors, to show the warning LED's flashing never
//           worked that well, An example is, they kept the LED's flashing long after
//           the laundry room became unoccupied and since they required movement
//           they would often stop flashing when the room was actually occupied.
//           The LD1125H solved all my problems. Simply stated...The LED's flash
//           when the laundry room is occupied and do not when not occupied. 
//           
//                    
  
#include <Wire.h> 
#include <WiFi.h>
#include <esp_now.h>

//=========================== ESPNOW Stuff ==============================================

void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength)
// Formats MAC Address
{
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}


void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen)
 
{
 
  // Only allow a maximum of 250 characters in the message + a null terminating byte
  char buffer[ESP_NOW_MAX_DATA_LEN + 1];
  int msgLen = min(ESP_NOW_MAX_DATA_LEN, dataLen);
  strncpy(buffer, (const char *)data, msgLen);

  // Make sure we are null terminated
  buffer[msgLen] = 0;

  // Format the MAC address
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);

  // Send Debug log message to the serial port if not the laundry room server
  Serial.printf("Received message from: %s - %s\n", macStr, buffer);
  
}


void sentCallback(const uint8_t *macAddr, esp_now_send_status_t status)
// Called when data is sent
{
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void broadcast(const String &message)
// Emulates a broadcast
{
  // Broadcast a message to every device in range
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
  if (!esp_now_is_peer_exist(broadcastAddress))
  {
    esp_now_add_peer(&peerInfo);
  }
  // Send message
  esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t *)message.c_str(), message.length());

  // Print results to serial monitor
  if (result == ESP_OK)
  { Serial.println("Broadcast message success");}
  else if (result == ESP_ERR_ESPNOW_NOT_INIT) {Serial.println("ESP-NOW not Init.");}
  else if (result == ESP_ERR_ESPNOW_ARG)      {Serial.println("Invalid Argument");}
  else if (result == ESP_ERR_ESPNOW_INTERNAL) {Serial.println("Internal Error");}
  else if (result == ESP_ERR_ESPNOW_NO_MEM)   {Serial.println("ESP_ERR_ESPNOW_NO_MEM");}
  else if (result == ESP_ERR_ESPNOW_NOT_FOUND){Serial.println("Peer not found.");}
  else{Serial.println("Unknown error");}
}
//=========================================================================

int count =0;

float OneMeter = 39.37; // inches
float inches ; 
float MaxDistance = 60.00;
bool Occupied = false;
char  SwitchState[] = "x";      
const int FlashnledPin = 33;  

int LightThreshold = 4000;
int PhotoResistorValue;
int PhotoResistorPin =35;
//================================= setup ===================================
// Establish Serial2 to read the LD1125m values

void setup () {
   Serial.begin(115200);
  delay(1000);
  Serial2.begin (115200, SERIAL_8N1, 13, 17);  
  delay (1000);
  Wire.begin();
  delay(500);
  Serial.println(" Fire this baby up! "); 

   // Set ESP32 in STA mode to begin with
  WiFi.mode(WIFI_STA);
  Serial.println("ESP-NOW Broadcast ");

  // Print MAC address
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Disconnect from WiFi
  WiFi.disconnect();

  // Initialize ESP-NOW
  if (esp_now_init() == ESP_OK)
  {
    Serial.println("ESP-NOW Init Success");
    esp_now_register_recv_cb(receiveCallback);
    esp_now_register_send_cb(sentCallback);
  }
  else
  {
    Serial.println("ESP-NOW Init Failed");
    delay(3000);
    ESP.restart();
  }
  
  pinMode(FlashnledPin, OUTPUT); 
  
 
}

//================================== loop =================================== 

void loop () 
 {

//=============================Turn the LED's off ==============================

// When there is no occupancy or movement the LD1125H stops sending sentences
// This condition only exists once the room is exited.
// Here if after 5 (just a guess) attempts to read sentences fail that condition
// is interpreted to indicate the room is not occupied. Turn the LED's off. 
  
int xx = 0; 
    if (Occupied) 
     { 
      while (!Serial2.available() && xx < 5)
       {
//      Serial.print(" xx1=");  Serial.println(xx);         
        delay(50); xx++;    
       }
//    Serial.print(" xx2=");  Serial.println(xx); 
      if(xx == 5)
       {   
        digitalWrite(FlashnledPin, LOW);     
//      Serial.println("Nota Broadcast Turn flashing light off++++++++++++");
        Occupied = false;
        SwitchState[0] = 'N';          
        digitalWrite(FlashnledPin, LOW);  
        broadcast("^E");     
       }   
     }

//=============================Read the input data stream ==============================

// Input takes the form:   mov, dis=x.zz where x.zz is the float distance in meters
//                      or occ, dis=x.zz where x.zz
//                     'mov' = movement; 'occ' = occupied 
// Here the '=' of '=x.zz' is searched for. When found the following 'x.zz' characters 
// are converted to float, then the float is converted to inches.

 count = 0;
 char characters[5]  = "    " ;

  while (Serial2.available())
   {
    characters[count] = Serial2.read();
//  if ( characters[count] == 'o')  Serial.print("occupied"); 
//  if ( characters[count] == 'm')  Serial.print("movement"); 
    if ( characters[count] == '=') 
     {  
      while(count < 4)
       {
         while (!Serial2.available()) {}
         characters[count] = Serial2.read(); 
         count++;
       }

// convert them to float then to inches 
           
      Serial.print(" meters=");  Serial.print(characters); 
      inches = atof (characters);
      inches *= OneMeter; 
      Serial.print(" inches=");  Serial.print(inches); 
       
// Now see if 'this' value is inside or outside the max distance and if 'this' 
// value is the same (inside or outside) as the last one
     
//    if ( Occupied && inches <=  MaxDistance) {Serial.println(" O");}
//    if (!Occupied && inches > MaxDistance)   {Serial.println(" N**********************************");} 
       
      if (Occupied && inches >  MaxDistance)     
       {         
//      Serial.print(" Switch to 'Not Occupied' ==================== ");      
        Occupied = false;
        SwitchState[0] = 'N';          
        digitalWrite(FlashnledPin, LOW);  
        broadcast("^E");    
       }
      if (!Occupied && inches <=  MaxDistance) 
       { 
//      Serial.println(" Switch to 'Occupied' ======");   
        Occupied = true;
        SwitchState[0] = 'O';         
        digitalWrite(FlashnledPin, HIGH);       
        broadcast("^S"); 
       }            
     }  /* while(count < 4) */
   }  /* while (Serial2.available()) */
 }  /* loop */
