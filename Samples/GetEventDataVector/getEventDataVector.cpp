/*
* Copyright (c) 2017-2018 CelePixel Technology Co. Ltd. All Rights Reserved
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

#include <opencv2/opencv.hpp>
#include <celex4/celex4.h>
#include <vector>
#define MAT_ROWS 640
#define MAT_COLS 768
#define BIN_FILE	"YOUR_BIN_FILE_PATH"
#define FPN_PATH    "../Samples/config/FPN.txt"

int main()
{
	CeleX4 *pCelex = new CeleX4;
	pCelex->openSensor("");
	pCelex->setSensorMode(EventMode);
	pCelex->setFpnFile(FPN_PATH);	//load FPN file for denoising
	//pCelex->openPlaybackFile(BIN_FILE);
	cv::Mat mat = cv::Mat::zeros(cv::Size(MAT_COLS, MAT_ROWS), CV_8UC1);
	while (true)
	{
		//get event data is available for both offline bin file and real-time 
		//pCelex->readPlayBackData();
		pCelex->pipeOutFPGAData();
		std::vector<EventData> v;	//vector to store the event data
		if (pCelex->getEventDataVector(v))	//get the event data and show corrospodding buffer
		{
			for (int i = 0; i < v.size() - 1; ++i)
			{
				mat.at<uchar>(MAT_ROWS - v[i].row - 1, MAT_COLS - v[i].col - 1) = v[i].brightness;
			}
		}
		cv::imshow("show", mat);
		cv::waitKey(10);
	}
	return 1;
}