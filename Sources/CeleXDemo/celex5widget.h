﻿#ifndef CELEX5WIDGET_H
#define CELEX5WIDGET_H

#include "sliderwidget.h"
#include "cfgslider.h"
#include "doubleslider.h"
#include "./include/celex5/celex5.h"
#include "./include/celex5/celex5datamanager.h"
#include <QTime>

using namespace std;

#pragma execution_character_set("utf-8")

class QLabel;
class MainWindow;
class QComboBox;
class SensorDataObserver : public QWidget, public CeleX5DataManager
{
    Q_OBJECT
public:
    SensorDataObserver(CX5SensorDataServer* sensorData, QWidget *parent);
    ~SensorDataObserver();
    virtual void onFrameDataUpdated(CeleX5ProcessedData* pSensorData); //overrides Observer operation
    void setCeleX5(CeleX5* pCeleX5);
    void setLoopModeEnabled(bool enable);
    void setPictureMode(int picMode);
    void setFPS(int count);
    void setFullFrameFPS(uint16_t value);

private:
    void updateImage(unsigned char* pBuffer1, CeleX5::CeleX5Mode mode, int loopNum);

protected:
    void paintEvent(QPaintEvent *event);

protected slots:
    void onUpdateImage();

private:
    QImage         m_imageMode1;
    QImage         m_imageMode2;
    QImage         m_imageMode3;

    CX5SensorDataServer*    m_pSensorData;
    CeleX5*                 m_pCeleX5;
    bool                    m_bLoopModeEnabled;
    int                     m_iPicMode;
    bool                    m_bRealtimeDisplay;
    uint16_t                m_uiTemperature;
    uint16_t                m_uiFullFrameFPS;
    uint16_t                m_uiRealFullFrameFPS;
    int                     m_iFPS;
    uchar*                  m_pBuffer;
    QTimer*                 m_pUpdateTimer;
};

class QHBoxLayout;
class QAbstractButton;
class QPushButton;
class QButtonGroup;
class CeleX5Widget : public QWidget
{
    Q_OBJECT
public:
    explicit CeleX5Widget(QWidget *parent = 0);
    ~CeleX5Widget();
    void closeEvent(QCloseEvent *event);

private:
    void playback();
    QComboBox *createModeComboBox(QString text, QRect rect, QWidget* parent, bool bLoop, int loopNum);
    void createButtons(QHBoxLayout* layout);
    void changeFPN();
    void record();
    void switchMode();
    void showCFG();
    void setSliderMaxValue(QWidget* parent, QString objName, int value);
    int  getSliderMax(QWidget* parent, QString objName);
    void showPlaybackBoard(bool show);

    QPushButton* createButton(QString text, QRect rect, QWidget *parent);
    SliderWidget* createSlider(CeleX5::CfgInfo cfgInfo, int value, QRect rect, QWidget *parent, QWidget *widgetSlot);
    void setButtonEnable(QPushButton* pButton);
    void setButtonNormal(QPushButton* pButton);

signals:

protected slots:
    void onButtonClicked(QAbstractButton* button);
    //
    void onValueChanged(uint32_t value, CfgSlider* slider);
    //
    void onPipeoutDataTimer();
    void onReadBinTimer();
    void onUpdatePlayInfo();
    //
    void onSensorModeChanged(QString text);
    void onImageTypeChanged(int index);

private:
    CeleX5*             m_pCeleX5;
    QWidget*            m_pCFGWidget;
    QButtonGroup*       m_pButtonGroup;
    //
    QComboBox*          m_pCbBoxFixedMode;
    QComboBox*          m_pCbBoxLoopAMode;
    QComboBox*          m_pCbBoxLoopBMode;
    QComboBox*          m_pCbBoxLoopCMode;
    //
    QComboBox*          m_pCbBoxImageType;
    //
    QWidget*            m_pPlaybackBg;
    DoubleSlider*       m_pSliderPlayer;
    CfgSlider*          m_pFPSSlider;

    QLabel*             m_pLabelCurrentTime;
    QLabel*             m_pLabelEndTime;
    QTime               m_timeCurrent;
    //
    QTimer*             m_pPipeOutDataTimer;
    QTimer*             m_pReadBinTimer;
    QTimer*             m_pUpdateTimer;

    SensorDataObserver* m_pSensorDataObserver;
    map<string, vector<CeleX5::CfgInfo>> m_mapCfgDefault;
};

#endif // CELEX5WIDGET_H
