#include "ofxModbusTcpClient.h"


void ofxModbusTcpClient::setup(string _ip, int _numberOfSlaves) {
    ip = _ip;
    numberOfSlaves = _numberOfSlaves;
    if (numberOfSlaves == 0 || numberOfSlaves > 247) { numberOfSlaves = 1; }
    setupSlaves();
    stringstream dm;
    dm<< "New Slave Setup - Ip: "<<ip<<" Number of Slaves: "<<numberOfSlaves;
    sendDebug(dm.str());
}

void ofxModbusTcpClient::setup(string _ip) {
    ip = _ip;
    numberOfSlaves = 1;
    setupSlaves();
    stringstream dm;
    dm<< "New Slave Setup - Ip: "<<ip<<" Number of Slaves: "<<numberOfSlaves;
    sendDebug(dm.str());
}

void ofxModbusTcpClient::connect() {
    enabled = true;
    weConnected = tcpClient.setup(ip, port);
    connectTime = 0;
    deltaTime = 0;
    
    stringstream dm;
    dm<<"Connection Status: "<<weConnected;
    sendDebug(dm.str());
}

void ofxModbusTcpClient::disconnect() {
    enabled = false;
    stringstream dm;
    dm<<"Disconnected"<<weConnected;
    sendDebug(dm.str());
}

//Debug / Enabled
void ofxModbusTcpClient::setDebug(bool _debug) {
    debug = _debug;
    for (int i=0; i<slaves.size(); i++) {
        slaves.at(i)->setDebug(_debug);
    }
}
void ofxModbusTcpClient::setEnabled(bool _enabled) { enabled = _enabled; }
void ofxModbusTcpClient::sendDebug(string _msg) { if (debug) cout<<"ofxModbusTcpClient Debug - "<<_msg<<endl; }

void ofxModbusTcpClient::setupSlaves() {
    for (int i = 0; i<=numberOfSlaves; i++) {
        slaves.push_back(new slave(i+1));
    }
}


void ofxModbusTcpClient::update() {
    
    int transactionID = 0;
    int lengthPacket = 0;
    
    if (enabled) {
        if (weConnected) {
            
            uint8_t headerReply[2000];
            
            int totalBytes = 0;
            if (tcpClient.receiveRawBytes((char *)headerReply, 2000) > 0) { //Grab Header
                int transID = convertToWord(headerReply[0], headerReply[1]);
                int protocolID = convertToWord(headerReply[3], headerReply[4]);
                
                
                //Ignore if not last transaction ID
                if (!transID == lastTransactionID) {
                    stringstream dm;
                    dm<<"Transaction ID Mismatch, discarding Reply. Got: "<<ofToHex(transID)<<" expected:"<<ofToHex(lastTransactionID)<<endl;
                    sendDebug(dm.str());
                    return;
                }
                
                //Ignore if not correct protocol
                if (!headerReply[3] == 0x00 || !headerReply[4] == 0x00) {
                    stringstream dm;
                    dm<<"Invalid Protocol ID, discarding reply. Got: "<<ofToHex(headerReply[3])<<" "<<ofToHex(headerReply[4]);
                    sendDebug(dm.str());
                    
                    return;
                }
                
                int functionCode = headerReply[7];
                
                //Ignore if not correct last function code
                if (!functionCode == lastFunctionCode) {
                    sendDebug("Function Code Mismatch, discarding reply");
                }
                
                int originatingID = headerReply[6];
                int lengthData = convertToWord(headerReply[4], headerReply[5]);
                
                
                switch (functionCode) {
                    case 1: {
                        sendDebug("function code: Read Coils");
                        
                    } break;
                    case 3: {
                        sendDebug("function code: Read Registers ");
                        
                    } break;
                    case 5: {
                        sendDebug("function code: write single coil");
                        int address = convertToWord(headerReply[8], headerReply[9])+1;
                        bool t;
                        if (headerReply[10] == 0xff) {t=true;} else {t=false;}
                        slaves.at(originatingID-1)->setCoil(address+1, t);
                    } break;
                    case 6: {
                        sendDebug("function code: write single register");
                        int address = convertToWord(headerReply[8], headerReply[9]);
                        cout<< "id "<<originatingID<<" st reg:"<<address<<endl;
                        slaves.at(originatingID-1)->setRegister(address+1, convertToWord(headerReply[10], headerReply[11]));
                    } break;
                    case 15: {
                        sendDebug("function code: write multiple coils");
                    } break;
                    case 16: {
                        sendDebug("function code: write multiple registers");
                    } break;
                }
            }
            
            if(!active) {
                active = true;
                PreviousActivityTime = ofGetElapsedTimeMillis();
			}
            
            if(ofGetElapsedTimeMillis() > (PreviousActivityTime + 60000)) {
                if(active) {
                    active = false;
                }
            }

        } else if (!tcpClient.isConnected()){
            //if we are not connected lets try and reconnect every 5 seconds
            deltaTime = ofGetElapsedTimeMillis() - connectTime;
            
            if( deltaTime > 5000 ){
                weConnected = tcpClient.setup(ip, port);
                connectTime = ofGetElapsedTimeMillis();
            }
        }
    } else {
        if (tcpClient.isConnected()) {
            tcpClient.close();
        }
    }
}


//Slave Get Values
bool ofxModbusTcpClient::getCoil(int _id, int _startAddress) {
    if (_id>0 && _id <=slaves.size() && _startAddress>0 && _startAddress<=2000) {
        return slaves.at(_id-1)->getCoil(_startAddress);
    }
}
int ofxModbusTcpClient::getRegister(int _id, int _startAddress) {
    if (_id>0 && _id <=slaves.size() && _startAddress<=123) {
        return slaves.at(_id-1)->getRegister(_startAddress);
    }
}

//Slave Read Updates
void ofxModbusTcpClient::updateCoils(int _id, int _startAddress, int _qty) {
    if (_id<=numberOfSlaves && _id>0 && _qty <=2000 && _qty>0) {
        uint8_t localByteArray[12];
        
        WORD tx = getTransactionID();
        WORD length = 6;
        WORD start = _startAddress-1;
        WORD qty = _qty;
        
        localByteArray[0] = HIGHBYTE(tx); //Transaction High
        localByteArray[1] = LOWBYTE(tx); //Transaction Low
        localByteArray[2] = 0x00; //Protocal Identifier High
        localByteArray[3] = 0x00; //Protocol Identifier Low
        localByteArray[4] = HIGHBYTE(length); //Length High
        localByteArray[5] = LOWBYTE(length); //Length Low
        localByteArray[6] = _id; //Unit Idenfifier
        localByteArray[7] = 0x01; //Function Code
        localByteArray[8] = HIGHBYTE(start); //Start Address High
        localByteArray[9] = LOWBYTE(start); //Start Address Low
        localByteArray[10] = HIGHBYTE(qty); //Qty High
        localByteArray[11] = LOWBYTE(qty); //Qty Low
        
        unsigned char * t = localByteArray;
        tcpClient.sendRawBytes((const char*)t, 6+length);
        stringstream dm;
        dm<<"Reading Coils of "<<_id<<" Start:"<<_startAddress<<" Qty:"<<_qty;;
        sendDebug(dm.str());
    } else {
        sendDebug("Reding Coils Parameters Are Out Of Range");
    }
}
void ofxModbusTcpClient::updateRegisters(int _id, int _startAddress, int _qty) {
    if (_id<=numberOfSlaves && _id>0 && _qty<=125 && _qty>0) {
        uint8_t localByteArray[12];
        
        WORD tx = getTransactionID();
        WORD length = 6;
        WORD start = _startAddress-1;
        WORD qty = _qty;
        
        localByteArray[0] = HIGHBYTE(tx); //Transaction High
        localByteArray[1] = LOWBYTE(tx); //Transaction Low
        localByteArray[2] = 0x00; //Protocal Identifier High
        localByteArray[3] = 0x00; //Protocol Identifier Low
        localByteArray[4] = HIGHBYTE(length); //Length High
        localByteArray[5] = LOWBYTE(length); //Length Low
        localByteArray[6] = _id; //Unit Idenfifier
        localByteArray[7] = 0x03; //Function Code
        localByteArray[8] = HIGHBYTE(start); //Start Address High
        localByteArray[9] = LOWBYTE(start); //Start Address Low
        localByteArray[10] = HIGHBYTE(qty); //Qty High
        localByteArray[11] = LOWBYTE(qty); //Qty Low
        
        unsigned char * t = localByteArray;
        tcpClient.sendRawBytes((const char*)t, 6+length);
        stringstream dm;
        dm<<"Reading Registers of "<<_id<<" Start:"<<_startAddress<<" Qty:"<<_qty;;
        sendDebug(dm.str());
    } else {
        sendDebug("Read Registers Parameters Are Out Of Range");
    }
}

//Slave Writes
void ofxModbusTcpClient::writeCoil(int _id, int _startAddress, bool _newValue) {
    if (_id<=numberOfSlaves && _id>0 && _startAddress>0 && _startAddress<=2000) {
        uint8_t localByteArray[12];
        
        WORD tx = getTransactionID();
        WORD length = 6;
        WORD start = _startAddress-1;
        
        localByteArray[0] = HIGHBYTE(tx); //Transaction High
        localByteArray[1] = LOWBYTE(tx); //Transaction Low
        localByteArray[2] = 0x00; //Protocal Identifier High
        localByteArray[3] = 0x00; //Protocol Identifier Low
        localByteArray[4] = HIGHBYTE(length); //Length High
        localByteArray[5] = LOWBYTE(length); //Length Low
        localByteArray[6] = _id; //Unit Idenfifier
        localByteArray[7] = 0x05; //Function Code
        localByteArray[8] = HIGHBYTE(start); //Start Address High
        localByteArray[9] = LOWBYTE(start); //Start Address Low
        
        //New Value
        if (_newValue) {
            localByteArray[10] = 0xff; //New Value High
            localByteArray[11] = 0x00; //New Value Low
        } else {
            localByteArray[10] = 0x00; //New Value High
            localByteArray[11] = 0x00; //New Value Low
        }
        
        lastFunctionCode = 0x05;
        
        unsigned char * t = localByteArray;
        tcpClient.sendRawBytes((const char*)t, 6+length);
        stringstream dm;
        dm<<"Writing Coil of "<<_id<<" Address:"<<_startAddress<<" Value:"<<_newValue;
        sendDebug(dm.str());
    } else {
        sendDebug("Write Coil Parameters Are Out Of Range");
    }
}
void ofxModbusTcpClient::writeRegister(int _id, int _startAddress, int _newValue) {
    if (_id<=numberOfSlaves && _id>0 && _startAddress>0 && _startAddress<=123) {
        uint8_t localByteArray[12];
        
        WORD tx = getTransactionID();
        WORD length = 6;
        WORD start = _startAddress-1;
        WORD newVal = _newValue;
        
        localByteArray[0] = HIGHBYTE(tx); //Transaction High
        localByteArray[1] = LOWBYTE(tx); //Transaction Low
        localByteArray[2] = 0x00; //Protocal Identifier High
        localByteArray[3] = 0x00; //Protocol Identifier Low
        localByteArray[4] = HIGHBYTE(length); //Length High
        localByteArray[5] = LOWBYTE(length); //Length Low
        localByteArray[6] = _id; //Unit Idenfifier
        localByteArray[7] = 0x06; //Function Code
        localByteArray[8] = HIGHBYTE(start); //Start Address High
        localByteArray[9] = LOWBYTE(start); //Start Address Low
        localByteArray[10] = HIGHBYTE(newVal); //New Value High
        localByteArray[11] = LOWBYTE(newVal); //New Value Low
        
        lastFunctionCode = 0x06;
        
        unsigned char * t = localByteArray;
        tcpClient.sendRawBytes((const char*)t, 6+length);
        stringstream dm;
        dm<<"Writing Register to "<<_id<<" Address:"<<_startAddress<<" Value:"<<_newValue;
        sendDebug(dm.str());
    } else {
        sendDebug("Write Register Parameters Are Out Of Range");
    }
}
void ofxModbusTcpClient::writeMultipleCoils(int _id, int _startAddress, vector<bool> _newValues) {
    if (_id<=numberOfSlaves && _id>0 && _startAddress>0 && _startAddress<=2000) {
        uint8_t localByteArray[6+(_newValues.size()*2)];
        
        WORD tx = getTransactionID();
        int totB = ceil((float)_newValues.size()/(float)8); //Always in multiples of 8
        WORD length = 4 + (totB);
        WORD start = _startAddress-1;
        
        localByteArray[0] = HIGHBYTE(tx); //Transaction High
        localByteArray[1] = LOWBYTE(tx); //Transaction Low
        localByteArray[2] = 0x00; //Protocal Identifier High
        localByteArray[3] = 0x00; //Protocol Identifier Low
        localByteArray[4] = HIGHBYTE(length); //Length High
        localByteArray[5] = LOWBYTE(length); //Length Low
        localByteArray[6] = _id; //Unit Idenfifier
        localByteArray[7] = 0x0f; //Function Code
        localByteArray[8] = HIGHBYTE(start); //Start Address High
        localByteArray[9] = LOWBYTE(start); //Start Address Low
        
        //New Value
        int cb = 10; //Starting Write Byte
        int cv = 0; //Starting bit
        
        for (int p=0; p<_newValues.size(); p++) {
            
            if (_newValues.at(p)) { //Set Bit
                localByteArray[cb] |= 1 << cv;
            } else { //Clear Bit
                localByteArray[cb] &= ~(1 << cv);
            }
            
            cv++; //Move to Next Bit
            if (cv==8) {
                cv = 0; //Reset Bits
                cb++; //Move to next Byte
            }
        }
        
        lastFunctionCode = 0x0f;
        
        unsigned char * t = localByteArray;
        tcpClient.sendRawBytes((const char*)t, 6+length);
        stringstream dm;
        dm<<"Writing Multiple Coils to "<<_id<<" Start Address:"<<_startAddress<<" Qty:"<<_newValues.size();
        sendDebug(dm.str());
    } else {
        sendDebug("Write Multiple Coil Parameters Are Out Of Range");
    }
}
void ofxModbusTcpClient::writeMultipleRegisters(int _id, int _startAddress, vector<int> _newValues) {
    if (_id<=numberOfSlaves && _id>0 && _startAddress>0 && _startAddress<=123) {
        uint8_t localByteArray[6+(_newValues.size()*2)];
        
        WORD tx = getTransactionID();
        WORD length = 4 + (_newValues.size() * 2);
        WORD start = _startAddress-1;
        
        localByteArray[0] = HIGHBYTE(tx); //Transaction High
        localByteArray[1] = LOWBYTE(tx); //Transaction Low
        localByteArray[2] = 0x00; //Protocal Identifier High
        localByteArray[3] = 0x00; //Protocol Identifier Low
        localByteArray[4] = HIGHBYTE(length); //Length High
        localByteArray[5] = LOWBYTE(length); //Length Low
        localByteArray[6] = _id; //Unit Idenfifier
        localByteArray[7] = 0x10; //Function Code
        localByteArray[8] = HIGHBYTE(start); //Start Address High
        localByteArray[9] = LOWBYTE(start); //Start Address Low
        
        //New Value
        int ct = 10; //Starting Bit
        for (int i=0; i<_newValues.size(); i++) {
            localByteArray[ct] = HIGHBYTE(_newValues.at(i)); //New Value High
            localByteArray[ct+1] = LOWBYTE(_newValues.at(i)); //New Value Low
            ct = ct+2; //Increase to next two bits
        }
        
        lastFunctionCode = 0x10;
        
        unsigned char * t = localByteArray;
        tcpClient.sendRawBytes((const char*)t, 6+length);
        stringstream dm;
        dm<<"Writing Multiple Registers to "<<_id<<" Start Address:"<<_startAddress<<" Qty:"<<_newValues.size();
        sendDebug(dm.str());
    } else {
        sendDebug("Write Multiple Register Parameters Are Out Of Range");
    }
}



WORD ofxModbusTcpClient::convertToWord(WORD _h, WORD _l) {
    WORD out;
    out = (_h << 8) | _l;
    return out;
}

int ofxModbusTcpClient::getTransactionID() {
    lastTransactionID = 1 + lastTransactionID * (lastTransactionID<999);
    return lastTransactionID;
}




