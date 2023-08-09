import serial 
import argparse
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.lines as lines

# Static Vars

fig, ax = plt.subplots()
ln, = ax.plot([], [], lw=2)
xs, ys = [], []

serial_port = serial.Serial("COM8",115200)

# ADC Setup

# Determined by PGA, look at datasheet for alpha/beta
PGA = 16
alpha = 4194304
beta = 1.8639

v_ref = 2.490
R = 47000

ofc = 0
fsc = 0

# Vin = DAC_2V - SIG_OUT
def raw_to_sig_out(raw, ofc, fsc):
  return 2 - (((raw / (fsc * beta)) + (ofc/alpha)) * (2 * v_ref) / PGA)

def raw_to_current(raw, ofc, fsc):
    return raw_to_sig_out(raw, ofc, fsc) / R 

def init():
    ln.set_data([], [])
    return ln,

# Specific to DPV (Changing voltage suddenly)
def mimic_voltage(xs, ys, start_index):
  for i in range(start_index, len(xs)):
    # shift the time of the previous voltage value to the same location
    # insert a value of the previous voltage at the same time
    # vertical jump versus slope
    if ys[i] != ys[i-1]:
        xs[i-1] = xs[i]
  return xs, ys

# This function is called periodically from FuncAnimation
def dac_animate(i):
    # Split line by comma
    output = serial_port.readline().decode()
    if (output.find(",") == -1):
        return ln,

    time, value = output.split(",")

    # Add x and y to lists
    xs.append(int(time) / 10**6)
    ys.append(2 - float(value))
    if (len(xs) > 1):
        mimic_voltage(xs, ys, len(xs)-1)
    ln.set_data(xs, ys)
    return ln,
    

def dac_script():
    ax.set_xlim(0, 40)
    ax.set_ylim(-0.5, 0)

    ani = animation.FuncAnimation(fig, dac_animate, init_func=init, interval=10, blit=True)
    
    plt.title('DAC Command Voltage vs Time')
    plt.ylabel('Voltage (V)')
    plt.xlabel("Time (s)")
    plt.show()

# This function is called periodically from FuncAnimation
def adc_animate(i):
    # Split line by comma
    output = serial_port.readline().decode()
    if (output.find(",") == -1):
        return ln,

    time, value = output.split(",")

    # Add x and y to lists
    xs += int(time) / 10**6
    ys += raw_to_sig_out(float(value), ofc, fsc)

    ln.set_data(xs, ys)
    return ln,

def adc_script():
    # First line is calibration
    output = serial_port.readline().decode()
    values = output.split(",")
    ofc = int(values[0])
    fsc = int(values[1])

    ani = animation.FuncAnimation(fig, adc_animate, init_func=init, interval=10, blit=True)
    
    plt.title('ADC Voltage vs Time')
    plt.ylabel('Voltage (V)')
    plt.xlabel("Time (s)")
    plt.show()

def debug():
    print("opened port")
    while (True):
        output = serial_port.readline().decode()
        print(output)
        

def main():
    dac_script()

if __name__ == "__main__":
    main()
