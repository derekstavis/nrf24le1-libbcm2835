# nrf24le1-libbcm2835
A simple command-line interface with Nordic nRF24LE1 using 
RaspberryPi over bcm2835 library.

This tool is mostly a userpace port of @holoturoide's nrf24le1 
linux device driver.

## Requirements
- You must be using Raspbian with `bcm2835` and `rt` libraries installed.
- In some cases this tool must be run as superuser 

## Roadmap
- Pass `da_test_show` test.
- Implement more extensive tests

## Features (in a distant future)
- Program memory read/write
- NVM memory read/write
- InfoPage handling

# References

* Nordic nRF24LE1:
<http://www.nordicsemi.com/eng/Products/2.4GHz-RF/nRF24LE1>
* bcm2835 Library: 
<http://www.airspayce.com/mikem/bcm2835/>
* @holoturoide's linux device driver: 
<https://bitbucket.org/erm/nrf24le1>
