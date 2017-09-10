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

class slave {
public:
    slave (int _id) {
        init(_id);
    }
    
    //Gets
    int getID() { return idNumber; }
    string getMasterIP() {return masterIP; }
    string getName() { return name; }
    bool getCoil (int _coilAddress) {
        if (_coilAddress <= C.size()) {
            return C.at(_coilAddress);
        } else {
            ofLogError("ofxModbusTCP Slave:"+ofToString(idNumber)+" on "+masterIP)<<"Get Slave Coil Exceeds Size";
        }
    }
    int getRegister (int _registerAddress) {
        if (_registerAddress < R.size()) {
            return R.at(_registerAddress);
        } else {
            ofLogError("ofxModbusTCP Slave:"+ofToString(idNumber)+" on "+masterIP)<<"Get Slave Register Exceeds Size";
        }
    }
    
    
    //Sets
    void setName (string _name) { name = _name; }
    void setmasterIP (string _ip) { masterIP = _ip; }
    void setCoil(int _coilAddress, bool _newValue) {
        if (_coilAddress<C.size()) {
            C.at(_coilAddress) = _newValue;
            stringstream dm;
            dm<<"Coil "<<_coilAddress<<" set to: "<<_newValue;
            sendDebug(dm.str());
        } else {
            ofLogError("ofxModbusTCP Slave:"+ofToString(idNumber)+" on "+masterIP)<<"Set Slave Coil Exceeds Size";
        }
    }
    void setMultipleCoils(int _coilStartingAddress, vector<bool> _newValues) {
        if (_newValues.size()){
            stringstream dm;
            dm<<"Coils starting at:"<<_coilStartingAddress<<" set to: ";
            for (int i=0; i<_newValues.size(); i++){
                if (i < C.size()){
                    C.at(_coilStartingAddress) = _newValues.at(i);
                    dm<<_newValues.at(i);
                } else {
                    ofLogError("ofxModbusTCP Slave:"+ofToString(idNumber)+" on "+masterIP)<<"Set Multiple Slave Coils Exceeds Sizes";
                }
            }
            sendDebug(dm.str());
        } else {
            ofLogError("ofxModbusTCP Slave:"+ofToString(idNumber)+" on "+masterIP)<<"Set Multiple Slave Coils Exceeds Sizes";
        }
    }
    void setRegister (int _registerAddress, int _newValue) {
        if (_registerAddress < R.size()) {
            R.at(_registerAddress) = _newValue;
            stringstream dm;
            dm<<"Register "<<_registerAddress<<" set to: "<<_newValue;
            sendDebug(dm.str());
        } else {
            ofLogError("ofxModbusTCP Slave:"+ofToString(idNumber)+" on "+masterIP)<<"Set Slave Register Exceeds Size";
        }
    }
    
    void setDebugEnable(bool _enabled){debug = _enabled;}
    
private:
    int idNumber;
    string masterIP;
    string name;
    
    bool debug = false;
    
    vector<int> R; //Registers
    vector<bool> C; //Coils
    
    void sendDebug(string _dm) {
        if (debug){
            ofLogVerbose("ofxModbusTCP Slave:"+ofToString(idNumber)+" on "+masterIP)<<_dm;
        }
    }
    
    void init(int _id) {
        idNumber = _id;
        R.resize(5000);
        C.resize(5000);
    }    
};

