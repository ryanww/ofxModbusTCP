ofxModbusTCP
============

Modbus TCP Communications between industrial Automation Controllers 

Most industrial controllers use Modbus for device communication by allowing other 
devices to access Coils and Registers. Modbus typically is an RS485 based network which
uses Modbus RTU or Modbus Ascii to communicate. Now, it is much faster to communicate
over TCP. 

You can read the specifics of it here: http://en.wikipedia.org/wiki/Modbus

<bold>Usage</bold>

In your main .h file, add the following line to the header file:
<code> #include "ofxModbusTCP.h"</code>

Then create one object per "master" connection by using the following:
<code>ofxModbusTCPClient mb;</code>

Now in your .cpp file in the setup function, you are going to enter in the following
to match your environment:

<code>
//Enables Debug Messaging
    mb.setDebug(true); //Enables Debug Messaging
    
    //Setup Modbus (optional int after IP i
    mb.setup("10.0.1.254");
    
    //Start Connection
    mb.connect();
    
    //to Disconnect, call mb.disconnect() or mb.setEnabled(false)
</code>


