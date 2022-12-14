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


#include "ofxModbusTcpClient.h"

//Main
#pragma mark MAIN
ofxModbusTcpClient::ofxModbusTcpClient(){
    numOfCoils = 5000;
    numOfRegisters = 5000;
    
    PreviousActivityTime = 0;
    counter = 0.0f;
    connectTime = 0;
    deltaTime = 0;
    weConnected = false;
    ip = "";
    port = 502;
    numberOfSlaves = 1;
    
    lastTransactionID = 0;
    lastFunctionCode = 0;
    lastStartingReg = 0;
    waitingForReply = false;
    
    lastSentCommand = new mbCommand;
}
ofxModbusTcpClient::~ofxModbusTcpClient(){
    stopThread();
    for (auto &cmd : commandToSend){
        delete cmd;
    }
    commandToSend.clear();
    
    for (auto &sl : slaves){
        delete sl;
    }
    slaves.clear();
}


//Setup
#pragma mark SETUP
void ofxModbusTcpClient::setup(string _ip, int _numberOfSlaves) {
    ip = _ip;
    numberOfSlaves = _numberOfSlaves;
    if (numberOfSlaves == 0 || numberOfSlaves > 247) { numberOfSlaves = 1; }
    ofLogVerbose("ofxModbusTCP IP:"+ip)<<"Setup with "<<numberOfSlaves<<" slaves";
    setupSlaves();
}

void ofxModbusTcpClient::setup(string _ip) {
    ip = _ip;
    numberOfSlaves = 1;
    ofLogVerbose("ofxModbusTCP IP:"+ip)<<"Setup with "<<numberOfSlaves<<" slaves";
    setupSlaves();
}
void ofxModbusTcpClient::setup(string _ip, int _numberOfSlaves, int _port) {
    ip = _ip;
    port = _port;
    numberOfSlaves = _numberOfSlaves;
    if (numberOfSlaves == 0 || numberOfSlaves > 247) { numberOfSlaves = 1; }
    ofLogVerbose("ofxModbusTCP IP:"+ip)<<"Setup with "<<numberOfSlaves<<" slaves";
    setupSlaves();
}
void ofxModbusTcpClient::setupSlaves() {
    for (int i = 0; i<=numberOfSlaves; i++) {
        slaves.push_back(new slave(i+1, numOfCoils, numOfRegisters));
        slaves.back()->setmasterIP(ip);
    }
}

//Connection
#pragma mark CONNECTION
void ofxModbusTcpClient::connect() {
    if (!weConnected){
        enabled = true;
        try {
            weConnected = tcpClient.setup(ip, port);
        } catch (...) {
            ofLogError("ofxModbusTCP IP:"+ip)<<"Couldn't connect to Modbus Master at "<<ip;
        }
        connectTime = 0;
        deltaTime = 0;
        
        if (!isThreadRunning())startThread();
        
        ofLogVerbose("ofxModbusTCP IP:"+ip)<<"Connect - Status: "<<weConnected;
    } else {
        ofLogVerbose("ofxModbusTCP IP:"+ip)<<"Already connected - ignoring1";
    }
}
void ofxModbusTcpClient::disconnect() {
    if (enabled && tcpClient.isConnected()){
        tcpClient.close();
    }
    enabled = false;
    weConnected = false;
    
    for (auto c : this->commandToSend){
        delete c;
    }
    commandToSend.clear();
    ofLogVerbose("ofxModbusTCP IP:"+ip)<<"Disconnect";
}
bool ofxModbusTcpClient::isConnected(){
    if (enabled){
        return weConnected;
    } else {
        return false;
    }
}

//Enables
#pragma mark ENABLES
void ofxModbusTcpClient::setEnabled(bool _enabled) { enabled = _enabled; }
void ofxModbusTcpClient::sendDebug(string _msg) {
    if (debug){
        ofLogVerbose("ofxModbusTCP IP:"+ip)<<_msg;
    }
}
void ofxModbusTcpClient::setDebugEnabled(bool _enabled){
    debug = _enabled;
}

//Running Function
#pragma mark RUNNING FUNCTION
void ofxModbusTcpClient::threadedFunction(){
    while (isThreadRunning()){
        for (auto &s : slaves){
            s->setDebugEnable(debug);
        }
        
        int transactionID = 0;
        int lengthPacket = 0;
        uint8_t headerReply[2000];
        
        if (enabled) {
            if (weConnected) {
                //Check for socket error condition..
                if (!tcpClient.isConnected()){
                    weConnected = false;
                }
                
                //Read from TCP
                if (tcpClient.receiveRawBytes((char *)headerReply, 2000) > 0) { //Grab Header
                    int transID = convertToWord(headerReply[0], headerReply[1]);
                    int protocolID = convertToWord(headerReply[3], headerReply[4]);
                    
                    //Ignore if not last transaction ID
                    if (transID != lastTransactionID) {
                        ofLogError("ofxModbusTCP IP:"+ip)<<"Transaction ID Mismatch, discarding Reply. Got: "<<ofToHex(transID)<<" expected:"<<ofToHex(lastTransactionID);
                        return;
                    }
                    
                    //Ignore if not correct protocol
                    if (headerReply[3] != 0x00 || headerReply[4] != 0x00) {
                        ofLogError("ofxModbusTCP IP:"+ip)<<"Invalid Protocol ID, discarding reply. Got: "<<ofToHex(headerReply[3])<<" "<<ofToHex(headerReply[4]);
                        return;
                    }
                    
                    int functionCode = headerReply[7];
                    if (functionCode != lastFunctionCode) { //There was an error or exception code
                        if (functionCode == 0x80 + lastFunctionCode){
                            int exceptionCode = headerReply[8];
                            switch (headerReply[8]){
                                case 0x01: {
                                    ofLogError("ofxModbusTCP IP:"+ip)<<"Function Code Error received - exception code = Illegal Function";
                                    break;
                                }
                                case 0x02: {
                                    ofLogError("ofxModbusTCP IP:"+ip)<<"Function Code Error received - exception code = Illegal Data Address";
                                    break;
                                }
                                case 0x03: {
                                    ofLogError("ofxModbusTCP IP:"+ip)<<"Function Code Error received - exception code = Illegal Data Value";
                                    break;
                                }
                                case 0x04: {
                                    ofLogError("ofxModbusTCP IP:"+ip)<<"Function Code Error received - exception code = Slave Device Failure";
                                    break;
                                }
                                case 0x05: {
                                    ofLogNotice("ofxModbusTCP IP:"+ip)<<"Function Code Error received - exception code = Acknowledge";
                                    break;
                                }
                                case 0x06: {
                                    ofLogError("ofxModbusTCP IP:"+ip)<<"Function Code Error received - exception code = Slave Device Busy";
                                    break;
                                }
                                case 0x07: {
                                    ofLogError("ofxModbusTCP IP:"+ip)<<"Function Code Error received - exception code = Negative Acknowledge";
                                    break;
                                }
                                case 0x08: {
                                    ofLogError("ofxModbusTCP IP:"+ip)<<"Function Code Error received - exception code = Memory Parity Error";
                                    break;
                                }
                                case 0x10: {
                                    ofLogError("ofxModbusTCP IP:"+ip)<<"Function Code Error received - exception code = Gateway Path Unavailable";
                                    break;
                                }
                                case 0x11: {
                                    ofLogError("ofxModbusTCP IP:"+ip)<<"Function Code Error received - exception code = Gateway Target Device Failed to Respond";
                                    break;
                                }
                                default: {
                                    ofLogError("ofxModbusTCP IP:"+ip)<<"Function Code Error received - exception code unknown: "<<exceptionCode;
                                    break;
                                }
                            }
                        } else {
                            ofLogError("ofxModbusTCP IP:"+ip)<<"Function Code Mismatch, discarding reply - Should be "<<lastFunctionCode<<", got "<<functionCode;
                        }
                    }
                    
                    int originatingID = headerReply[6];
                    int lengthData = convertToWord(headerReply[4], headerReply[5]);
                    
                    switch (functionCode) {
                        case 1: {
                            int byteCount = headerReply[8];
                            vector<char> nc;
                            vector<bool> nv;
                            for (int i=9; i<9+byteCount; i++){ nc.push_back(headerReply[i]); }
                            int startAddr = 1+convertToWord(lastSentCommand->msg[8], lastSentCommand->msg[9]);
                            int qty = convertToWord(lastSentCommand->msg[10], lastSentCommand->msg[11]);
                            sendDebug("Reply Received, Reading Multiple Coils - Qty:"+ofToString(qty)+" starting at:"+ofToString(startAddr));
                            if ((nc.size()*8) >= qty ){
                                for (int i=0; i<nc.size(); i++){
                                    bitset<8> bs(nc[i]);
                                    if ((i*8)+1 <= qty){ nv.push_back(bs[0]); }
                                    if ((i*8)+2 <= qty){ nv.push_back(bs[1]); }
                                    if ((i*8)+3 <= qty){ nv.push_back(bs[2]); }
                                    if ((i*8)+4 <= qty){ nv.push_back(bs[3]); }
                                    if ((i*8)+5 <= qty){ nv.push_back(bs[4]); }
                                    if ((i*8)+6 <= qty){ nv.push_back(bs[5]); }
                                    if ((i*8)+7 <= qty){ nv.push_back(bs[6]); }
                                    if ((i*8)+8 <= qty){ nv.push_back(bs[7]); }
                                }
                                
                                slaves.at(originatingID-1)->setMultipleCoils(startAddr, nv);
                            } else {
                                sendDebug("Error - Quantity doesn't match up.");
                            }
                        } break;
                        case 3: {
                            sendDebug("Reply Received, Setting Multiple Registers");
                            int byteCount = headerReply[8];
                            int currentByte = 9;
                            for (int i=0; i<(byteCount/2); i++) {
                                int newVal = convertToWord(headerReply[currentByte], headerReply[currentByte+1]);
                                slaves.at(originatingID-1)->setRegister(i, newVal);
                                currentByte = currentByte + 2;
                            }
                        } break;
                        case 5: {
                            int address = convertToWord(headerReply[8], headerReply[9])+1;
                            bool t;
                            if (headerReply[10] == 0xff) {t=true;} else {t=false;}
                            sendDebug("Reply Received, Setting Single Coil at Address "+ofToString(address)+" to value "+ofToString(t));
                            slaves.at(originatingID-1)->setCoil(address, t);
                        } break;
                        case 6: {
                            int address = convertToWord(headerReply[8], headerReply[9])+1;
                            sendDebug("Reply Received, Setting Single Register at Address "+ofToString(address)+" to value "+ofToString(convertToWord(headerReply[10], headerReply[11])));
                            slaves.at(originatingID-1)->setRegister(address, convertToWord(headerReply[10], headerReply[11]));
                        } break;
                        case 15: {
                            int repAddress = convertToWord(headerReply[8], headerReply[9])+1;
                            int repQtyOfCoils = convertToWord(headerReply[10], headerReply[11]);
                            
                            //Check Address
                            if (lastSentCommand->msg[8] != headerReply[8] || lastSentCommand->msg[9] != headerReply[9]){
                                //Address reply incorrect
                                sendDebug("Error - Address doesn't match up.");
                                break;
                            }
                            
                            //Check Coil Qty
                            if (lastSentCommand->msg[10] != headerReply[10] || lastSentCommand->msg[11] != headerReply[11]){
                                //Coil qty wrong
                                sendDebug("Error - Coil Quantity doesn't match up.");
                                break;
                            }
                            
                            //Load Data
                            int curByte = 13;
                            int curBit = 0;
                            int totalBytes = lastSentCommand->msg[12];

                            for (int i=0; i<totalBytes; i++){
                                bitset<8> bs (lastSentCommand->msg[curByte+i]);
                                for (int b=0; b<8; b++){
                                    if (curBit < repQtyOfCoils){
                                        slaves.at(originatingID-1)->setCoil(curBit+repAddress, bs[curBit]);
                                        curBit++;
                                    } else {
                                        b = 8;
                                    }
                                }
                            }
                            sendDebug("Reply Received, Setting "+ofToString(repQtyOfCoils)+" Coils at address: "+ofToString(repAddress)+" for slave "+ofToString(originatingID-1));
                        } break;
                        case 16: {
                            sendDebug("Reply Received, Setting Multiple Registers - not supported yet");
                        } break;
                    }
                    waitingForReply = false;
                }
                
                //Send next command
                if (!waitingForReply && weConnected){
                    sendNextCommand();
                }
                
                //Activity
                if(!active && weConnected) {
                    active = true;
                    PreviousActivityTime = ofGetElapsedTimeMillis();
                }
                if(ofGetElapsedTimeMillis() > (PreviousActivityTime + 60000)) {
                    if(active) {
                        active = false;
                    }
                }
                
            } else if (!weConnected){
                //if we are not connected lets try and reconnect every 5 seconds
                deltaTime = ofGetElapsedTimeMillis() - connectTime;
                
                if( deltaTime > 5000 ){
                    connect();
                    connectTime = ofGetElapsedTimeMillis();
                }
            }
        } else {
            if (tcpClient.isConnected()) {
                tcpClient.close();
                weConnected = false;
            }
        }
    }
}

//Slave Get Values
#pragma mark SLAVE GETS
bool ofxModbusTcpClient::getCoil(int _id, int _startAddress) {
    if (enabled) {
        if (_id>0 && _id < slaves.size() && _startAddress < numOfCoils) {
            return slaves.at(_id-1)->getCoil(_startAddress);
        } else {
            return false;
        }
    } else {
        return false;
    }
}
int ofxModbusTcpClient::getRegister(int _id, int _startAddress) {
    if (enabled ) {
        if (_id>0 && _id < slaves.size() && _startAddress < numOfRegisters) {
            return slaves.at(_id-1)->getRegister(_startAddress);
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

//Slave Read Updates
#pragma mark SLAVE READ UPDATES
void ofxModbusTcpClient::updateCoils(int _id, int _startAddress, int _qty) {
    if (enabled) {
        if (_id<=numberOfSlaves && _id>0 && _qty < numOfCoils) {
            uint8_t localByteArray[12];
            
            WORD length = 6;
            WORD start = _startAddress;
            WORD qty = _qty;
            
            localByteArray[0] = HIGHBYTE(0); //Transaction High
            localByteArray[1] = LOWBYTE(0); //Transaction Low
            localByteArray[2] = 0x00; //Protocal Identifier High
            localByteArray[3] = 0x00; //Protocol Identifier Low
            localByteArray[4] = HIGHBYTE(length); //Length High
            localByteArray[5] = LOWBYTE(length); //Length Low
            localByteArray[6] = _id; //Unit Idenfifier
            localByteArray[7] = 0x01; //Function Code
            localByteArray[8] = HIGHBYTE(start-1); //Start Address High
            localByteArray[9] = LOWBYTE(start-1); //Start Address Low
            localByteArray[10] = HIGHBYTE(qty); //Qty High
            localByteArray[11] = LOWBYTE(qty); //Qty Low
            
            lastStartingReg = start;
        
            stringstream dm;
            dm<<"Reading Coils of "<<_id<<" Start:"<<_startAddress<<" Qty:"<<_qty;
            
            vector<uint8_t> lba;
            int sizeOfArray = sizeof(localByteArray) / sizeof(localByteArray[0]);
            for (int i=0; i<sizeOfArray; i++){
                lba.push_back(localByteArray[i]);
            }
            
            mbCommand * cmd = new mbCommand();
            cmd->msg = lba;
            cmd->length = length;
            cmd->timeAdded = ofGetElapsedTimeMillis();
            cmd->debugString = dm.str();
            commandToSend.push_back(cmd);
        } else {
            ofLogError("ofxModbusTCP IP:"+ip)<<"Reding Coils Parameters Are Out Of Range";
        }
    }
}
void ofxModbusTcpClient::updateRegisters(int _id, int _startAddress, int _qty) {
    if (enabled) {
        if (_id<=numberOfSlaves && _id > 0 && _qty < numOfRegisters) {
            uint8_t localByteArray[12];
            
            WORD tx = getTransactionID();
            WORD length = 6;
            WORD start = _startAddress;
            WORD qty = _qty;
            
            localByteArray[0] = HIGHBYTE(tx); //Transaction High
            localByteArray[1] = LOWBYTE(tx); //Transaction Low
            localByteArray[2] = 0x00; //Protocal Identifier High
            localByteArray[3] = 0x00; //Protocol Identifier Low
            localByteArray[4] = HIGHBYTE(length); //Length High
            localByteArray[5] = LOWBYTE(length); //Length Low
            localByteArray[6] = _id; //Unit Idenfifier
            localByteArray[7] = 0x03; //Function Code
            localByteArray[8] = HIGHBYTE(start-1); //Start Address High
            localByteArray[9] = LOWBYTE(start-1); //Start Address Low
            localByteArray[10] = HIGHBYTE(qty); //Qty High
            localByteArray[11] = LOWBYTE(qty); //Qty Low
            
            lastStartingReg = start;
            
            stringstream dm;
            dm<<"Reading Registers of "<<_id<<" Start:"<<_startAddress<<" Qty:"<<_qty;
            
            vector<uint8_t> lba;
            int sizeOfArray = sizeof(localByteArray) / sizeof(localByteArray[0]);
            for (int i=0; i<sizeOfArray; i++){
                lba.push_back(localByteArray[i]);
            }
            
            mbCommand * cmd = new mbCommand();
            cmd->msg = lba;
            cmd->length = length;
            cmd->timeAdded = ofGetElapsedTimeMillis();
            cmd->debugString = dm.str();
            commandToSend.push_back(cmd);
        } else {
            ofLogError("ofxModbusTCP IP:"+ip)<<"Read Registers Parameters Are Out Of Range";
        }
    }
}

//Slave Writes
#pragma mark SLAVE WRITES
void ofxModbusTcpClient::writeCoil(int _id, int _startAddress, bool _newValue) {
    if (enabled) {
        if (_id<=numberOfSlaves && _id>0 && _startAddress < numOfCoils) {
            uint8_t localByteArray[12];
            
            WORD length = 6;
            WORD start = _startAddress;
            
            localByteArray[0] = HIGHBYTE(0); //Transaction High
            localByteArray[1] = LOWBYTE(0); //Transaction Low
            localByteArray[2] = 0x00; //Protocal Identifier High
            localByteArray[3] = 0x00; //Protocol Identifier Low
            localByteArray[4] = HIGHBYTE(length); //Length High
            localByteArray[5] = LOWBYTE(length); //Length Low
            localByteArray[6] = _id; //Unit Idenfifier
            localByteArray[7] = 0x05; //Function Code
            localByteArray[8] = HIGHBYTE(start-1); //Start Address High
            localByteArray[9] = LOWBYTE(start-1); //Start Address Low
            
            //New Value
            if (_newValue) {
                localByteArray[10] = 0xff; //New Value High
                localByteArray[11] = 0x00; //New Value Low
            } else {
                localByteArray[10] = 0x00; //New Value High
                localByteArray[11] = 0x00; //New Value Low
            }
            
            stringstream dm;
            dm<<"Writing Coil of "<<_id<<" Address:"<<_startAddress<<" Value:"<<_newValue;
            
            vector<uint8_t> lba;
            int sizeOfArray = sizeof(localByteArray) / sizeof(localByteArray[0]);
            for (int i=0; i<sizeOfArray; i++){
                lba.push_back(localByteArray[i]);
            }
            
            mbCommand * cmd = new mbCommand();
            cmd->msg = lba;
            cmd->length = length;
            cmd->timeAdded = ofGetElapsedTimeMillis();
            cmd->debugString = dm.str();
            commandToSend.push_back(cmd);
        } else {
            ofLogError("ofxModbusTCP IP:"+ip)<<"Write Coil Parameters Are Out Of Range";
        }
    }
}
void ofxModbusTcpClient::writeRegister(int _id, int _startAddress, int _newValue) {
    if (enabled) {
        if (_id<=numberOfSlaves && _id>0 && _startAddress < numOfRegisters) {
            uint8_t localByteArray[12];
            
            WORD length = 6;
            WORD start = _startAddress;
            WORD newVal = _newValue;
            
            localByteArray[0] = HIGHBYTE(0); //Transaction High
            localByteArray[1] = LOWBYTE(0); //Transaction Low
            localByteArray[2] = 0x00; //Protocal Identifier High
            localByteArray[3] = 0x00; //Protocol Identifier Low
            localByteArray[4] = HIGHBYTE(length); //Length High
            localByteArray[5] = LOWBYTE(length); //Length Low
            localByteArray[6] = _id; //Unit Idenfifier
            localByteArray[7] = 0x06; //Function Code
            localByteArray[8] = HIGHBYTE(start-1); //Start Address High
            localByteArray[9] = LOWBYTE(start-1); //Start Address Low
            localByteArray[10] = HIGHBYTE(newVal); //New Value High
            localByteArray[11] = LOWBYTE(newVal); //New Value Low
            
            stringstream dm;
            dm<<"Writing Register to "<<_id<<" Address:"<<_startAddress<<" Value:"<<_newValue;
            
            vector<uint8_t> lba;
            int sizeOfArray = sizeof(localByteArray) / sizeof(localByteArray[0]);
            for (int i=0; i<sizeOfArray; i++){
                lba.push_back(localByteArray[i]);
            }
            
            mbCommand * cmd = new mbCommand();
            cmd->msg = lba;
            cmd->length = length;
            cmd->timeAdded = ofGetElapsedTimeMillis();
            cmd->debugString = dm.str();
            commandToSend.push_back(cmd);
        } else {
            ofLogError("ofxModbusTCP IP:"+ip)<<"Write Register Parameters Are Out Of Range";
        }
    }
}
void ofxModbusTcpClient::writeMultipleCoils(int _id, int _startAddress, vector<bool> _newValues) {
    if (enabled) {
        if (_id<=numberOfSlaves && _id>0 && (_startAddress+_newValues.size()) < numOfCoils) {
            int totalBytes = (_newValues.size()+7)/8;
            int length = 7 + totalBytes;
            uint8_t localByteArray[7 + length];
            
            localByteArray[0] = HIGHBYTE(0); //Transaction High
            localByteArray[1] = LOWBYTE(0); //Transaction Low
            localByteArray[2] = 0x00; //Protocal Identifier High
            localByteArray[3] = 0x00; //Protocol Identifier Low
            localByteArray[4] = HIGHBYTE(length); //Length High
            localByteArray[5] = LOWBYTE(length); //Length Low
            localByteArray[6] = _id; //Unit Idenfifier
            localByteArray[7] = 0x0f; //Function Code
            localByteArray[8] = HIGHBYTE(_startAddress-1); //Start Address High
            localByteArray[9] = LOWBYTE(_startAddress-1); //Start Address Low
            localByteArray[10] = HIGHBYTE(_newValues.size()); //Quantity of Coils High
            localByteArray[11] = LOWBYTE(_newValues.size()); //Quantity of Coils Low
            localByteArray[12] = totalBytes; //Number of bytes
            
            stringstream dm;
            dm<<"Writing Multiple Coils to "<<_id<<" Start Address:"<<_startAddress<<" Qty:"<<_newValues.size()<<" - ";
            
            //Load Data
            int curByte = 13;
            int curBit = 0;
            
            for (int i=0; i<totalBytes; i++){
                bitset<8> bs;
                bs.reset();
                for (int b=0; b<8; b++){
                    if (curBit < _newValues.size()){
                        bs[b] = _newValues[curBit];
                        dm<<bs[b];
                        curBit++;
                    } else {
                        b = 8;
                    }
                }
                localByteArray[i+curByte] = bs.to_ulong();
            }
            
            vector<uint8_t> lba;
            int sizeOfArray = sizeof(localByteArray) / sizeof(localByteArray[0]);
            for (int i=0; i<sizeOfArray; i++){
                lba.push_back(localByteArray[i]);
            }

            mbCommand * cmd = new mbCommand();
            cmd->msg = lba;
            cmd->length = length;
            cmd->timeAdded = ofGetElapsedTimeMillis();
            cmd->debugString = dm.str();
            commandToSend.push_back(cmd);
        } else {
            ofLogError("ofxModbusTCP IP:"+ip)<<"Write Multiple Coil Parameters Are Out Of Range";
        }
    }
}
void ofxModbusTcpClient::writeMultipleRegisters(int _id, int _startAddress, vector<int> _newValues) {
    if (enabled) {
        if (_id<=numberOfSlaves && _id>0 && _startAddress < numOfRegisters) {
            uint8_t localByteArray[6+(_newValues.size()*2)];
            
            WORD length = 4 + (_newValues.size() * 2);
            WORD start = _startAddress;
            
            localByteArray[0] = HIGHBYTE(0); //Transaction High
            localByteArray[1] = LOWBYTE(0); //Transaction Low
            localByteArray[2] = 0x00; //Protocal Identifier High
            localByteArray[3] = 0x00; //Protocol Identifier Low
            localByteArray[4] = HIGHBYTE(length); //Length High
            localByteArray[5] = LOWBYTE(length); //Length Low
            localByteArray[6] = _id; //Unit Idenfifier
            localByteArray[7] = 0x10; //Function Code
            localByteArray[8] = HIGHBYTE(start-1); //Start Address High
            localByteArray[9] = LOWBYTE(start-1); //Start Address Low
            
            //New Value
            int ct = 10; //Starting Bit
            for (int i=0; i<_newValues.size(); i++) {
                localByteArray[ct] = HIGHBYTE(_newValues.at(i)); //New Value High
                localByteArray[ct+1] = LOWBYTE(_newValues.at(i)); //New Value Low
                ct = ct+2; //Increase to next two bits
            }
            
            stringstream dm;
            dm<<"Writing Multiple Registers to "<<_id<<" Start Address:"<<_startAddress<<" Qty:"<<_newValues.size();
            
            vector<uint8_t> lba;
            int sizeOfArray = sizeof(localByteArray) / sizeof(localByteArray[0]);
            for (int i=0; i<sizeOfArray; i++){
                lba.push_back(localByteArray[i]);
            }
            
            mbCommand * cmd = new mbCommand();
            cmd->msg = lba;
            cmd->length = length;
            cmd->timeAdded = ofGetElapsedTimeMillis();
            cmd->debugString = dm.str();
            commandToSend.push_back(cmd);
        } else {
            ofLogError("ofxModbusTCP IP:"+ip)<<"Write Multiple Register Parameters Are Out Of Range";
        }
    }
}

//Command Send
#pragma mark CMD TO SEND
int ofxModbusTcpClient::getTransactionID() {
    lastTransactionID = 1 + lastTransactionID;
    lastTransactionID = lastTransactionID * (lastTransactionID<999);
    return lastTransactionID;
}
void ofxModbusTcpClient::sendNextCommand(){
    if (commandToSend.size()>0){
        if (commandToSend.front()->msg.size()>6){
            waitingForReply = true;
            
            //Set last function code
            lastFunctionCode = commandToSend.front()->msg[7];
            
            //set transaction id
            WORD tx = getTransactionID();
            commandToSend.front()->msg[0] = HIGHBYTE(tx); //Transaction High
            commandToSend.front()->msg[1] = LOWBYTE(tx); //Transaction Low
            
            //load vector to local uint8_t
            uint8_t localByteArray[commandToSend.front()->msg.size()];
            for (int i=0; i<commandToSend.front()->msg.size(); i++){
                localByteArray[i] = commandToSend.front()->msg[i];
            }
            
            //send command
            unsigned char * t = localByteArray;
            if (weConnected)tcpClient.sendRawBytes((const char*)t, 6+commandToSend.front()->length);
            
            lastSentCommand->msg = commandToSend[0]->msg;
            lastSentCommand->length = commandToSend[0]->length;
            lastSentCommand->timeAdded = commandToSend[0]->timeAdded;
            lastSentCommand->debugString = commandToSend[0]->debugString;
            
            //Log it
            sendDebug("Sent: "+commandToSend.front()->debugString);
            
            //erase last command
            delete commandToSend.front();
            commandToSend.erase(commandToSend.begin());
        }
    }
}

//Tools
#pragma mark TOOLS
WORD ofxModbusTcpClient::convertToWord(WORD _h, WORD _l) {
    WORD out;
    out = (_h << 8) | _l;
    return out;
}
