# IoT-PowerLoss_M5StickC

Check for power loss and send alerts using AWS IoT using a M5StickC device.

### Requirements
- M5StickC device -  [https://m5stack.com/products/stick-c](https://m5stack.com/products/stick-c)
- AWS Account with IoT Core service setup

### Setup

First you need to rename secrets_orig.h to secrets.h and edit the file to include your data, such as local WiFi credentials, AWS IoT endpoint (based on your region) and device certificates (provided by AWS when adding a device in AWS IoT Core).

Load the project in Arduino Studio and connect your M5StickC to your computer using the USB cable and deploy your code.
