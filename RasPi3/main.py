from gpiozero import LED
import serial
import subprocess
import threading
import time
from collections import deque

'''
    UART pins for RasPi
    Tx: GPIO 14 -- Physical pin 8
    Rx: GPIO 15 -- Physical pin 10
'''

# hackrf_sweep -f 778:786 -l 16 -1

# This is the pin that the Pi will used to trigger an interrupt on the ESP
PERI_INTR_TRIG_PIN = LED("GPIO17")

# result = subprocess.run(['hackrf_sweep', '-f', '778:782', '-l', '16', '-1'], capture_output=True, text=True)
# print(result.stdout)

serObj = serial.Serial('/dev/serial0', 115200, timeout=1)

class HackRFSweepReader:
    def __init__(self, sweep_args, buffer_size=1000):
        # deque with maxlen acts as a circular buffer. 
        # When it hits buffer_size items, adding a new item automatically removes the oldest.
        self.data_buffer:deque[str] = deque(maxlen=buffer_size)
        self.process = None
        self.thread = None
        self.is_running = False
        self.sweep_args = sweep_args

    def start(self):
        """Starts the hackrf_sweep process and the background reading thread."""
        self.is_running = True
        
        # Popen starts the process without blocking.
        # stdout=subprocess.PIPE allows us to read the output directly in memory.
        # text=True decodes the byte stream into string lines automatically.
        self.process = subprocess.Popen(
            ['hackrf_sweep'] + self.sweep_args,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, # Ignore stderr, or pipe it if you want errors
            text=True,
            bufsize=1 # Line buffered
        )
        
        if self.process is None:
            raise RuntimeError(
                "ERROR: HackRF backgrount process isn't working"
            )

        # Start the background daemon thread
        self.thread = threading.Thread(target=self._read_output, daemon=True)
        self.thread.start()

    def _read_output(self):
        """Continuously reads from the process stdout and pushes to the buffer."""
        # This loop will block on readline(), waiting for new data from hackrf_sweep
        for line in iter(self.process.stdout.readline, ''):
            if not self.is_running:
                break
            if line:
                self.data_buffer.append(line.strip())
                
        self.process.stdout.close()

    def get_latest_reading(self):
        """Returns the most recent 'num_lines' of data instantly.
        Returns:
            (str): The latest reading in string format.
        """
        # Convert the current state of the deque to a list and slice the end
        current_data = list(self.data_buffer)
        
        return current_data[-1] if len(current_data) else ""

    def get_latest_reading_list(self, num_lines=1):
        """Returns the most recent 'num_lines' of data instantly.

        Args:
            num_lines (int, optional): Specify how many lines of data to return. Defaults to 1.

        Returns:
            (list[str]): If num_lines == 1, will return the line in string format. If num_lines > 1, returns a list of lines in string format
        """
        # Convert the current state of the deque to a list and slice the end
        current_data = list(self.data_buffer)
        
        # Safeguard: If the buffer is completely empty, return empty values
        if not current_data:
            return "" if num_lines == 1 else []
        
        if (num_lines == 1):
            # The end of the list is the most recent item
            return current_data[-1]
        
        return current_data[-num_lines:]

    def stop(self):
        """Cleans up the process and thread."""
        self.is_running = False
        if self.process:
            self.process.terminate()
            self.process.wait()
            
def format_line_for_peri(raw_line:str):
    """Parses a single CSV string from hackrf_sweep into a dictionary.
    Returns empty string if the line is incomplete or malformed.

    Args:
        raw_line (str): A single reading line

    Returns:
        str: String properly formatted (ending in '~')
    """
    # Split the line and remove any trailing whitespace/newlines
    parts = [p.strip() for p in raw_line.split(',')]
    
    # A valid line must have at least the 6 metadata columns + 1 data point
    if len(parts) < 7:
        return "" 
        
    toReturn = ""
    try:
        # 1. Extract Metadata
        # date = parts[0]
        # toReturn += date + ","
        time = parts[1][:5]
        toReturn += time + ","
        
        # 2. Extract Power Data (dBFS)
        # We iterate from index 6 to the end, converting to float
        # 'if x' handles any trailing commas that might create empty strings
        # 6:10 b/c hackrf could potentially output more than 5 readings
        dbfs_values = [f"{float(x)}" for x in parts[6:10] if x]
        
        for val in dbfs_values:
            toReturn += val + ","
        toReturn = toReturn[:-1]
        
        toReturn += "~"
            
        return toReturn
        
    except (ValueError, IndexError):
        # Catch errors from partial lines written right as the process is stopped
        return ""
            
def main(scanner:HackRFSweepReader):
    # try:
        print("Starting HackRF sweep...")
        scanner.start()
        
        # Give it a second to spin up and fill the buffer; setup/calibration
        time.sleep(3) 
        
        print("Started")
        
        while(True):
            if serObj.in_waiting:
                # Decode and ignore garbage bytes
                userMsg = serObj.read_until(b'~').decode('utf-8', errors='ignore')
                print(f"Msg from peri: {userMsg}")
                
                # Replace the delimiter and aggressively strip hidden chars (\r, \n, spaces)
                userMsg = userMsg.replace('~', '').strip()
                
                if userMsg == "done":
                    scanner.stop()
                    exit()
                elif userMsg == "reading":
                    reading = format_line_for_peri(scanner.get_latest_reading())
                    if not reading:          
                        reading = "~"   # Send an empty terminator so ESP32 doesn't block!
                    print(f"Sending {reading}")
                    serObj.write(reading.encode())
                else:
                    print("Idk what that is")
            
    # except KeyboardInterrupt:
    #     print("\nKeyboard Interrupt. Stopping...")
    # finally:
    #     scanner.stop()

# --- Example Usage ---
if __name__ == "__main__":

    # time.sleep(2)
    # print("bouta send")
    # serObj.flush()
    # serObj.write(b"Sent from Pi~")
    # print("written")
    # peri_response = serObj.read_until(b"~")
    # print(peri_response.decode('utf-8')[:-1])
    
    # Example arguments: sweep from 2400MHz to 2500MHz
    args = ["-f", "778:786", "-l", "16"] 
    
    # Initialize the reader with a buffer of the last 500 lines
    scanner = HackRFSweepReader(sweep_args=args, buffer_size=500)
    
    main(scanner)

