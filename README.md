# DualMotor_Control_PWM
A basic Arduino Uno script to drive a H-bridge board for a power wheels Corvette

This all started with a Power Wheels Corvette and wanting to make it better. The stock speed control is just a foot switch and two switches in the shifter that connected the motors either in serial, parallel, or reverse serial. So the project expanded beyond a DeWalt battery to include:

- Variable resistance pedal from a Razor Cart (P/N W25143490043)
- Dual Motor H-Bridge board from eBay (search for Dual Motor Driver Board IRF3205 peak 30A 3V-36V H-Bridge)
- Arduino Uno
- Adafruit Proto Shield
- ATC Fuse Block

The code is build on a simple ISR timer counter to collect and manage different car functions. It reads the pedal and shifter position then sets the PWM outputs. It also tries to keep things safe by not allowing sudden changes in motor direction and forcing the controller into stop mode. It also reads the battery voltage to prevent driving with a dead battery since DeWalt batteries do not protect themselves. 
