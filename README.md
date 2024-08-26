This projects makes the discovery board behave like a usb devices that accepts 48khz ditigal audio and outputs it via the SAI1 module in the form of I2S.
The I2S has a sample rate of 96kHz and a bit depth of 32 bit's.

The project is also in charge of programming an esstech es9017s dac that lives on the I2C bus at adress 0x90.

Lastly the touchscreen is used to set the volume of the individual channels or just L/R volume. 
In L/R mode the even dac's are controlled with the left slider and the uneven dac's are controlled trough the right slider (start counting from 1). 
The L/R channels are lockable with the push of a button.
