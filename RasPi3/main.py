from gpiozero import LED
import serial

# This is the pin that the Pi will used to trigger an interrupt on the ESP
PERI_INTR_TRIG_PIN = LED("GPIO17")

serObj = serial.Serial('/dev/serial0', 9600, timeout=1)
serObj.write(b"Sent from Pi~")
peri_response = serObj.readline()
print(peri_response.decode('utf-8'))



