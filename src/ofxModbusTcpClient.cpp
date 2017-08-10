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


//Setup
void ofxModbusTcpClient::setup(string _ip, int _numberOfSlaves) {
    ip = _ip;
    numberOfSlaves = _numberOfSlaves;
    if (numberOfSlaves == 0 || numberOfSlaves > 247) { numberOfSlaves = 1; }
    ofLogNotice("ofxModbusTCP IP:"+ip)<<"Setup with "<<numberOfSlaves<<" slaves";
    setupSlaves();
    //ofAddListener(ofEvents().update, this, &ofxModbusTcpClient::update);
}
void ofxModbusTcpClient::setup(string _ip) {
    ip = _ip;
    numberOfSlaves = 1;
    ofLogNotice("ofxModbusTCP IP:"+ip)<<"Setup with "<<numberOfSlaves<<" slaves";
    setupSlaves();
    //ofAddListener(ofEvents().update, this, &ofxModbusTcpClient::update);
}
void ofxModbusTcpClient::setupSlaves() {
    for (int i = 0; i<=numberOfSlaves; i++) {
        slaves.push_back(new slave(i+1));
        slaves.back()->setmasterIP(ip);
    }
}

//Connection
void ofxModbusTcpClient::connect() {
    if (!weConnected){
        enabled = true;
        try {
            weConnected = tcpClient.setup(ip, port);
        } catch (...) {
            cout<<"Couldn't connect to Modbus Master at "<<ip<<endl;
        }
        connectTime = 0;
        deltaTime = 0;
        
        if (!isThreadRunning())startThread();
        
        ofLogNotice("ofxModbusTCP IP:"+ip)<<"Connect - Status: "<<weConnected;
    } else {
        ofLogNotice("ofxModbusTCP IP:"+ip)<<"Already connected - ignoring1";
    }
}
void ofxModbusTcpClient::disconnect() {
    if (enabled){
        tcpClient.close();
    }
    enabled = false;
    weConnected = false;
    commandToSend.clear();
    ofLogNotice("ofxModbusTCP IP:"+ip)<<"Disconnect";
}
bool ofxModbusTcpClient::isConnected(){
    if (enabled){
        return weConnected;
    } else {
        return false;
    }
}

//Enables
void ofxModbusTcpClient::setEnabled(bool _enabled) { enabled = _enabled; }
void ofxModbusTcpClient::sendDebug(string _msg) {
    ofLogVerbose("ofxModbusTCP IP:"+ip)<<_msg;
}
void ofxModbusTcpClient::setDebugEnabled(bool _enabled){
    debug = _enabled;
    for (int i=0; i<slaves.size(); i++){
        slaves[i]->setDebugEnable(debug);
    }
}

//Main Function
void ofxModbusTcpClient::threadedFunction(){
    while (isThreadRunning()){
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
                    if (functionCode > 127) functionCode = functionCode - 127;
                    //Ignore if not correct last function code
                    if (functionCode != lastFunctionCode) {
                        ofLogError("ofxModbusTCP IP:"+ip)<<"Function Code Mismatch, discarding reply - Should be "<<lastFunctionCode<<", got "<<functionCode;
                    }
                    
                    int originatingID = headerReply[6];
                    int lengthData = convertToWord(headerReply[4], headerReply[5]);
                    
                    
                    switch (functionCode) {
                        case 1: {
                            int byteCount = headerReply[8];
                            vector<char> nc;
                            vector<bool> nv;
                            for (int i=9; i<9+byteCount; i++){ nc.push_back(headerReply[i]); }
                            int startAddr = convertToWord(lastSentCommand->msg[8], lastSentCommand->msg[9])+1;
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
                                slaves.at(originatingID-1)->setRegister(i+1, newVal);
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
                            int address = convertToWord(headerReply[8], headerReply[9]);
                            sendDebug("Reply Received, Setting Single Register at Address "+ofToString(address)+" to value "+ofToString(convertToWord(headerReply[10], headerReply[11])));
                            slaves.at(originatingID-1)->setRegister(address+1, convertToWord(headerReply[10], headerReply[11]));
                        } break;
                        case 15: {
                            sendDebug("Reply Received, Setting Multiple Coils - not supported yet");
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
bool ofxModbusTcpClient::getCoil(int _id, int _startAddress) {
    if (enabled) {
        if (_id>0 && _id <=slaves.size() && _startAddress>0 && _startAddress<=2000) {
            return slaves.at(_id-1)->getCoil(_startAddress);
        }
    } else {
        return false;
    }
}
int ofxModbusTcpClient::getRegister(int _id, int _startAddress) {
    if (enabled ) {
        if (_id>0 && _id <=slaves.size() && _startAddress<=123) {
            return slaves.at(_id-1)->getRegister(_startAddress);
        }
    } else {
        return 0;
    }
}

//Slave Read Updates
void ofxModbusTcpClient::updateCoils(int _id, int _startAddress, int _qty) {
    if (enabled) {
        if (_id<=numberOfSlaves && _id>0 && _qty <=2000 && _qty>0) {
            uint8_t localByteArray[12];
            
            WORD length = 6;
            WORD start = _startAddress-1;
            WORD qty = _qty;
            
            localByteArray[0] = HIGHBYTE(0); //Transaction High
            localByteArray[1] = LOWBYTE(0); //Transaction Low
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
void ofxModbusTcpClient::writeCoil(int _id, int _startAddress, bool _newValue) {
    if (enabled) {
        if (_id<=numberOfSlaves && _id>0 && _startAddress>0 && _startAddress<=2000) {
            uint8_t localByteArray[12];
            
            WORD length = 6;
            WORD start = _startAddress-1;
            
            localByteArray[0] = HIGHBYTE(0); //Transaction High
            localByteArray[1] = LOWBYTE(0); //Transaction Low
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
        if (_id<=numberOfSlaves && _id>0 && _startAddress>0 && _startAddress<=123) {
            uint8_t localByteArray[12];
            
            WORD length = 6;
            WORD start = _startAddress-1;
            WORD newVal = _newValue;
            
            localByteArray[0] = HIGHBYTE(0); //Transaction High
            localByteArray[1] = LOWBYTE(0); //Transaction Low
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
            
            stringstream dm;
            dm<<"Writing Register to "<<_id<<" Address:"<<_startAddress<<" Value:"<<_newValue;
            
            vector<uint8_t> lba;
            int sizeOfArray = sizeof(localByteArray) / sizeof(localByteArray[0]);
            for (int i=0; i<sizeOfArray; i++){
                lba.push_back(localByteArray[i]);
            }
            
            commandToSend.push_back(new mbCommand);
            commandToSend.back()->msg = lba;
            commandToSend.back()->length = length;
            commandToSend.back()->timeAdded = ofGetElapsedTimeMillis();
            commandToSend.back()->debugString = dm.str();
        } else {
            ofLogError("ofxModbusTCP IP:"+ip)<<"Write Register Parameters Are Out Of Range";
        }
    }
}
void ofxModbusTcpClient::writeMultipleCoils(int _id, int _startAddress, vector<bool> _newValues) {
    if (enabled) {
        if (_id<=numberOfSlaves && _id>0 && _startAddress>0 && _startAddress<=2000) {
            uint8_t localByteArray[6+(_newValues.size()*2)];
            
            int totB = ceil((float)_newValues.size()/(float)8); //Always in multiples of 8
            WORD length = 4 + (totB);
            WORD start = _startAddress-1;
            
            localByteArray[0] = HIGHBYTE(0); //Transaction High
            localByteArray[1] = LOWBYTE(0); //Transaction Low
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
            
            stringstream dm;
            dm<<"Writing Multiple Coils to "<<_id<<" Start Address:"<<_startAddress<<" Qty:"<<_newValues.size();
            
            vector<uint8_t> lba;
            int sizeOfArray = sizeof(localByteArray) / sizeof(localByteArray[0]);
            for (int i=0; i<sizeOfArray; i++){
                lba.push_back(localByteArray[i]);
            }

            commandToSend.push_back(new mbCommand);
            commandToSend.back()->msg = lba;
            commandToSend.back()->length = length;
            commandToSend.back()->timeAdded = ofGetElapsedTimeMillis();
            commandToSend.back()->debugString = dm.str();
        } else {
            ofLogError("ofxModbusTCP IP:"+ip)<<"Write Multiple Coil Parameters Are Out Of Range";
        }
    }
}
void ofxModbusTcpClient::writeMultipleRegisters(int _id, int _startAddress, vector<int> _newValues) {
    if (enabled) {
        if (_id<=numberOfSlaves && _id>0 && _startAddress>0 && _startAddress<=123) {
            uint8_t localByteArray[6+(_newValues.size()*2)];
            
            WORD length = 4 + (_newValues.size() * 2);
            WORD start = _startAddress-1;
            
            localByteArray[0] = HIGHBYTE(0); //Transaction High
            localByteArray[1] = LOWBYTE(0); //Transaction Low
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
            
            stringstream dm;
            dm<<"Writing Multiple Registers to "<<_id<<" Start Address:"<<_startAddress<<" Qty:"<<_newValues.size();
            
            vector<uint8_t> lba;
            int sizeOfArray = sizeof(localByteArray) / sizeof(localByteArray[0]);
            for (int i=0; i<sizeOfArray; i++){
                lba.push_back(localByteArray[i]);
            }
            
            commandToSend.push_back(new mbCommand);
            commandToSend.back()->msg = lba;
            commandToSend.back()->length = length;
            commandToSend.back()->timeAdded = ofGetElapsedTimeMillis();
            commandToSend.back()->debugString = dm.str();
        } else {
            ofLogError("ofxModbusTCP IP:"+ip)<<"Write Multiple Register Parameters Are Out Of Range";
        }
    }
}

//Command Send
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
            
            lastSentCommand = commandToSend.front();
            
            //Log it
            sendDebug("Sent: "+commandToSend.front()->debugString);
            
            //erase last command
            commandToSend.erase(commandToSend.begin());
        }
    }
}

//Tools
WORD ofxModbusTcpClient::convertToWord(WORD _h, WORD _l) {
    WORD out;
    out = (_h << 8) | _l;
    return out;
}
