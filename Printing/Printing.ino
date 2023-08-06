#include <LiquidCrystal.h>// library for LCD

LiquidCrystal lcd(12, 11, 4, 5, 6, 7);



byte ch;
int col=0;
int row=0;

void setup() 
{
  	Serial.begin(9600);
  	lcd.begin(16,2);
  	lcd.clear();
}

void loop() 
{
  	if(Serial.available())
  	{
    	char ch=Serial.read();
    	Serial.write(ch);
    	Serial.println();
    	lcd.setCursor(col,row);
    	lcd.write(ch);
    	col++;
      
  		if(col>15)
        {
    		row++;
		    col=0;
    		lcd.write(ch);
  		}
	}
  
	if(ch=='*' ||row==1&&col>=15)
    {
  		lcd.clear();
  		col=0;
		row=0;
	}
}
