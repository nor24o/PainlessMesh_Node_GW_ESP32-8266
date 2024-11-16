## NODE
  Node has basic functions to send data as a json string 
    -It can accept command's from GW

## GW   
  Forwards the data received to the MQTT server 
  Able to send Unicas or Multicast command 
    - Unicast
      topic:
        painlessMesh/to/986968584 
      payload: Must be String!
        0,1;1,0;2,0      => IO-I1=1,IO-I2=0,IO-I2=0
### The GW can receive any data what is sent to it  from node but preferably should use the incorporated one 
      
