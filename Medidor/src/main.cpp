#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Firebase_ESP_Client.h>

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif


//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "jz"
#define WIFI_PASSWORD "42700203"

// Insert Firebase project API Key
#define API_KEY "AIzaSyCitYtCGLHg9aoDjsf3Sh6KSmxJaodWMDs"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://console.firebase.google.com/project/medidor-e8147/database/medidor-e8147-default-rtdb/data/~2F" 

// Define the credentials for the ESP32
#define USER_EMAIL "admin@admin.com"
#define USER_PASSWORD "adminadmin"


//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0;

int valor = 0;

String complete_path;
String tipo;

//variables de la LCD I2C
#define COLUMS 16
#define ROWS   2
#define PAGE   ((COLUMS) * (ROWS))
LiquidCrystal_I2C lcd(PCF8574A_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);

// Matriz de factores comerciales de resistencias
float resCom [14] ={1,1.2,1.5,1.8,2.2,2.4,2.7,3.3,3.9,4.7,5.1,5.6,6.8,8.2};

// Perifericos de salida
int led_1_4w = 4;
int led_1_2w = 5;
int buzzer = 2;

// Botones (Perifericos de entrada)
int bt_modo = 14;
int bt_aceptar = 16;
int bt_mostrar = 17;

// inicializacion de modos del sistema
bool modo = true;
bool aceptar = true;
bool mostrar = false;

// boton bluetooth
int bt_bth = 18;

// Inicializacion del primer ADC (1/4W)
const int vsensorA = 36;

// Inicializacion del segundo ADC (1/2W)
const int vsensorB = 39;

// Elegir el ADC 
bool adc_select=true;

void selec_mode()
{
  lcd.clear();
  if(digitalRead(led_1_4w)== HIGH)
  {
    digitalWrite(led_1_2w,HIGH);
    digitalWrite(led_1_4w,LOW);
    lcd.setCursor(0,0);
    lcd.print("Select mode:");
    lcd.setCursor(3,1);
    lcd.print("1/2W");
    adc_select = true;
  }
  else
  {
    digitalWrite(led_1_2w,LOW);
    digitalWrite(led_1_4w,HIGH);
    lcd.setCursor(0,0);
    lcd.print("Select mode:");
    lcd.setCursor(3,1);
    lcd.print("1/4W");
    adc_select = false;
  }
}

float escogerRes(float r)
{
  int i=0;
  for(i=0;i<14;i++)
  {
    float valorA=resCom[i];
    if(r>(resCom[i]-(resCom[i]*0.091) ) && r<(resCom[i]+(resCom[i]*0.09) ))
    {
      return(resCom[i]);
    }
    if(r>((resCom[i]-(resCom[i]*0.091) )*10) && r<((resCom[i]+(resCom[i]*0.09) )*10))
    {
      return(resCom[i]*10);
    }
    if(r>((resCom[i]-(resCom[i]*0.091) )*100) && r<((resCom[i]+(resCom[i]*0.09) )*100))
    {
      return(resCom[i]*100);
    }
    if(r>((resCom[i]-(resCom[i]*0.091) )*1000) && r<((resCom[i]+(resCom[i]*0.09) )*1000))
    {
      return(resCom[i]*1000);
    }
    if(r>((resCom[i]-(resCom[i]*0.091) )*10000) && r<((resCom[i]+(resCom[i]*0.09) )*10000))
    {
      return(resCom[i]*10000);
    }   
  }  
}

float calculoRes()
{
  float voltage = 0;
  int muestras = 100;

  for (int i=0;i<muestras;i++)
  {
    if (!adc_select)
    {
      voltage = voltage + analogRead(vsensorA)*(3.313/4095.0);
    }
    else
    {
      voltage = voltage + analogRead(vsensorB)*(3.313/4095.0);
    }
  }
  
  voltage = (voltage/muestras)+0.3;
  float current = (3.313-voltage)/9750.9;
  float res = voltage/current;
  return escogerRes(res);
}

void tono()
{
  digitalWrite(buzzer,HIGH);
  delay(50);
  digitalWrite(buzzer,LOW);
}

void setup() {
  Serial.begin(115200);
  lcd.begin(COLUMS,ROWS);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lcd.setCursor(0,0);
  lcd.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    delay(300);
  }
  lcd.clear();
  lcd.setCursor(2,0);
  lcd.print("Conexion");
  lcd.setCursor(3,1);
  lcd.print("exitosa");
  delay(500);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
 
  pinMode(bt_modo,INPUT_PULLUP);
  pinMode(bt_aceptar,INPUT_PULLUP);
  pinMode(bt_mostrar,INPUT_PULLUP);
  pinMode(bt_bth,INPUT_PULLUP);
  pinMode(vsensorA,INPUT);
  pinMode(vsensorB,INPUT);

  pinMode(led_1_2w,OUTPUT);
  pinMode(led_1_4w,OUTPUT);
  pinMode(buzzer,OUTPUT);

  lcd.clear();
  lcd.setCursor(2,0);
  lcd.print("**Ecuares**");
  delay(1000);
  lcd.clear();
}

void registro()
{
  lcd.clear();
  float valorRes = calculoRes();
  lcd.setCursor(2,0);
  lcd.printf("R= %.2f",valorRes);      
  lcd.setCursor(2,1);

  if (digitalRead(led_1_2w)==HIGH)
  {
    lcd.print("P=1/2W");
  }
  else
  {
    lcd.print("P=1/4W");
  }
  
  delay(1500);
  
  if (digitalRead(led_1_4w)== HIGH)
  {
    tipo = "/1_4W";
  }
  else
  {
    tipo = "/1_2W";
  }

  complete_path = "/datos";
  complete_path += tipo;

  database_connect(complete_path);

  digitalWrite(led_1_2w,LOW);
  digitalWrite(led_1_4w,LOW);
  
  }

void database_connect(String path)
{
  if(Firebase.ready() && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();

    if(Firebase.RTDB.getInt(&fbdo,path,&valor))
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("**Resistencia**");
      lcd.setCursor(1,1);
      lcd.print("*existente*");
      delay(500);

      if(Firebase.RTDB.setInt(&fbdo,path,valor+1))
      {
        lcd.clear();
        lcd.setCursor(1,0);
        lcd.print("**Agregando**");
        lcd.setCursor(0,1);
        lcd.print(".........");
        delay(500);
      }
      else
      {
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("**Fallo**");
        delay(700);
      }


    }
    else
    {
      valor = 1;
      lcd.clear();
      lcd.setCursor(3,0);
      lcd.print("**Nueva**");
      lcd.setCursor(0,1);
      lcd.print("**resistencia**");
      delay(500);
    }
  }
}


void loop() 
{
    while (aceptar)
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Iniciar medicion");
      while (modo)
      {
        if(digitalRead(bt_modo)==LOW)
        {
          tono();
          delay(20);
          if(digitalRead(bt_modo)==LOW)
          {
            selec_mode();
            while (digitalRead(bt_modo)==LOW)
            {
              delay(20);
            }
          }
        }
        if((digitalRead(led_1_2w)==HIGH | digitalRead(led_1_4w)== HIGH) & digitalRead(bt_aceptar)==LOW)
        {
          tono();
          modo = false;
          lcd.clear();
          lcd.setCursor(1,0);
          lcd.print("*Realizando*");
          lcd.setCursor(2,1);
          lcd.print("medicion");
          delay(2000);
        }
        if(digitalRead(bt_mostrar)==LOW)
        {
          modo=false;
          mostrar = true;
        }
      }
    }
}


