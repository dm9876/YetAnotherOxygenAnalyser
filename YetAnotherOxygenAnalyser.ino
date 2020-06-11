//sensor, tip = gnd, sleeve = signal 

#include <Arduino.h>
#include <U8x8lib.h>
#include <Adafruit_ADS1015.h>
#include <Bounce2.h>
#include <EEPROM.h>
#include <Smoothed.h>
#include <DHT.h>;
#include <Wire.h>

#define OP_POWER_LATCH 13
#define IP_POWER_BUTTON 8
#define IP_HOLD_BUTTON 9
#define IP_CALIBRATE_BUTTON 10
#define POWER_OFF_TIMEOUT_SETPOINT 300000
#define DHTPIN 7     
#define DHTTYPE DHT22   // DHT 22  (AM2302)

Adafruit_ADS1115 ads(0x48);  /* Use this for the 16-bit version */

Bounce PowerButtonDebouncer = Bounce(); // Instantiate a Bounce object
Bounce HoldButtonDebouncer = Bounce(); // Instantiate a Bounce object
Bounce CalibrateButtonDebouncer = Bounce(); // Instantiate a Bounce object

DHT dht(DHTPIN, DHTTYPE); //// Initialize DHT sensor for normal 16mhz Arduino

Smoothed <float> battVoltage;
Smoothed <float> O2Sensor_mV;

float O2Percent, O2Calibration, Sensor_mV, MOD14, MOD16, H, T;
bool LowBattery = 0;
unsigned long power_off_timeout = 0;
int eeAddress = 0;   //Location we want the data to be put.

// U8x8 Contructor List 
// The complete list is available here: https://github.com/olikraus/u8g2/wiki/u8x8setupcpp
// Please update the pin numbers according to your setup. Use U8X8_PIN_NONE if the reset pin is not connected
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);

enum states {ST_STARTUP, ST_MONITORING, ST_HOLD, ST_CALIBRATE, ST_STOP} state;



void setup(void) 
{
  u8x8.begin();
  dht.begin();
  ads.begin();
  battVoltage.begin(SMOOTHED_AVERAGE, 10);
  O2Sensor_mV.begin(SMOOTHED_AVERAGE, 20);

  pinMode(OP_POWER_LATCH, OUTPUT);
  
  PowerButtonDebouncer.attach(IP_POWER_BUTTON,INPUT); // Attach the debouncer to pin with input eg INPUT or INPUT_PULLUP mode
  PowerButtonDebouncer.interval(25); 
  HoldButtonDebouncer.attach(IP_HOLD_BUTTON,INPUT_PULLUP); // Attach the debouncer to pin with input eg INPUT or INPUT_PULLUP mode
  HoldButtonDebouncer.interval(25); 
  CalibrateButtonDebouncer.attach(IP_CALIBRATE_BUTTON,INPUT_PULLUP); // Attach the debouncer to pin with input eg INPUT or INPUT_PULLUP mode
  CalibrateButtonDebouncer.interval(25); 

  //put the state machine logic in the starting state 
  state_switch(ST_STARTUP, ST_STARTUP);
  
}

void loop(void) 
{
  PowerButtonDebouncer.update();
  HoldButtonDebouncer.update();
  CalibrateButtonDebouncer.update();

  //aquire ADC samples and add them to the smoothing object
  int16_t adc1;
  ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
  adc1 = ads.readADC_SingleEnded(1);
  O2Sensor_mV.add(adc1 * 0.0078125);

  //calculate values ready for display or use at any time, 
  //todo this can be moved into the display routine so its frequency is reduced and less float ops the main loop can be faster maybe
  
  Sensor_mV = O2Sensor_mV.get();
  O2Percent = Sensor_mV * O2Calibration;
  MOD14 = 1400 / O2Percent - 10;
  MOD16 = 1600 / O2Percent - 10;
 
  
  //reset the off timer if any buttons are pressed.
  if (CalibrateButtonDebouncer.fell() || HoldButtonDebouncer.fell())
  {
    power_off_timeout = millis();
  }

  //process the state logic
  state_logic();
  
}  

void state_logic(void) {
  switch(state)
  {
    case ST_STARTUP:
      state_switch(ST_MONITORING, state);
      break;
    case ST_MONITORING:
      st_monitoring_routine();
      if (millis() - power_off_timeout > POWER_OFF_TIMEOUT_SETPOINT)
      {
        state_switch(ST_STOP, state);      
      }
      else if (PowerButtonDebouncer.rose())
      {
        state_switch(ST_STOP, state);    
      }
      else if (HoldButtonDebouncer.fell())
      {
        state_switch(ST_HOLD, state);    
      }
      else if (CalibrateButtonDebouncer.fell())
      {
        state_switch(ST_CALIBRATE, state);    
      }
      break;
    case ST_HOLD:
      if (millis() - power_off_timeout > POWER_OFF_TIMEOUT_SETPOINT)
      {
        state_switch(ST_STOP, state);      
      }
      else if (PowerButtonDebouncer.rose())
      {
        state_switch(ST_STOP, state);    
      }
      else if (HoldButtonDebouncer.fell())
      {
        state_switch(ST_MONITORING, state);    
      }
      break;
    case ST_CALIBRATE:
      state_switch(ST_MONITORING, state);
      break;
    case ST_STOP:
      break;
    }

}

void state_switch(enum states next, enum states old)
{
  state = next;
  
  //on exit
  if (old == ST_STARTUP)
  {
    
  }
  if (old == ST_MONITORING)
  {
    
  }
  if (old == ST_HOLD)
  {
    
  }
  if (old == ST_CALIBRATE)
  {
    
  }
  if (old == ST_STOP)
  {
    
  }
  //on entry
  if (next == ST_STARTUP)
  {
    st_startup_onEntry_routine();
  }
  if (next == ST_MONITORING)
  {
    
  }
  if (next == ST_HOLD)
  {
    st_hold_onEntry_routine();
  }
  if (next == ST_CALIBRATE)
  {
    st_calibrate_onEntry_routine();
  }
  if (next == ST_STOP)
  {
    st_stop_onEntry_routine();
  }
}

void st_startup_onEntry_routine(void)
{
  // latches the power on
    digitalWrite(OP_POWER_LATCH, HIGH);   
    
    //welcome screen
    u8x8.setFont(u8x8_font_px437wyse700a_2x2_r);
    u8x8.drawString(0, 0, "Yet");
    u8x8.drawString(0, 2, "Another");    
    u8x8.drawString(0, 4, "Oxygen");  
    u8x8.drawString(0, 6, "Analyser");

    delay(3000);
    
    //check and display battery voltage (maybe need a new state switch direct from startup to stop?
    batteryCheck();
    //LowBattery
    
    //load previous calibration from EEPROM or use a default.
    EEPROM.get(eeAddress, O2Calibration);

    
    
}
void st_calibrate_onEntry_routine(void)
{
  float cal_value;
  cal_value = get_calibration_value(H, T);
  calibrate_sensor(cal_value);
  u8x8.clear();
  u8x8.setFont(u8x8_font_7x14_1x2_f);
  u8x8.setCursor(0, 0);
  u8x8.print("Calibrated to:");
  u8x8.setFont(u8x8_font_7x14B_1x2_f);
  u8x8.setCursor(0, 2);
  u8x8.print(cal_value, 1);
  u8x8.print("%");
  delay(2000);
    
}
void st_hold_onEntry_routine(void)
{
  update_display(1); 
}

void st_stop_onEntry_routine(void)
{
  u8x8.clear();
  u8x8.setInverseFont(0);
  u8x8.setFont(u8x8_font_px437wyse700a_2x2_r);
  u8x8.drawString(0, 2, " Power");
  u8x8.drawString(0, 4, "  Off");
  delay(2000);
  u8x8.clear();
  
  // releases the power latch
  digitalWrite(OP_POWER_LATCH, LOW);  
}
void st_monitoring_routine(void)
{
  update_display(0); 
}

void update_display(bool invert)
{
  if (invert) u8x8.setInverseFont(1);
  else u8x8.setInverseFont(0);
  
  //u8x8.clear();
  
  u8x8.setFont(u8x8_font_7x14B_1x2_f);
  u8x8.setCursor(0, 0);
  u8x8.print(O2Percent, 1);
  u8x8.print("%   "); //spaces on end to overite end of prior displayed value if display values get lower and thus shorter strings
  u8x8.setFont(u8x8_font_7x14_1x2_f);
  u8x8.setCursor(8, 0);
  u8x8.print("        ");
  u8x8.setCursor(8, 0);
  u8x8.print(Sensor_mV, 1);
  u8x8.print("mV"); 

  u8x8.setCursor(0, 2);
  u8x8.print(MOD14,1);
  u8x8.print("M @1.4  "); //spaces on end to overite end of prior displayed value if display values get lower and thus shorter strings

  u8x8.setCursor(0, 4);
  u8x8.print(MOD16,1);
  u8x8.print("M @1.6  "); //spaces on end to overite end of prior displayed value if display values get lower and thus shorter strings
  
  u8x8.setCursor(0, 6);
  u8x8.print(H, 0);
  u8x8.print("%RH    ");
  u8x8.setCursor(8, 6);
  u8x8.print("        ");
  u8x8.setCursor(8, 6);
  u8x8.print(T, 1);
  u8x8.print((char)176);
  u8x8.print("C"); 
  
}

void calibrate_sensor(float cal_value) 
{
  O2Calibration = cal_value / Sensor_mV;
  EEPROM.put(eeAddress, O2Calibration);
}

float get_calibration_value(float humidity, float temperature)
{
  //Analox O2EII®– Oxygen Analyser – Humidity Compensation
  //https://www.analoxsensortechnology.com/downloads/1417691936Analox_O2EII_RM-001-003_-_Humidity_compensation.pdf
  //humidity 0-100 x temp 0 - 49
  const uint8_t calibration_table[11][10] {
  {209, 209, 209, 209, 209, 209, 209, 209, 209, 209},
  {209, 209, 209, 209, 208, 208, 208, 208, 207, 207},
  {209, 209, 208, 208, 208, 208, 207, 206, 205, 204},
  {209, 208, 208, 208, 207, 207, 206, 205, 204, 202},
  {208, 208, 208, 207, 207, 206, 205, 204, 202, 199},
  {208, 208, 208, 207, 206, 205, 204, 202, 200, 197},
  {208, 208, 207, 207, 206, 205, 203, 201, 198, 195},
  {208, 208, 207, 206, 205, 204, 202, 199, 196, 192},
  {208, 208, 207, 206, 205, 203, 201, 198, 195, 190},
  {208, 207, 207, 206, 204, 203, 200, 197, 193, 187},
  {208, 207, 206, 205, 204, 202, 199, 195, 191, 185}
  };

  //indexes into the array
  int x, y;
    
  //range limit the inputs to this function
  if (humidity < 0 || humidity > 100)
    {
      humidity = 0;
    }
  
  //this makes is so 0-5 gives index 0, 5-15 gives index 1 etc
  humidity = humidity + 5; 
  x = (int)humidity/10;
 
  if (temperature < 3)
    {
      y = 0;  
    }
  else if (temperature >= 3 && temperature < 7)
    {
      y = 1;  
    }
  else if (temperature >= 7 && temperature < 13)
    {
      y = 2;  
    }
  else if (temperature >= 13 && temperature < 18)
    {
      y = 3;  
    }
  else if (temperature >= 18 && temperature < 25)
    {
      y = 4;  
    }
  else if (temperature >= 25 && temperature < 30)
    {
      y = 5;  
    }
  else if (temperature >= 30 && temperature < 35)
    {
      y = 6;  
    }
  else if (temperature >= 35 && temperature < 40)
    {
      y = 7;  
    }
  else if (temperature >= 40 && temperature < 45)
    {
      y = 8;  
    }
  else if (temperature >= 45)
    {
      y = 9;  
    }
  else
    {
      y = 0;
    }
 
  return (float)calibration_table[x][y] / 10;
}

void batteryCheck(void)
{
  float Battery;
  int16_t adc0;
  
  //aquire ADC samples and add them to the smoothing object
  ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  
  for ( int x = 1; x <= 10 ; x++ )
  {
    adc0 = ads.readADC_SingleEnded(0);
    battVoltage.add(adc0 * 0.1875 / 500); //div 500 so that voltage is in Volts not millivolts and is x2 (since battery voltage is monitored via a 50/50 voltage divider)
  }
  
  Battery = battVoltage.get(); 

  u8x8.clear();
  u8x8.setFont(u8x8_font_7x14_1x2_f);
  //u8x8.setInverseFont(0);
  
  u8x8.setCursor(0, 0);
  u8x8.print("Battery Voltage:");
  u8x8.setCursor(0, 2);
  u8x8.print(Battery);
  u8x8.print("V"); 

  
  if (Battery > 9.0)
  {
    u8x8.setCursor(0, 4);
    u8x8.print("Full");
  }
  else if (Battery > 6.0)
  {
    u8x8.setCursor(0, 4);
    u8x8.print("Good");
  }
  else if (Battery > 5.4)
  {
    u8x8.setCursor(0, 4);
    u8x8.print("Low");
  }
  else 
  {
    u8x8.setCursor(0, 4);
    u8x8.print("Too Low");
    LowBattery = 1;
  }

  delay(2000);
}
