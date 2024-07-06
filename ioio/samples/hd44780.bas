' HD44780 - Text LCD
' ==================
'
' This example demonstrates how to use the alphanumeric
' dot matrix liquid crystal display HD44780.
' 
' Connect the LCD to the IOIO-OTG board:
'
' -------         ------                   1 (GND)
'  IOIO  |       |HD44780                  |
'  PIN 41|-------|11 (DB4)                ---
'  PIN 42|-------|12 (DB5)               |   |
'  PIN 43|-------|13 (DB6)               |10K|<---3 (VEE)
'  PIN 44|-------|14 (DB7)               |   |
'  GND   |-------| 5 (RW)                 --- 
'  PIN 45|-------| 6 (E)                   |
'  PIN 46|-------| 4 (RS)                  2 (VIN)
'  GND   |-------| 1 (GND)
'  5V    |-------| 2 (VIN)
'--------         --------

' A potentiometer needs to be connected to the display
' to control the contrast of the display. If the display
' has a background light, connect power according to the
' data sheet to the pins 15 and 16.
'
' Code based on https://www.mikrocontroller.net/articles/AVR-GCC-Tutorial/LCD-Ansteuerung


'#########################################################
'# Constant definition                                   #
'#########################################################

const LCD_CLEAR_DISPLAY =       0x01
const LCD_CURSOR_HOME =         0x02

const LCD_SET_ENTRY =           0x04
const LCD_ENTRY_DECREASE =      0x00
const LCD_ENTRY_INCREASE =      0x02
const LCD_ENTRY_NOSHIFT =       0x00
const LCD_ENTRY_SHIFT =         0x01
 
const LCD_SET_DISPLAY =         0x08
const LCD_DISPLAY_OFF =         0x00
const LCD_DISPLAY_ON =          0x04
const LCD_CURSOR_OFF =          0x00
const LCD_CURSOR_ON =           0x02
const LCD_BLINKING_OFF =        0x00
const LCD_BLINKING_ON =         0x01
 
const LCD_SET_SHIFT =           0x10
const LCD_CURSOR_MOVE =         0x00
const LCD_DISPLAY_SHIFT =       0x08
const LCD_SHIFT_LEFT =          0x00
const LCD_SHIFT_RIGHT =         0x04
 
const LCD_SET_FUNCTION =        0x20
const LCD_FUNCTION_4BIT =       0x00
const LCD_FUNCTION_8BIT =       0x10
const LCD_FUNCTION_1LINE =      0x00
const LCD_FUNCTION_2LINE =      0x08
const LCD_FUNCTION_5X7 =        0x00
const LCD_FUNCTION_5X10 =       0x04
 
const LCD_SOFT_RESET =          0x30

const LCD_SET_CGADR =           0x40
 
const LCD_DDADR_LINE1 =         0x00
const LCD_DDADR_LINE2 =         0x40
const LCD_DDADR_LINE3 =         0x10
const LCD_DDADR_LINE4 =         0x50
 
const LCD_GC_CHAR0 =            0
const LCD_GC_CHAR1 =            1
const LCD_GC_CHAR2 =            2
const LCD_GC_CHAR3 =            3
const LCD_GC_CHAR4 =            4
const LCD_GC_CHAR5 =            5
const LCD_GC_CHAR6 =            6
const LCD_GC_CHAR7 =            7
 
const LCD_SET_DDADR =           0x80

'#########################################################
'# Main program                                          #
'#########################################################

import ioio

Print "Connect to HD44780"
RS  = ioio.openDigitalOutput(46)
E   = ioio.openDigitalOutput(45)
DB4 = ioio.openDigitalOutput(41)
DB5 = ioio.openDigitalOutput(42)
DB6 = ioio.openDigitalOutput(43)
DB7 = ioio.openDigitalOutput(44)
ioio.waitForConnect(10)
Print "Connection established"


Init()
LCD_Write("  Hello  World")
LCD_Locate(1,2)
LCD_Write("-= SmallBASIC =-")
print "Done"


end

'#########################################################
'# Functions and subs                                    #
'#########################################################

sub Init()

  RS.write(0)
  E.write(0)
  DB4.write(0)
  DB5.write(0)
  DB6.write(0)
  DB7.write(0)

  delay(50)

  ' Send soft-reset 3 time to initialize LCD
  Send4Bit(LCD_SOFT_RESET)
  delay(5) 
  SendEnable()
  delay(1)
  SendEnable()
  delay(1)

  ' Set 4-bit mode
  Send4Bit(LCD_SET_FUNCTION BOR LCD_FUNCTION_4BIT)
  delay(5)

  ' 2 lines and 5x7 pixel in 4 bit mode
  SendCommand(LCD_SET_FUNCTION BOR LCD_FUNCTION_4BIT BOR LCD_FUNCTION_2LINE BOR LCD_FUNCTION_5X7)
  ' Display on, cursor off and blinking off
  SendCommand(LCD_SET_DISPLAY BOR LCD_DISPLAY_ON BOR LCD_CURSOR_OFF BOR LCD_BLINKING_OFF)
  ' Cursor increment no scrolling
  SendCommand(LCD_SET_ENTRY BOR LCD_ENTRY_INCREASE BOR LCD_ENTRY_NOSHIFT )
  
  LCD_Cls()
end

sub LCD_Write(Text)
  ' Write characters to lcd
  local length, t
  length = len(Text)
  
  for i = 1 to length
    t = asc(mid(text, i, 1))
    SendData(t)
  next
end

sub LCD_Cls()
  SendCommand(LCD_CLEAR_DISPLAY)
  SendCommand(LCD_CURSOR_HOME)
end

sub LCD_Off()
  SendCommand(LCD_SET_DISPLAY BOR LCD_DISPLAY_OFF)
end

sub LCD_On()
  SendCommand(LCD_SET_DISPLAY BOR LCD_DISPLAY_ON)
end

sub LCD_Locate(x, y)
  local dat

  if(x < 1) then x == 1
  
  select case y
    case 1     ' 1. line
      dat = LCD_SET_DDADR + LCD_DDADR_LINE1 + x - 1
    case 2     ' 2. line
      dat = LCD_SET_DDADR + LCD_DDADR_LINE2 + x - 1
    case 3     ' 3. line
      dat = LCD_SET_DDADR + LCD_DDADR_LINE3 + x - 1
    case 4     ' 4. line
      dat = LCD_SET_DDADR + LCD_DDADR_LINE4 + x - 1
    case else
      return
  end select

  SendCommand(dat)
end

sub SendCommand(cmd)
  RS.write(0)
  SendByte(cmd)
end

sub SendData(dat)
  RS.write(1)
  SendByte(dat)
end

sub SendEnable()
  E.write(1)
  delay(1)
  E.write(0)
  delay(1)
end

sub SendByte(byte)
  Send4Bit(byte)          ' Send high bits first
  Send4Bit(byte lshift 4) ' Send low bits
end

sub Send4Bit(byte)
  DB7.write(GetBit(byte, 7))
  DB6.write(GetBit(byte, 6))
  DB5.write(GetBit(byte, 5))
  DB4.write(GetBit(byte, 4))
  SendEnable()  
end

func GetBit(value, bit)
  return (value rshift bit) BAND 1
end