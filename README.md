# ESP32-Nextion-OTA
Perform an OTA update for Nextion display over Internet.

## Usage
An example I made for doing an OTA update for a Nextion display using an API endpoint.
The API endpoint that is responsible for sending the firmware file is beyond the scope of this example.

I based the code loosely on the Nextion Arduino Upload code. 

Since there was no library for the ESP32 IDF, I decided to write my own. Maybe it's helpful to someone.

The code uses the esp_http_library for making the API call. 

I compiled this verion against v3.2 for the ESP IDF
