/**********************************************************************************
 
 Copyright (C) 2014 Ryan Wilkinson - Wired Media Solutions (www.wiredms.com)
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in
 the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do
 so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 
 **********************************************************************************/


#pragma once

#include "ofMain.h"
#include "ofxNetwork.h"
#include "modbusSlave.h"
#include "ofEvents.h"

typedef unsigned short  WORD;
#define LOWBYTE(v)   ((unsigned char) (v))
#define HIGHBYTE(v)  ((unsigned char) (((unsigned int) (v)) >> 8))

struct mbCommand{
    vector<uint8_t> msg;
    WORD length;
    long timeAdded;
    string debugString;
};

class ofxModbusTcpClient : public ofThread {
public:
    
    void setup(string _ip, int _numberOfSlaves);
    void setup(string _ip);
    
    void connect();
    void disconnect();
    
    
    bool active = false;
    
    bool enabled = false;
    void setEnabled(bool _enabled);
	
    bool debug = false;
    void setDebugEnabled(bool _enabled);
    
    //Slave Updates
    void updateCoils(int _id, int _startAddress, int _qty);
    void updateRegisters(int _id, int _startAddress, int _qty);
    
    //Slave Writes
    void writeCoil(int _id, int _startAddress, bool _newValue);
    void writeRegister(int _id, int _startAddress, int _newValue);
    void writeMultipleCoils(int _id, int _startAddress, vector<bool> _newValues);
    void writeMultipleRegisters(int _id, int _startAddress, vector<int> _newValues);
    
    //Slave Read Coils
    bool getCoil(int _id, int _startAddress);
    int getRegister(int _id, int _startAddress);
    
    bool connected = false;
protected:
    
    //void update(ofEventArgs & args);
    void threadedFunction();
    
    //Slave Addresses & Slave Variables
    int numberOfSlaves = 1;
    void setupSlaves();
    vector<slave *> slaves; // vector of all the slave addresses
    
    //TCP Coms
    ofxTCPClient tcpClient;
    int PreviousActivityTime = 0;
    float counter = 0.0f;
    int connectTime = 0;
    int deltaTime = 0;
    bool weConnected = 0;
    string ip = "";
    int port = 502;
    
    //Commands
    vector<mbCommand> commandToSend;
    void sendNextCommand();
    int getTransactionID();
    int lastTransactionID = 0;
    int lastFunctionCode = 0;
    int lastStartingReg = 0;
    bool waitingForReply = false;
    
    //Debug
    void sendDebug(string _msg);
    
    //tools
    WORD convertToWord(WORD _h, WORD _l);
    unsigned char ToByte(bool b[8]);
    
};

