#pragma once

#include "ofMain.h"
#include "ofxNetwork.h"
#include "modbusSlave.h"

typedef unsigned short  WORD;
#define LOWBYTE(v)   ((unsigned char) (v))
#define HIGHBYTE(v)  ((unsigned char) (((unsigned int) (v)) >> 8))

class ofxModbusTcpClient : public ofBaseApp {
public:
    
    void setup(string _ip, int _numberOfSlaves);
    void setup(string _ip);
    
    void connect();
    void disconnect();
    
    void update();
    
    bool active = false;
    
    bool enabled = false;
    void setEnabled(bool _enabled);
	
    bool debug = false;
    void setDebug(bool _debug);
    
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
    
    
protected:
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
    
    //Debug
    void sendDebug(string _msg);
    
    //tools
    WORD convertToWord(WORD _h, WORD _l);
    int getTransactionID();
    int lastTransactionID = 0;
    int lastFunctionCode = 0;
    unsigned char ToByte(bool b[8]);
    
};

