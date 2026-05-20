from gpiozero import LED
import serial

# This is the pin that the Pi will used to trigger an interrupt on the ESP
PERI_INTR_TRIG_PIN = LED("GPIO17")

serObj = serial.Serial('/dev/serial0', 9600, timeout=1)
print("bouta send")
serObj.write(b"Sent from Pi~")
# print("written")
peri_response = serObj.read_until(b"~")
print(peri_response.decode('utf-8')[:-1])



