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

#include "../include/celex5/celex5.h"
#include "../frontpanel/frontpanel.h"
#include "../driver/CeleDriver.h"
#include "../configproc/hhsequencemgr.h"
#include "../configproc/hhwireincommand.h"
#include "../base/xbase.h"
#include "../eventproc/dataprocessthread.h"
#include "../eventproc/datareaderthread.h"
#include "datarecorder.h"
#include <iostream>
#include <cstring>
#include <thread>

#ifdef _WIN32
static HANDLE s_hEventHandle = nullptr;
#endif

CeleX5::CeleX5() 
	: m_bLoopModeEnabled(false)
	, m_uiClockRate(100)
	, m_iEventDataFormat(2)
	, m_pDataToRead(NULL)
	, m_uiPackageCount(0)
	, m_bFirstReadFinished(false)
	, m_pReadBuffer(NULL)
	, m_emDeviceType(CeleX5::Unknown_Devive)
	, m_pFrontPanel(NULL)
	, m_pCeleDriver(NULL)
	, m_uiPackageCounter(0)
	, m_uiPackageTDiff(0)
	, m_uiPackageBeginT(0)
	, m_bAutoISPEnabled(false)
	, m_arrayISPThreshold{60, 500, 2500}
	, m_arrayBrightness{100, 130, 150, 175}
	, m_uiAutoISPRefreshTime(80)
{
	//m_pCeleDriver = new CeleDriver;
	//m_pCeleDriver->Open();

	m_pSequenceMgr = new HHSequenceMgr;
	m_pSequenceMgr->parseCeleX5Cfg();
	m_mapCfgModified = m_mapCfgDefaults = getCeleX5Cfg();
	//
	//create data process thread
	m_pDataProcessThread = new DataProcessThreadEx("CeleX5Thread");
	//m_pDataProcessThread->setDeviceType(CeleX5::CeleX5_MIPI);
	m_pDataProcessThread->setCeleX(this);
	m_pDataProcessThread->start();

	m_pDataRecorder = new DataRecorder;
	//auto n = thread::hardware_concurrency();//cpu core count = 8
}

CeleX5::~CeleX5()
{
	if (m_pFrontPanel)
	{
		delete m_pFrontPanel;
		m_pFrontPanel = NULL;
	}
	if (m_pCeleDriver)
	{
		m_pCeleDriver->clearData();
		m_pCeleDriver->Close();
		delete m_pCeleDriver;
		m_pCeleDriver = NULL;
	}
	if (m_pSequenceMgr) delete m_pSequenceMgr;
	//
	if (m_pDataProcessThread) delete m_pDataProcessThread;
	//
	if (m_pDataRecorder) delete m_pDataRecorder;

	if (m_pReadBuffer) {
		delete[] m_pReadBuffer;
		m_pReadBuffer = NULL;
	}
}

bool CeleX5::openSensor(DeviceType type)
{
	m_emDeviceType = type;
	m_pDataProcessThread->setDeviceType(type);
	if (CeleX5::CeleX5_Parallel == type)
	{
		m_pFrontPanel = FrontPanel::getInstance();
		initializeFPGA();
	}
	else
	{
		if (NULL == m_pCeleDriver)
		{
			m_pCeleDriver = new CeleDriver;
			//if (!m_pCeleDriver->Open())
			if (!m_pCeleDriver->openUSB())
				return false;
		}
	}
	if (isSensorReady())
	{
		if (!configureSettings(type))
			return false;
	}
	return true;
}

bool CeleX5::isSensorReady()
{
	return true;
}

void CeleX5::pipeOutFPGAData()
{
	if (CeleX5::CeleX5_Parallel != m_emDeviceType)
	{
		return;
	}
	if (!isSensorReady())
	{
		return;
	}
	uint32_t pageCount;
	m_pFrontPanel->wireOut(0x21, 0x1FFFFF, &pageCount);
	//cout << "-------------- pageCount = " << pageCount << endl;
	if (pageCount > 10)
	{
		if (pageCount > MAX_PAGE_COUNT)
			pageCount = MAX_PAGE_COUNT;
		int blockSize = 128;
		long length = (long)(pageCount * blockSize);
		if (NULL == m_pReadBuffer)
			m_pReadBuffer = new unsigned char[128 * MAX_PAGE_COUNT];
		//unsigned char* pData = new unsigned char[length];
		//Return the number of bytes read or ErrorCode (<0) if the read failed. 
		long dataLen = m_pFrontPanel->blockPipeOut(0xa0, blockSize, length, m_pReadBuffer);
		if (dataLen > 0)
		{
			//record sensor data
			if (m_pDataRecorder->isRecording())
			{
				m_pDataRecorder->writeData(m_pReadBuffer, dataLen);
			}
			m_pDataProcessThread->addData(m_pReadBuffer, dataLen);
		}
		else if (dataLen < 0) //read failed
		{
			switch (dataLen)
			{
			case okCFrontPanel::InvalidBlockSize:
				cout << "Block Size Not Supported" << endl;
				break;

			case okCFrontPanel::UnsupportedFeature:
				cout << "Unsupported Feature" << endl;
				break;

			default:
				cout << "Transfer Failed with error: " << dataLen << endl;
				break;
			}
			cout << "pageCount = " << pageCount << ", blockSize = " << blockSize << ", length = " << length << endl;
		}
	}
}

void CeleX5::getMIPIData(vector<uint8_t> &buffer)
{
	if (CeleX5::CeleX5_MIPI != m_emDeviceType)
	{
		return;
	}
	if (m_pCeleDriver->getimage(buffer))
	{
		//record sensor data
		if (m_pDataRecorder->isRecording())
		{
			m_pDataRecorder->writeData(buffer);
		}
		//cout << "image buffer size = " << buffer.size() << endl;
		//m_pDataProcessThread->addData(buffer);

#ifdef _WIN32
		m_uiPackageCounter++;
		uint32_t t2 = GetTickCount();
		m_uiPackageTDiff += (t2 - m_uiPackageBeginT);
		m_uiPackageBeginT = t2;
		if (m_uiPackageTDiff > 1000)
		{
			//cout << "--- package count = " << counter << endl;
			m_pDataProcessThread->getDataProcessor5()->getProcessedData()->setFullFrameFPS(m_uiPackageCountPS);
			m_uiPackageTDiff = 0;
			m_uiPackageCountPS = m_uiPackageCounter;
			m_uiPackageCounter = 0;
		}
#endif
	}
}

bool CeleX5::setFpnFile(const std::string& fpnFile)
{
	return m_pDataProcessThread->getDataProcessor5()->setFpnFile(fpnFile);
}

void CeleX5::generateFPN(std::string fpnFile)
{
	m_pDataProcessThread->getDataProcessor5()->generateFPN(fpnFile);
}

void CeleX5::reset()
{
	//resetPin(true);
	//resetPin(false);
	resetConfigureSettings(m_emDeviceType);
}


uint32_t CeleX5::getFullFrameFPS()
{
	return m_uiPackageCountPS;
}

// Set the Sensor operation mode in fixed mode
// address = 53, width = [2:0]
void CeleX5::setSensorFixedMode(CeleX5Mode mode)
{
	m_pDataProcessThread->clearData();

	if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		m_pCeleDriver->clearData();
	}

	//Disable ALS read and write, must be the first operation
	setALSEnabled(false);

	//Enter CFG Mode
	wireIn(93, 0, 0xFF);
	wireIn(90, 1, 0xFF);

	//Write Fixed Sensor Mode
	wireIn(53, static_cast<uint32_t>(mode), 0xFF);

	//Disable brightness adjustment (auto isp), always load sensor core parameters from profile0
	wireIn(221, 0, 0xFF); //AUTOISP_BRT_EN
	wireIn(223, 0, 0xFF); //AUTOISP_TRIGGER
	wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR, Write core parameters to profile0
	writeRegister(233, -1, 232, 1500); //AUTOISP_BRT_VALUE, Set initial brightness value 1500
	writeRegister(22, -1, 23, 140); //BIAS_BRT_I, Override the brightness value in profile0, avoid conflict with AUTOISP profile0

	//Enter Start Mode
	wireIn(90, 0, 0xFF);
	wireIn(93, 1, 0xFF);

	m_pDataProcessThread->getDataProcessor5()->setSensorFixedMode(mode);
}

// Set the Sensor operation mode in loop mode
// loop = 1: the first operation mode in loop mode, address = 53, width = [2:0]
// loop = 2: the second operation mode in loop mode, address = 54, width = [2:0]
// loop = 3: the third operation mode in loop mode, address = 55, width = [2:0]
void CeleX5::setSensorLoopMode(CeleX5Mode mode, int loopNum)
{
	m_pDataProcessThread->clearData();
	if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		m_pCeleDriver->clearData();
	}

	if (loopNum < 1 || loopNum > 3)
	{
		cout << "CeleX5::setSensorMode: wrong loop number!";
		return;
	}
	//Enter CFG Mode
	wireIn(93, 0, 0xFF);
	wireIn(90, 1, 0xFF);

	wireIn(52 + loopNum, static_cast<uint32_t>(mode), 0xFF);

	//Enter Start Mode
	wireIn(90, 0, 0xFF);
	wireIn(93, 1, 0xFF);

	m_pDataProcessThread->getDataProcessor5()->setSensorLoopMode(mode, loopNum);
}

CeleX5::CeleX5Mode CeleX5::getSensorFixedMode()
{
	return m_pDataProcessThread->getDataProcessor5()->getSensorFixedMode();
}

CeleX5::CeleX5Mode CeleX5::getSensorLoopMode(int loopNum)
{
	return m_pDataProcessThread->getDataProcessor5()->getSensorLoopMode(loopNum);
}

// enable or disable loop mode
// address = 64, width = [0], 0: fixed mode / 1: loop mode
void CeleX5::setLoopModeEnabled(bool enable)
{
	m_bLoopModeEnabled = enable;

	if (m_bAutoISPEnabled)
		setALSEnabled(false);

	//Enter CFG Mode
	wireIn(93, 0, 0xFF);
	wireIn(90, 1, 0xFF);
	if (enable)
	{
		wireIn(64, 1, 0xFF);
		//Disable brightness adjustment (auto isp), always load sensor core parameters from profile0
		wireIn(221, 0, 0xFF); //AUTOISP_BRT_EN, disable auto isp
		wireIn(223, 1, 0xFF); //AUTOISP_TRIGGER
		wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR, Write core parameters to profile0
		writeRegister(233, -1, 232, 1500); //AUTOISP_BRT_VALUE, Set initial brightness value 1500
		writeRegister(22, -1, 23, 140); //BIAS_BRT_I, Override the brightness value in profile0, avoid conflict with AUTOISP profile0
	}
	else
	{
		wireIn(64, 0, 0xFF);
	}
	//Enter Start Mode
	wireIn(90, 0, 0xFF);
	wireIn(93, 1, 0xFF);

	m_pDataProcessThread->getDataProcessor5()->setLoopModeEnabled(enable);
}

bool CeleX5::isLoopModeEnabled()
{
	return m_bLoopModeEnabled;
}

// Set the duration of event mode (Mode_A/B/C) when sensor operates in loop mode
// low byte address = 57, width = [7:0]
// high byte address = 58, width = [1:0]
void CeleX5::setEventDuration(uint32_t value)
{
	//Enter CFG Mode
	wireIn(93, 0, 0xFF);
	wireIn(90, 1, 0xFF);

	uint32_t valueH = value >> 8;
	uint32_t valueL = 0xFF & value;
	wireIn(58, valueH, 0xFF);
	wireIn(57, valueL, 0xFF);

	//Enter Start Mode
	wireIn(90, 0, 0xFF);
	wireIn(93, 1, 0xFF);
}

// Set the mumber of pictures to acquire in Mode_D/E/F/G/H
// Mode_D: address = 59, width = [7:0]
// Mode_E: address = 60, width = [7:0]
// Mode_F: address = 61, width = [7:0]
// Mode_G: address = 62, width = [7:0]
// Mode_H: address = 63, width = [7:0]
void CeleX5::setPictureNumber(uint32_t num, CeleX5Mode mode)
{
	//Enter CFG Mode
	wireIn(93, 0, 0xFF);
	wireIn(90, 1, 0xFF);

	if (Full_Picture_Mode == mode)
		wireIn(59, num, 0xFF);
	else if (Full_Optical_Flow_S_Mode == mode)
		wireIn(60, num, 0xFF);
	else if (Full_Optical_Flow_M_Mode == mode)
		wireIn(62, num, 0xFF);

	//Enter Start Mode
	wireIn(90, 0, 0xFF);
	wireIn(93, 1, 0xFF);
}

// related to fps (main clock frequency), hardware parameter
// fps: 20 ~ 160
void CeleX5::setFullPicFrameTime(uint32_t value)
{
	uint32_t fps = 1000 / value;
}

uint32_t CeleX5::getFullPicFrameTime()
{
	return 0;
}

void CeleX5::setEventFrameTime(uint32_t value)
{
	m_pDataProcessThread->getDataProcessor5()->setEventFrameTime(value);
}

uint32_t CeleX5::getEventFrameTime()
{
	return 0;
}

void CeleX5::setOpticalFlowFrameTime(uint32_t value)
{
	;
}

uint32_t CeleX5::getOpticalFlowFrameTime()
{
	return 0;
}

void CeleX5::setEventFrameOverlappedTime(uint32_t msec)
{
	;
}
uint32_t CeleX5::getEventFrameOverlappedTime()
{
	return 0;
}

void CeleX5::setEventFrameParameters(uint32_t frameTime, uint32_t intervalTime)
{
	;
}

// BIAS_EVT_VL : 341 address(2/3)
// BIAS_EVT_DC : 512 address(4/5)
// BIAS_EVT_VH : 683 address(6/7)
void CeleX5::setThreshold(uint32_t value)
{
	m_uiThreshold = value;

	//Enter CFG Mode
	wireIn(93, 0, 0xFF);
	wireIn(90, 1, 0xFF);

	int EVT_VL = 512 - value;
	if (EVT_VL < 0)
		EVT_VL = 0;
	writeRegister(2, -1, 3, EVT_VL);

	int EVT_VH = 512 + value;
	if (EVT_VH > 1023)
		EVT_VH = 1023;
	writeRegister(6, -1, 7, EVT_VH);

	//Enter Start Mode
	wireIn(90, 0, 0xFF);
	wireIn(93, 1, 0xFF);
}

uint32_t CeleX5::getThreshold()
{
	return m_uiThreshold;
}

// Set Contrast
// COL_GAIN: address = 45, width = [1:0]
void CeleX5::setContrast(uint32_t value)
{
	m_uiContrast = value;
	//if (value < 1)
	//	m_uiContrast = 1;
	//else if (value > 3)
	//	m_uiContrast = 3;

	//Enter CFG Mode
	wireIn(93, 0, 0xFF);
	wireIn(90, 1, 0xFF);

	writeRegister(45, -1, -1, m_uiContrast);

	//Enter Start Mode
	wireIn(90, 0, 0xFF);
	wireIn(93, 1, 0xFF);

	m_pDataProcessThread->getDataProcessor5()->setColGainValue(m_uiContrast);
}

uint32_t CeleX5::getContrast()
{
	return m_uiContrast;
}

// Set brightness
// <BIAS_BRT_I>
// high byte address = 22, width = [1:0]
// low byte address = 23, width = [7:0]
void CeleX5::setBrightness(uint32_t value)
{
	m_uiBrightness = value;

	//Enter CFG Mode
	wireIn(93, 0, 0xFF);
	wireIn(90, 1, 0xFF);

	writeRegister(22, -1, 23, value);

	//Enter Start Mode
	wireIn(90, 0, 0xFF);
	wireIn(93, 1, 0xFF);

	m_pDataProcessThread->getDataProcessor5()->setBrightness(value);
}

uint32_t CeleX5::getBrightness()
{
	return m_uiBrightness;
}

uint32_t CeleX5::getClockRate()
{
	return m_uiClockRate;
}

void CeleX5::setClockRate(uint32_t value)
{
	if (value > 100 || value < 20)
		return;
	m_uiClockRate = value;
	if (CeleX5::CeleX5_Parallel == m_emDeviceType)
	{
		//Enter CFG Mode
		wireIn(93, 0, 0xFF);
		wireIn(90, 1, 0xFF);

		// Disable PLL
		wireIn(150, 0, 0xFF);
		// Write PLL Clock Parameter
		wireIn(159, value * 3 / 5, 0xFF);
		// Enable PLL
		wireIn(150, 1, 0xFF);

		//Enter Start Mode
		wireIn(90, 0, 0xFF);
		wireIn(93, 1, 0xFF);
	}
	else if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		//Enter CFG Mode
		wireIn(93, 0, 0xFF);
		wireIn(90, 1, 0xFF);

		// Disable PLL
		wireIn(150, 0, 0xFF);
		int clock[15] = { 20,  30,  40,  50,  60,  70,  80,  90, 100, 110, 120, 130, 140, 150, 160 };

		int PLL_DIV_N[15] = { 12,  18,  12,  15,  18,  21,  12,  18,  15,  22,  18,  26,  21,  30,  24 };
		int PLL_DIV_L[15] = { 2,   3,   2,   2,   2,   2,   2,   3,   2,   3,   2,   3,   2,   3,   2 };
		int PLL_FOUT_DIV1[15] = { 3,   2,   1,   1,   1,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0 };
		int PLL_FOUT_DIV2[15] = { 3,   2,   3,   3,   3,   3,   3,   2,   3,   3,   3,   3,   3,   3,   3 };

		int MIPI_PLL_DIV_I[15] = { 3,   2,   3,   3,   2,   2,   3,   2,   3,   2,   2,   2,   2,   2,   1 };
		//int MIPI_PLL_DIV_N[15] = { 120, 120, 120, 96, 120, 102, 120, 120, 96, 130, 120, 110, 102, 96, 120 };
		//int MIPI_PLL_DIV_N[15] = { 100, 120, 100, 96, 120, 102, 100, 120, 96, 130, 120, 110, 102, 96, 120 };
		int MIPI_PLL_DIV_N[15] = { 180, 180, 180, 144, 180, 153, 180, 180, 144, 195, 180, 165, 153, 144, 180 };

		int index = value / 10 - 2;

		cout << "CeleX5::setClockRate: " << clock[index] << " MHz" << endl;

		// Write PLL Clock Parameter
		writeRegister(159, -1, -1, PLL_DIV_N[index]);
		writeRegister(160, -1, -1, PLL_DIV_L[index]);
		writeRegister(151, -1, -1, PLL_FOUT_DIV1[index]);
		writeRegister(152, -1, -1, PLL_FOUT_DIV2[index]);

		// Enable PLL
		wireIn(150, 1, 0xFF);

		//Disable MIPI
		wireIn(139, 0, 0xFF);
		wireIn(140, 0, 0xFF);
		wireIn(141, 0, 0xFF);
		wireIn(142, 0, 0xFF);
		wireIn(143, 0, 0xFF);
		wireIn(144, 0, 0xFF);

		writeRegister(113, -1, -1, MIPI_PLL_DIV_I[index]);
		writeRegister(115, -1, 114, MIPI_PLL_DIV_N[index]);

		//Enable MIPI
		wireIn(139, 1, 0xFF);
		wireIn(140, 1, 0xFF);
		wireIn(141, 1, 0xFF);
		wireIn(142, 1, 0xFF);
		wireIn(143, 1, 0xFF);
		wireIn(144, 1, 0xFF);

		//Enter Start Mode
		wireIn(90, 0, 0xFF);
		wireIn(93, 1, 0xFF);
	}
}

//0: format 0; 1: format 1; 2: format 2
void CeleX5::setEventDataFormat(int format)
{
	m_iEventDataFormat = format;
}

int CeleX5::getEventDataFormat()
{
	return m_iEventDataFormat;
}

void CeleX5::getFullPicBuffer(unsigned char* buffer)
{
	m_pDataProcessThread->getDataProcessor5()->getFullPicBuffer(buffer);
}

cv::Mat CeleX5::getFullPicMat()
{
	CeleX5ProcessedData* pSensorData = m_pDataProcessThread->getDataProcessor5()->getProcessedData();
	if (pSensorData)
	{
		return cv::Mat(cv::Size(CELEX5_COL, CELEX5_ROW), CV_8UC1, pSensorData->getFullPicBuffer());
	}
	return cv::Mat();
}

void CeleX5::getEventPicBuffer(unsigned char* buffer, emEventPicType type)
{
	m_pDataProcessThread->getDataProcessor5()->getEventPicBuffer(buffer, type);
}

cv::Mat CeleX5::getEventPicMat(emEventPicType type)
{
	CeleX5ProcessedData* pSensorData = m_pDataProcessThread->getDataProcessor5()->getProcessedData();
	if (pSensorData)
	{
		return cv::Mat(cv::Size(CELEX5_COL, CELEX5_ROW), CV_8UC1, pSensorData->getEventPicBuffer(type));
	}
	return cv::Mat();
}

void CeleX5::getOpticalFlowPicBuffer(unsigned char* buffer)
{
	m_pDataProcessThread->getDataProcessor5()->getOpticalFlowPicBuffer(buffer);
}

cv::Mat CeleX5::getOpticalFlowPicMat()
{
	CeleX5ProcessedData* pSensorData = m_pDataProcessThread->getDataProcessor5()->getProcessedData();
	if (pSensorData)
	{
		return cv::Mat(cv::Size(CELEX5_COL, CELEX5_ROW), CV_8UC1, pSensorData->getOpticalFlowPicBuffer());
	}
	return cv::Mat();
}

bool CeleX5::getEventDataVector(std::vector<EventData> &vector)
{
	return true;
}

bool CeleX5::getEventDataVector(std::vector<EventData> &vector, uint64_t& frameNo)
{
	return true;
}

void CeleX5::startRecording(std::string filePath)
{
	m_pDataRecorder->startRecording(filePath);
}

void CeleX5::stopRecording()
{
	if (CeleX5::CeleX5_Parallel == m_emDeviceType)
	{
		m_pDataRecorder->stopRecording(25, 0);
	}
	else if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		BinFileAttributes header;
		if (isLoopModeEnabled())
		{
			header.bLoopMode = 1;
			header.loopA_mode = m_pDataProcessThread->getDataProcessor5()->getSensorLoopMode(1);
			header.loopB_mode = m_pDataProcessThread->getDataProcessor5()->getSensorLoopMode(2);
			header.loopC_mode = m_pDataProcessThread->getDataProcessor5()->getSensorLoopMode(3);
		}
		else
		{
			header.bLoopMode = 0;
			header.loopA_mode = m_pDataProcessThread->getDataProcessor5()->getSensorFixedMode();
			header.loopB_mode = 255;
			header.loopC_mode = 255;
		}
		header.event_data_format = 2;
		m_pDataRecorder->stopRecording(&header);
	}
}

CX5SensorDataServer* CeleX5::getSensorDataServer()
{
	return m_pDataProcessThread->getDataProcessor5()->getSensorDataServer();
}

map<string, vector<CeleX5::CfgInfo>> CeleX5::getCeleX5Cfg()
{
	map<string, vector<HHCommandBase*>> mapCfg = m_pSequenceMgr->getCeleX5Cfg();
	//
	map<string, vector<CeleX5::CfgInfo>> mapCfg1;
	for (auto itr = mapCfg.begin(); itr != mapCfg.end(); itr++)
	{
		//cout << "CelexSensorDLL::getCeleX5Cfg: " << itr->first << endl;
		vector<HHCommandBase*> pCmdList = itr->second;
		vector<CeleX5::CfgInfo> vecCfg;
		for (auto itr1 = pCmdList.begin(); itr1 != pCmdList.end(); itr1++)
		{
			WireinCommandEx* pCmd = (WireinCommandEx*)(*itr1);
			//cout << "----- Register Name: " << pCmd->name() << endl;
			CeleX5::CfgInfo cfgInfo;
			cfgInfo.name = pCmd->name();
			cfgInfo.min = pCmd->minValue();
			cfgInfo.max = pCmd->maxValue();
			cfgInfo.value = pCmd->value();
			cfgInfo.high_addr = pCmd->highAddr();
			cfgInfo.middle_addr = pCmd->middleAddr();
			cfgInfo.low_addr = pCmd->lowAddr();
			vecCfg.push_back(cfgInfo);
		}
		mapCfg1[itr->first] = vecCfg;
	}
	return mapCfg1;
}

map<string, vector<CeleX5::CfgInfo>> CeleX5::getCeleX5CfgModified()
{
	return m_mapCfgModified;
}

void CeleX5::writeRegister(int16_t addressH, int16_t addressM, int16_t addressL, uint32_t value)
{
	if (addressL == -1)
	{
		wireIn(addressH, value, 0xFF);
	}
	else
	{
		if (addressM == -1)
		{
			uint32_t valueH = value >> 8;
			uint32_t valueL = 0xFF & value;
			wireIn(addressH, valueH, 0xFF);
			wireIn(addressL, valueL, 0xFF);
		}
	}
}

CeleX5::CfgInfo CeleX5::getCfgInfoByName(string csrType, string name, bool bDefault)
{
	map<string, vector<CfgInfo>> mapCfg;
	if (bDefault)
		mapCfg = m_mapCfgDefaults;
	else
		mapCfg = m_mapCfgModified;
	CeleX5::CfgInfo cfgInfo;
	for (auto itr = mapCfg.begin(); itr != mapCfg.end(); itr++)
	{
		string tapName = itr->first;
		if (csrType == tapName)
		{
			vector<CfgInfo> vecCfg = itr->second;
			int index = 0;
			for (auto itr1 = vecCfg.begin(); itr1 != vecCfg.end(); itr1++)
			{
				if ((*itr1).name == name)
				{
					cfgInfo = (*itr1);
					return cfgInfo;
				}
				index++;
			}
			break;
		}
	}
	return cfgInfo;
}

void CeleX5::writeCSRDefaults(string csrType)
{
	cout << "CeleX5::writeCSRDefaults: " << csrType << endl;
	m_mapCfgModified[csrType] = m_mapCfgDefaults[csrType];
	for (auto itr = m_mapCfgDefaults.begin(); itr != m_mapCfgDefaults.end(); itr++)
	{
		//cout << "group name: " << itr->first << endl;
		string tapName = itr->first;
		if (csrType == tapName)
		{
			vector<CeleX5::CfgInfo> vecCfg = itr->second;
			for (auto itr1 = vecCfg.begin(); itr1 != vecCfg.end(); itr1++)
			{
				CeleX5::CfgInfo cfgInfo = (*itr1);
				writeRegister(cfgInfo);
			}
			break;
		}
	}
}

void CeleX5::modifyCSRParameter(string csrType, string cmdName, uint32_t value)
{
	CeleX5::CfgInfo cfgInfo;
	for (auto itr = m_mapCfgModified.begin(); itr != m_mapCfgModified.end(); itr++)
	{
		string tapName = itr->first;
		if (csrType.empty())
		{
			vector<CfgInfo> vecCfg = itr->second;
			int index = 0;
			for (auto itr1 = vecCfg.begin(); itr1 != vecCfg.end(); itr1++)
			{
				if ((*itr1).name == cmdName)
				{
					cfgInfo = (*itr1);
					cout << "CeleX5::modifyCSRParameter: Old value = " << cfgInfo.value << endl;
					//modify the value in m_pMapCfgModified
					cfgInfo.value = value;
					vecCfg[index] = cfgInfo;
					m_mapCfgModified[tapName] = vecCfg;
					cout << "CeleX5::modifyCSRParameter: New value = " << cfgInfo.value << endl;
					break;
				}
				index++;
			}
		}
		else
		{
			if (csrType == tapName)
			{
				vector<CfgInfo> vecCfg = itr->second;
				int index = 0;
				for (auto itr1 = vecCfg.begin(); itr1 != vecCfg.end(); itr1++)
				{
					if ((*itr1).name == cmdName)
					{
						cfgInfo = (*itr1);
						cout << "CeleX5::modifyCSRParameter: Old value = " << cfgInfo.value << endl;
						//modify the value in m_pMapCfgModified
						cfgInfo.value = value;
						vecCfg[index] = cfgInfo;
						m_mapCfgModified[tapName] = vecCfg;
						cout << "CeleX5::modifyCSRParameter: New value = " << cfgInfo.value << endl;
						break;
					}
					index++;
				}
				break;
			}
		}
	}
}

bool CeleX5::saveXMLFile()
{
	cout << "CeleX5::saveXMLFile" << endl;
	return m_pSequenceMgr->saveCeleX5XML(m_mapCfgModified);
}

bool CeleX5::initializeFPGA()
{
	return m_pFrontPanel->initializeFPGA("celex5_top.bit");
}

bool CeleX5::configureSettings(CeleX5::DeviceType type)
{
	if (CeleX5::CeleX5_Parallel == type)
	{
		//--------------- Step1 ---------------
		wireIn(94, 1, 0xFF);

		//--------------- Step2 ---------------
		//Disable PLL
		wireIn(150, 0, 0xFF);
		//Load PLL Parameters
		writeCSRDefaults("PLL_Parameters");
		//Enable PLL
		wireIn(150, 1, 0xFF);

		//--------------- Step3 ---------------
		//Enter CFG Mode;
		wireIn(93, 0, 0xFF);
		wireIn(90, 1, 0xFF);

		//Load Other Parameters
		wireIn(92, 12, 0xFF);
		wireIn(220, 0, 0xFF);
		//Load Sensor Core Parameters
		writeCSRDefaults("Sensor_Core_Parameters");

		wireIn(91, 11, 0xFF);
		wireIn(18, 151, 0xFF);

		wireIn(64, 0, 0xFF);
		//sensor mode
		wireIn(53, 0, 0xFF);
		wireIn(54, 3, 0xFF);
		wireIn(55, 4, 0xFF);

		wireIn(57, 60, 0xFF);
		wireIn(58, 0, 0xFF);

		//Enter Start Mode
		wireIn(90, 0, 0xFF);
		wireIn(93, 1, 0xFF);
	}
	else if (CeleX5::CeleX5_MIPI == type)
	{
		setALSEnabled(false);
		if (m_pCeleDriver)
			m_pCeleDriver->openStream();

		//--------------- Step1 ---------------
		wireIn(94, 0, 0xFF); //PADDR_EN

		//--------------- Step2: Load PLL Parameters ---------------
		//Disable PLL
		cout << "--- Disable PLL ---" << endl;
		wireIn(150, 0, 0xFF); //PLL_PD_B
		//Load PLL Parameters
		cout << endl << "--- Load PLL Parameters ---" << endl;
		writeCSRDefaults("PLL_Parameters");
		//Enable PLL
		cout << "--- Enable PLL ---" << endl;
		wireIn(150, 1, 0xFF); //PLL_PD_B

		//--------------- Step3: Load MIPI Parameters ---------------
		cout << endl << "--- Disable MIPI ---" << endl;
		wireIn(139, 0, 0xFF);
		wireIn(140, 0, 0xFF);
		wireIn(141, 0, 0xFF);
		wireIn(142, 0, 0xFF);
		wireIn(143, 0, 0xFF);
		wireIn(144, 0, 0xFF);

		cout << endl << "--- Load MIPI Parameters ---" << endl;
		writeCSRDefaults("MIPI_Parameters");
		writeRegister(115, -1, 114, 120); //MIPI_PLL_DIV_N

		//Enable MIPI
		cout << endl << "--- Enable MIPI ---" << endl;
		wireIn(139, 1, 0xFF);
		wireIn(140, 1, 0xFF);
		wireIn(141, 1, 0xFF);
		wireIn(142, 1, 0xFF);
		wireIn(143, 1, 0xFF);
		wireIn(144, 1, 0xFF);

		//--------------- Step4: ---------------
		cout << endl << "--- Enter CFG Mode ---" << endl;
		wireIn(93, 0, 0xFF);
		wireIn(90, 1, 0xFF);

		//----- Load Sensor Core Parameters -----
		wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR
		writeCSRDefaults("Sensor_Core_Parameters");
		//if (m_bAutoISPEnabled)
		{
			//--------------- for auto isp --------------- 
			wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR
			//Load Sensor Core Parameters
			writeCSRDefaults("Sensor_Core_Parameters");
			writeRegister(22, -1, 23, 140);

			wireIn(220, 1, 0xFF); //AUTOISP_PROFILE_ADDR
			//Load Sensor Core Parameters
			writeCSRDefaults("Sensor_Core_Parameters");
			writeRegister(22, -1, 23, m_arrayBrightness[1]);

			wireIn(220, 2, 0xFF); //AUTOISP_PROFILE_ADDR
			//Load Sensor Core Parameters
			writeCSRDefaults("Sensor_Core_Parameters");
			writeRegister(22, -1, 23, m_arrayBrightness[2]);

			wireIn(220, 3, 0xFF); //AUTOISP_PROFILE_ADDR
			//Load Sensor Core Parameters
			writeCSRDefaults("Sensor_Core_Parameters");
			writeRegister(22, -1, 23, m_arrayBrightness[3]);

			wireIn(221, 0, 0xFF); //AUTOISP_BRT_EN, disable auto ISP
			wireIn(222, 0, 0xFF); //AUTOISP_TEM_EN
			wireIn(223, 0, 0xFF); //AUTOISP_TRIGGER

			writeRegister(225, -1, 224, m_uiAutoISPRefreshTime); //AUTOISP_REFRESH_TIME

			writeRegister(235, -1, 234, m_arrayISPThreshold[0]); //AUTOISP_BRT_THRES1
			writeRegister(237, -1, 236, m_arrayISPThreshold[1]); //AUTOISP_BRT_THRES2
			writeRegister(239, -1, 238, m_arrayISPThreshold[2]); //AUTOISP_BRT_THRES3

			writeRegister(233, -1, 232, 1500); //AUTOISP_BRT_VALUE
		}
		writeCSRDefaults("Sensor_Operation_Mode_Control_Parameters");
		//wireIn(50, 200, 0xFF); //SWITCH_RESET_GAP1
		//wireIn(51, 250, 0xFF); //SWITCH_RESET_GAP2
		//wireIn(52, 200, 0xFF); //SWITCH_RESET_GAP3
		//wireIn(53, 0, 0xFF); //SENSOR_MODE_1
		//wireIn(54, 3, 0xFF); //SENSOR_MODE_2
		//wireIn(55, 4, 0xFF); //SENSOR_MODE_3	
		//wireIn(58, 0, 0xFF); //EVENT_DURATION MSB
		//wireIn(57, 20, 0xFF); //EVENT_DURATION LSB
		//wireIn(64, 0, 0xFF); //SENSOR_MODE_SELECT

		//writeCSRDefaults("Sensor_Data_Transfer_Parameters");
		wireIn(73, m_iEventDataFormat, 0xFF); //EVENT_PACKET_SELECT
		m_pDataProcessThread->getDataProcessor5()->setMIPIDataFormat(m_iEventDataFormat);
		wireIn(74, 254, 0xFF); //MIPI_PIXEL_NUM_EVENT  254 146
		writeRegister(79, -1, 80, 200); //MIPI_ROW_NUM_EVENT
		writeRegister(82, -1, 83, 600); //MIPI_HD_GAP_FULLFRAME 700 500
		writeRegister(84, -1, 85, 600); //MIPI_HD_GAP_EVENT 700 300
		writeRegister(86, -1, 87, 1400); //MIPI_GAP_EOF_SOF  990 1520

		//Enter Start Mode
		cout << endl << "--- Enter Start Mode ---" << endl;
		wireIn(90, 0, 0xFF);
		wireIn(93, 1, 0xFF);
	}
	return true;
}

bool CeleX5::resetConfigureSettings(DeviceType type)
{
	if (CeleX5::CeleX5_Parallel == type)
	{
		//--------------- Step1 ---------------
		wireIn(94, 1, 0xFF);

		//--------------- Step2 ---------------
		//Disable PLL
		wireIn(150, 0, 0xFF);
		//Load PLL Parameters
		writeCSRDefaults("PLL_Parameters");
		//Enable PLL
		wireIn(150, 1, 0xFF);

		//--------------- Step3 ---------------
		//Enter CFG Mode;
		wireIn(93, 0, 0xFF);
		wireIn(90, 1, 0xFF);

		//Load Other Parameters
		wireIn(92, 12, 0xFF);
		wireIn(220, 0, 0xFF);
		//Load Sensor Core Parameters
		writeCSRDefaults("Sensor_Core_Parameters");

		wireIn(91, 11, 0xFF);
		wireIn(18, 151, 0xFF);

		wireIn(64, 0, 0xFF);
		//sensor mode
		wireIn(53, 2, 0xFF);
		wireIn(54, 2, 0xFF);
		wireIn(55, 2, 0xFF);

		wireIn(57, 60, 0xFF);
		wireIn(58, 0, 0xFF);

		//Enter Start Mode
		wireIn(90, 0, 0xFF);
		wireIn(93, 1, 0xFF);
	}
	else if (CeleX5::CeleX5_MIPI == type)
	{
		//cout << endl << "--- Enter CFG Mode ---" << endl;
		wireIn(93, 0, 0xFF);
		wireIn(90, 1, 0xFF);
#ifdef _WIN32
		Sleep(10);
#else
		usleep(1000 * 10);
#endif
		//Enter Start Mode
		//cout << endl << "--- Enter Start Mode ---" << endl;
		wireIn(90, 0, 0xFF);
		wireIn(93, 1, 0xFF);
	}
	return true;
}

void CeleX5::wireIn(uint32_t address, uint32_t value, uint32_t mask)
{
	if (CeleX5::CeleX5_Parallel == m_emDeviceType)
	{
		//DAC_APPLY_OFF
		m_pFrontPanel->wireIn(0x05, 0x00, 0x02);
		//<command name = "set SPI_XXX to #value#">
		m_pFrontPanel->wireIn(0x04, value, 0x00FF); //0x03FF
		//<command name="set DAC_CHANNEL to #fixed value#">
		m_pFrontPanel->wireIn(0x04, (0x800 + address) << 8, 0xFFF00); //0xF000
		cout << "CeleX5::wireIn: address = " << address << ", value = " << value << endl;

		//DAC_APPLY_ON
		m_pFrontPanel->wireIn(0x05, 0x02, 0x02);

		//wire out the  data that have written  
		uint32_t valueOut;
		m_pFrontPanel->wireOut(0x20, 0xFFFF0000, &valueOut);
		cout << hex << "wireout value = " << valueOut << endl;
	}
	else if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		if (m_pCeleDriver)
		{
			if (isAutoISPEnabled())
			{
				setALSEnabled(false);
#ifdef _WIN32
				Sleep(2);
#else
				usleep(1000 * 2);
#endif
			}
			if (m_pCeleDriver->i2c_set(address, value))
			{
				//cout << "CeleX5::wireIn(i2c_set): address = " << address << ", value = " << value << endl;
			}
			if (isAutoISPEnabled())
			{
				setALSEnabled(true);
			}
		}
	}
}

void CeleX5::writeRegister(CfgInfo cfgInfo)
{
	if (cfgInfo.low_addr == -1)
	{
		wireIn(cfgInfo.high_addr, cfgInfo.value, 0xFF);
	}
	else
	{
		if (cfgInfo.middle_addr == -1)
		{
			uint32_t valueH = cfgInfo.value >> 8;
			uint32_t valueL = 0xFF & cfgInfo.value;
			wireIn(cfgInfo.high_addr, valueH, 0xFF);
			wireIn(cfgInfo.low_addr, valueL, 0xFF);
		}
	}
}

void CeleX5::resetPin(bool bReset)
{
	uint32_t address = 0x00;
	uint32_t value = 0;
	uint32_t mask = 0x08;
	if (bReset)
	{
		value = 0x08;
	}	
	m_pFrontPanel->wireIn(address, value, mask);
	cout << "CelexSensorDLL::resetPin: address = " << address << ", value = " << value << ", mask = " << mask << endl;
}

bool CeleX5::openBinFile(std::string filePath)
{
	m_uiPackageCount = 0;
	m_bFirstReadFinished = false;
	if (m_ifstreamPlayback.is_open())
	{
		m_ifstreamPlayback.close();
	}
	m_pDataProcessThread->clearData();
	m_ifstreamPlayback.open(filePath.c_str(), std::ios::binary);
	if (!m_ifstreamPlayback.good())
	{
		cout << "Can't Open File: " << filePath.c_str();
		return false;
	}
	m_pDataProcessThread->setPlayback();
	// read header
	m_ifstreamPlayback.read((char*)&m_stBinFileHeader, sizeof(BinFileAttributes));
	cout << "isLoopMode = " << (int)m_stBinFileHeader.bLoopMode
		 << ", loopA_mode = " << (int)m_stBinFileHeader.loopA_mode << ", loopB_mode = " << (int)m_stBinFileHeader.loopB_mode << ", loopC_mode = " << (int)m_stBinFileHeader.loopC_mode
		 << ", event_data_format = " << (int)m_stBinFileHeader.event_data_format
		 << ", hour = " << (int)m_stBinFileHeader.hour << ", minute = " << (int)m_stBinFileHeader.minute << ", second = " << (int)m_stBinFileHeader.second << endl;
	return true;
}

bool CeleX5::readPlayBackData(long length)
{ 
	//cout << __FUNCTION__ << endl;
	bool eof = false;
	int maxLen = 128 * MAX_PAGE_COUNT;
	int lenToRead = length > maxLen ? maxLen : length;

	if (NULL == m_pReadBuffer)
		m_pReadBuffer = new unsigned char[maxLen];

	while (true && m_pDataProcessThread->queueSize() < 1000000)
	{
		m_ifstreamPlayback.read((char*)m_pReadBuffer, lenToRead);

		int dataLen = m_ifstreamPlayback.gcount();
		if (dataLen > 0)
			m_pDataProcessThread->addData(m_pReadBuffer, dataLen);

		if (m_ifstreamPlayback.eof())
		{
			eof = true;
			//m_ifstreamPlayback.close();
			cout << "Read Playback file Finished!" << endl;
			break;
		}
	}
	return eof;
}

bool CeleX5::readBinFileData()
{
	cout << __FUNCTION__ << endl;
	bool eof = false;
	uint32_t lenToRead = 0;
	m_ifstreamPlayback.read((char*)&lenToRead, 4);
	if (NULL == m_pDataToRead)
		m_pDataToRead = new uint8_t[2048000];
	m_ifstreamPlayback.read((char*)m_pDataToRead, lenToRead);
	//cout << "lenToRead = " << lenToRead << endl;
	//
	int dataLen = m_ifstreamPlayback.gcount();
	if (dataLen > 0)
	{
		uint8_t* dataIn = new uint8_t[dataLen];
		memcpy(dataIn, (uint8_t*)m_pDataToRead, dataLen);
		m_pDataProcessThread->addData(dataIn, dataLen);
		if (!m_bFirstReadFinished)
		{
			m_uiPackageCount += 1;
			if (m_uiPackageCount == 1)
			{
				m_uiPackageCountList[0] = sizeof(BinFileAttributes);
				m_uiPackageCountList[1] = m_uiPackageCountList[0] + lenToRead + 4;
				//cout << "--------------- 0" << m_uiPackageCountList[0] << endl;
				//cout << "--------------- 1" << m_uiPackageCountList[1] << endl;
			}
			else
			{
				m_uiPackageCountList[m_uiPackageCount] = m_uiPackageCountList[m_uiPackageCount-1] + lenToRead + 4;
				//cout << "--------------- i" << m_uiPackageCountList[m_uiPackageCount] << endl;
			}
		}
		cout << "package_count = " << m_uiPackageCount << endl;
	}
	if (m_ifstreamPlayback.eof())
	{
		eof = true;
		m_bFirstReadFinished = true;
		//m_ifstreamPlayback.close();
		cout << "Read Playback file Finished!" << endl;
	}
	return eof;
}

uint32_t CeleX5::getTotalPackageCount()
{
	return m_uiPackageCount;
}

uint32_t CeleX5::getCurrentPackageNo()
{
	return m_pDataProcessThread->packageNo();
}

void CeleX5::setCurrentPackageNo(uint32_t value)
{
	m_ifstreamPlayback.clear();
	m_ifstreamPlayback.seekg(m_uiPackageCountList[value], ios::beg);
	m_pDataProcessThread->setPackageNo(value);
}

CeleX5::BinFileAttributes CeleX5::getBinFileAttributes()
{
	return m_stBinFileHeader;
}

// FLICKER_DETECT_EN: CSR_183
// Flicker detection enable select: 1:enable / 0:disable
void CeleX5::setAntiFlashlightEnabled(bool enabled)
{
	//Enter CFG Mode
	wireIn(93, 0, 0xFF);
	wireIn(90, 1, 0xFF);

	if (enabled)
		writeRegister(183, -1, -1, 1);
	else 
		writeRegister(183, -1, -1, 0);

	//Enter Start Mode
	wireIn(90, 0, 0xFF);
	wireIn(93, 1, 0xFF);
}

void CeleX5::enterTemperature(string strT)
{
	m_pDataProcessThread->getDataProcessor5()->enterTemperature(strT);
}

void CeleX5::setAutoISPEnabled(bool enable)
{
	m_bAutoISPEnabled = enable;
	if (enable)
	{
		//Enter CFG Mode
		wireIn(93, 0, 0xFF);
		wireIn(90, 1, 0xFF);

		wireIn(221, 1, 0xFF); //AUTOISP_BRT_EN, enable auto ISP
		if (isLoopModeEnabled())
			wireIn(223, 1, 0xFF); //AUTOISP_TRIGGER
		else
			wireIn(223, 0, 0xFF); //AUTOISP_TRIGGER

		wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR, Write core parameters to profile0
		writeRegister(233, -1, 232, 1500); //AUTOISP_BRT_VALUE, Set initial brightness value 1500
		writeRegister(22, -1, 23, 80); //BIAS_BRT_I, Override the brightness value in profile0, avoid conflict with AUTOISP profile0		

		//Enter Start Mode
		wireIn(90, 0, 0xFF);
		wireIn(93, 1, 0xFF);

		setALSEnabled(true);
	}
	else
	{
		setALSEnabled(false); //Disable ALS read and write

		//Enter CFG Mode
		wireIn(93, 0, 0xFF);
		wireIn(90, 1, 0xFF);

		//Disable brightness adjustment (auto isp), always load sensor core parameters from profile0
		wireIn(221, 0, 0xFF); //AUTOISP_BRT_EN, disable auto ISP
		if (isLoopModeEnabled())
			wireIn(223, 1, 0xFF); //AUTOISP_TRIGGER
		else
			wireIn(223, 0, 0xFF); //AUTOISP_TRIGGER

		wireIn(220, 0, 0xFF); //AUTOISP_PROFILE_ADDR, Write core parameters to profile0
		writeRegister(233, -1, 232, 1500); //AUTOISP_BRT_VALUE, Set initial brightness value 1500
		writeRegister(22, -1, 23, 140); //BIAS_BRT_I, Override the brightness value in profile0, avoid conflict with AUTOISP profile0
		
		//Enter Start Mode
		wireIn(90, 0, 0xFF);
		wireIn(93, 1, 0xFF);
	}
}

bool CeleX5::isAutoISPEnabled()
{
	return m_bAutoISPEnabled;
}

void CeleX5::setALSEnabled(bool enable)
{
	if (enable)
		m_pCeleDriver->i2c_set(254, 0);
	else
		m_pCeleDriver->i2c_set(254, 2);
}

void CeleX5::setISPThreshold(uint32_t value, int num)
{
	m_arrayISPThreshold[num - 1] = value;
	if (num == 1)
		writeRegister(235, -1, 234, m_arrayISPThreshold[0]); //AUTOISP_BRT_THRES1
	else if (num == 2)
		writeRegister(237, -1, 236, m_arrayISPThreshold[1]); //AUTOISP_BRT_THRES2
	else if (num == 3)
		writeRegister(239, -1, 238, m_arrayISPThreshold[2]); //AUTOISP_BRT_THRES3
}

void CeleX5::setISPBrightness(uint32_t value, int num)
{
	m_arrayBrightness[num - 1] = value;
	wireIn(220, num - 1, 0xFF); //AUTOISP_PROFILE_ADDR
	writeRegister(22, -1, 23, m_arrayBrightness[num - 1]);
}