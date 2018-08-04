/*
* Copyright (c) 2017-2018  CelePixel Technology Co. Ltd.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef FPGADATAREADER_H
#define FPGADATAREADER_H

#include "../base/hhconstants.h"

class FPGADataReader
{
public:
    static unsigned int getColumn(unsigned char data[EVENT_SIZE]);
    static unsigned int getRow(unsigned char data[EVENT_SIZE]);
    static unsigned int getTimeStamp(unsigned char data[EVENT_SIZE]);
    static unsigned int getBrightness(unsigned char data[EVENT_SIZE]);
    static unsigned int getCurrentRow() { return s_uiCurrentRow;    }
	static unsigned int getLastRow() { return s_uiLastRow; }
    static unsigned int getTFromFPGA()  { return s_uiTFromFPGA;     }
	static unsigned int getLastTFromFPGA() { return s_uiLastTFromFPGA; }
    static unsigned int getMapT()       { return s_uiMapT;          }
    static unsigned int getEventType()  { return s_uiEventType; }
    static unsigned int getSpecialEventType() { return s_uiSpecialEventType; }

    //check event type
    static bool isForcefirePixel(unsigned char data[EVENT_SIZE]); // column event from force fire, carrying ADC
    static bool isColumnEvent(unsigned char data[EVENT_SIZE]);
    static bool isRowEvent(unsigned char data[EVENT_SIZE]);
    static bool isSpecialEvent(unsigned char data[EVENT_SIZE]);
	static unsigned int MapTime(unsigned char data[EVENT_SIZE]);
    
private:
    static unsigned int  s_uiCurrentRow;
	static unsigned int  s_uiLastRow;
    static unsigned int  s_uiTFromFPGA;
	static unsigned int  s_uiLastTFromFPGA;
    static unsigned int  s_uiMapT;
    static unsigned int  s_uiEventType;
    static unsigned int  s_uiSpecialEventType;
};

#endif // FPGADATAREADER_H
