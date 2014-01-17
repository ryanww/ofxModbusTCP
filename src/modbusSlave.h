//
//  modbusSlave.h
//  modbusTest
//
//  Created by Ryan Wilkinson on 1/14/14.
//
//

#pragma once

class slave {
public:
    slave (int _id) {
        init(_id);
    }
    
    //Gets
    int getID() { return idNumber; }
    string getName() { return name; }
    bool getCoil (int _coilAddress) {
        if (_coilAddress>0 && _coilAddress<=C.size()) {
            return C.at(_coilAddress-1);
        } else {
            cout<<"Get Slave Coil Exceeds Size"<<endl;
        }
    }
    int getRegister (int _registerAddress) {
        if (_registerAddress>0 && _registerAddress<=R.size()) {
            return R.at(_registerAddress-1);
        } else {
            cout<<"Get Slave Register Exceeds Size"<<endl;
        }
    }
    
    
    //Sets
    void setName (string _name) { name = _name; }
    void setCoil(int _coilAddress, bool _newValue) {
        if (_coilAddress>0 && _coilAddress<=C.size()) {
            C.at(_coilAddress-1) = _newValue;
            stringstream dm;
            dm<<"Coil "<<_coilAddress<<" set to: "<<_newValue;
            sendDebug(dm.str());
        } else {
            cout<<"Set Slave Coil Exceeds Size"<<endl;
        }
    }
    void setRegister (int _registerAddress, int _newValue) {
        if (_registerAddress>0 && _registerAddress<=R.size()) {
            R.at(_registerAddress-1) = _newValue;
            stringstream dm;
            dm<<"Register "<<_registerAddress<<" set to: "<<_newValue;
            sendDebug(dm.str());
        } else {
            cout<<"Set Slave Register Exceeds Size"<<endl;
        }
    }
    void setDebug(bool _debug) { debug = _debug; }
    
private:
    int idNumber;
    string name;
    bool debug;
    
    vector<int> R; //Registers
    vector<bool> C; //Coils
    
    void sendDebug(string _dm) { if(debug) { cout<<"Slave "<<idNumber<<" Debug - "<<_dm<<endl; } }
    
    void init(int _id) {
        idNumber = _id;
        R.resize(125);
        C.resize(2000);
    }    
};

