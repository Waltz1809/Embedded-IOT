#include<LiquidCrystal.h>

const int sensor_pin = A1; 
LiquidCrystal lcd(12, 11, 4, 5, 6, 7);

void setup()
{
 	lcd.begin(16, 2); 

}

void loop()
{
  	int sensor_data = analogRead(sensor_pin);
  
  	/*Tính toán điện áp analog bằng cách nhân giá trị trong
  	sensor cho 5/1024*/
  	float volt = sensor_data * (5.0 / 1024.0);
  
  	/*Tính toán độ C bằng điện áp / 100. -0.5 điện áp bù*/
  	float temp_C = (volt - 0.5) * 100;
  
  	/*Đổi C sang F*/
  	float temp_F = (temp_C * 9.0 / 5.0) + 32.0;
 
  	delay(1000); 
  
  	lcd.setCursor(0,0);          
  	lcd.print("Nhiet do:"); 
  	lcd.setCursor(0,1);           
  	lcd.print(temp_C);
  	lcd.setCursor(5,1);
  	lcd.print(char(178));
  	lcd.print("C");
  
  	Serial.println(sensor_pin);
}