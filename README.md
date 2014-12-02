# nrf24le1-libbcm2835
A simple command-line interface with Nordic nRF24LE1 using 
RaspberryPi over bcm2835 library.

This tool is mostly a userpace port of @holoturoide's nrf24le1 
linux device driver.

## Requirements
- You must be using Raspbian with `bcm2835` and `rt` libraries installed.
- In some cases this tool must be run as superuser 

## Features (in a distant future)
- Program memory read/write
- NVM memory read/write
- InfoPage handling

## Command Line Format

For further development, the tool should conform to the above protocol.

### Reading data from nRF24LE1

`nrf24le1 read infopage [filename]`

`nrf24le1 read firmware [filename]`

`nrf24le1 read nvm [filename]`

All read oprations dump data to stdout by default in Intel Hex format, it is
possible to provide an optional filename as an argument and it will be saved in
Intel Hex format if the suffix is .hex or .ihx and in binary format otherwise.

### Writing data to nRF24LE1

`nrf24le1 write firmware [filename]

`nrf24le1 write infopage [filename]`

`nrf24le1 write nvm [filename]`

The files are expected to be either in Intel Hex format if they have a suffix
of .hex or .ihx and binary format otherwise.

### Additional Parameters:

(not yet implemented)

| Parameter          | Function                      |
| ------------------ | ----------------------------- |
| `--offset N_BYTES` | Skips N_BYTES bytes           |
| `--count N_BYTES`  | Read/Write only N_BYTES bytes |

# References

* Nordic nRF24LE1:
<http://www.nordicsemi.com/eng/Products/2.4GHz-RF/nRF24LE1>
* bcm2835 Library: 
<http://www.airspayce.com/mikem/bcm2835/>
* @holoturoide's linux device driver: 
<https://bitbucket.org/erm/nrf24le1>
