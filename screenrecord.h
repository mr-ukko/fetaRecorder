#ifndef SCREENRECORD_H
#define SCREENRECORD_H

#pragma once
#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QPointer>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mfreadwrite.h>
#include <QQueue>

struct CompressedFrame {
    std::vector<uint8_t> h264Bytes;
    bool isKeyFrame = false;
    LARGE_INTEGER timestamp = {};
};

class ScreenRecord : public QObject
{
    Q_OBJECT

private:
    QQueue<CompressedFrame> pressedFrameBelt;
    QMutex accessMutex;

    QTimer* m_timer = nullptr;
    int m_frameCount = 0;
    int maxQueueL = 60 * 10;
    LONGLONG m_sampleTime = 0;

    QPointer<ID3D11Device> m_d3d11Device;
    QPointer<ID3D11DeviceContext> m_d3d11Context;
    QPointer<IDXGIOutputDuplication> m_desktopDuplication;
    QPointer<IMFTransform> pEncoderTransform;
    DWORD streamIndex = 0;
    

    QPointer<ID3D11Texture2D> m_readableBgraTexture;
    

    // GPU Video Conversion Resources
    QPointer<ID3D11Texture2D> m_nv12Texture;
    QPointer<ID3D11VideoDevice> m_videoDevice;
    QPointer<ID3D11VideoContext> m_videoContext;
    QPointer<ID3D11VideoProcessor> m_videoProcessor;
    QPointer<ID3D11VideoProcessorEnumerator> m_vpEnum;
    QPointer<IMFSinkWriter> pSinkWriter;
    

public:
    explicit ScreenRecord(QObject *parent = nullptr) 
        : QObject(parent), m_frameCount(0), maxQueueL(60 * 10), m_sampleTime(0) 
    {
        m_timer = new QTimer();
    }

    ~ScreenRecord() override {
        StopRecording();
    }

    bool InitializeDXGI();
    void takeFrame(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* dxgiTexture, LARGE_INTEGER timestamp);
    HRESULT compileFrames(const wchar_t* outputFilePath, uint32_t width, uint32_t height, uint32_t fps);
    void StartRecording(int fps);
    
    Q_INVOKABLE void compress();
    void StopRecording();

private:
    void compressFrame(ID3D11Texture2D* text2D);
    void extractAndQueueCompressedBytes(IMFSample* pSample);
    HRESULT InitializeSinkWriter(const wchar_t* outputFilePath);
    void CaptureNextFrame();
    void drainEncoder();
    
};

#endif // SCREENRECORD_H