// Author: Dave Garner
// Date: Nov, 2023
// 
// Hardware:           
//          ESP32 using ESP-NOW to communicate with two clients and Serial2 to
//          recieve 'sentences' from the LD1125H.
//          HLK-LD1125H DC3.3-5V 24Ghz Millimetre Wave Radar Module Human Presence Sensor
//         
// Purpose: 1. Flash LED's over the outside of both doors to the laundry room
//             providing a warning that the laundy room is occupied (or not)
//
//             This sketch communicates with two (one for each door) ESP8266's
//             through ESP-NOW. Those  ESP8266's flash the LED's. 
  
//           Input takes the form:   "mov, dis=x.zz" where x.zz is the float distance in meters
//                                or "occ, dis=x.zz"           
//                  
// Method:   My previous attempts to show the warning LED's flashing never
//           worked that well, An example is, they kept the LED's flashing long after
//           the laundry room became unoccupied and since they required movement
//           they would often stop flashing when the room was actually occupied.
//           While this sensor is not always accurate it does, for the most part,
//           keep the LED's flashing even when there is an occupant but not moving.
//           It does not seem to report distance accuratly when a person is within  
//           40" or so, but is quite accurate over that. In this application I just
//           want to know if  the person is withn 60" so this limitation does not
//           affect me here. Also, randomly, it will report a false distance for
//           a very short time.
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

const int FlashnledPin = 18; 

//================================= setup ===================================
// Establish Serial2 to read the LD1125m values
// Initialize ESPNOW

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

  pinMode(TrigdledPin, OUTPUT);   pinMode(FlashnledPin, OUTPUT);  
}

//================================== loop =================================== 

void loop () {
   count = 0;
   char characters[5]  = "    " ;

// Input takes the form:   mov, dis=x.zz where x.zz is the float distance in meters
//                      or occ, dis=x.zz where x.zz
//                      and gather up the next 4 characters "x.zz"

   while (Serial2.available())
   {
    characters[count] = Serial2.read();
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
     
      if ( Occupied && inches <=  MaxDistance) {Serial.println(" O");}
      if (!Occupied && inches > MaxDistance)   {Serial.println(" N**********************************");} 
       
      if (Occupied && inches >  MaxDistance)     
       {         
        Serial.print(" Switch to 'Not Occupied' ==================== ");      
        Occupied = false;
        SwitchState[0] = 'N';          
        digitalWrite(FlashnledPin, LOW);  
        broadcast("^E");    
       }
      if (!Occupied && inches <=  MaxDistance) 
       { 
        Serial.println(" Switch to 'Occupied' ======");   
        Occupied = true;
        SwitchState[0] = 'O';        
        digitalWrite(FlashnledPin, HIGH);       
        broadcast("^S"); 
       }            
     }  /* while(count < 4) */
    }  /* while (Serial2.available()) */
}  /* loop */
