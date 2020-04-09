# ESP8266 with AWS IoT Device Shadows

This repository manages an ESP8266 with a Digital Humidity and Temperature (DHT) sensor attached. It takes a reading every minute and publishes the sensor's state to an AWS IoT Device Shadow. The shadow can then be used to trigger events using the AWS IoT rules engine.

## Provisioning
### AWS Provisioning
1. Create a new AWS IoT Thing
    * (Optional) Create/assign a Thing Type
2. 1-click generate certificates
3. Download CA, cert, and private key files.
4. [Encode your files in .der format](https://thomasphorton.com/2020/03/25/Using-X-509-Certificates-with-ESP8266s/)

### Device Provisioning
1. Create a config file in `/data` based off of the example.
2. Copy DER encoded CA, cert, and private key files to `/data`.
3. In platformio, run the `Upload file system image` task to copy `/data` to the device file system.
4. Use platformio to upload the repository code to your device.