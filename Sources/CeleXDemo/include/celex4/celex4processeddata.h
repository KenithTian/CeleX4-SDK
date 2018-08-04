#ifndef CELEX4_PROCESSED_DATA_H
#define CELEX4_PROCESSED_DATA_H

#include "celex4.h"

class CeleX4ProcessedData
{
public:
	CeleX4ProcessedData();
	~CeleX4ProcessedData();

	inline void setMeanIntensity(int value) { m_nMeanIntensity = value; }
	inline void setFullPicBuffer(unsigned char* buffer) { m_pFullPicBuffer = buffer; }
	inline void setEventOpticalFlow(unsigned char* buffer) { m_pEventOpticalFlow = buffer; }
	inline void setEventOpticalFlowDirection(unsigned char* buffer) { m_pEventOpticalFlowDirection = buffer; }
	inline void setEventOpticalFlowSpeed(unsigned char* buffer) { m_pEventOpticalFlowSpeed = buffer; }

	//----------------save vector for a frame-------------
	inline void setEventDataVector(std::vector<EventData> eventData) { m_vectorEventData.swap(eventData); }
	inline std::vector<EventData> getEventDataVector() { return m_vectorEventData; }

	inline unsigned char* getFullPicBuffer() { return m_pFullPicBuffer; }
	inline unsigned char* getEventPicBuffer(emEventPicMode mode)
	{
		switch (mode)
		{
		case EventBinaryPic:
			return m_pEventBinaryPic;
		case EventAccumulatedPic:
			return m_pEventAccumulatedPic;
		case EventGrayPic:
			return m_pEventGrayPic;
		case EventSuperimposedPic:
			return m_pEventSuperimposedPic;
		case EventDenoisedBinaryPic:
			return m_pEventDenoisedBinaryPic;
		case EventDenoisedGrayPic:
			return m_pEventDenoisedGrayPic;
		case EventCountPic:
			return m_pEventCountPic;;
		default:
			break;
		}
		return NULL;
	}
	inline unsigned char* getEventOpticalFlow() { return m_pEventOpticalFlow; }
	inline unsigned char* getEventOpticalFlowDirection() { return m_pEventOpticalFlowDirection; }
	inline unsigned char* getEventOpticalFlowSpeed() { return m_pEventOpticalFlowSpeed; }

private:
	std::vector<EventData> m_vectorEventData;	//-----------------------------
	unsigned char*    m_pFullPicBuffer;
	unsigned char*    m_pEventBinaryPic;
	unsigned char*    m_pEventAccumulatedPic;
	unsigned char*    m_pEventGrayPic;
	unsigned char*    m_pEventSuperimposedPic;
	unsigned char*    m_pEventDenoisedBinaryPic;
	unsigned char*    m_pEventDenoisedGrayPic;
	unsigned char*    m_pEventOpticalFlow;
	unsigned char*    m_pEventOpticalFlowDirection;
	unsigned char*    m_pEventOpticalFlowSpeed;
	unsigned char*	  m_pEventCountPic; 
	int               m_nMeanIntensity;
};

#endif // CELEX4_PROCESSED_DATA_H
