# Waterball-RGB

This repo has the code for a satellite sensor that the Waterball uses, with an RGB sensor and a light sensor.

## PubSubClient modification

You'll need to increase the maximum packet size for PubSubClient. After it's installed, edit `.piolibs/PubSubClient_ID89/src/PubSubClient.h`, find the line that defines `MQTT_MAX_PACKET_SIZE` and change its value to 300.

## LICENSE

This code is licensed under the [MIT License](https://romkey.mit-license.org).
