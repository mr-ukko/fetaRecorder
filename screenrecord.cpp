#include "screenrecord.h"

#include <QDebug>
#include <initguid.h>
#include <codecapi.h>
#include <wmcodecdsp.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <Qstring>
#include <QString>

#pragma comment(lib, "mfplat.lib")      
#pragma comment(lib, "mfuuid.lib")      
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

bool ScreenRecord::InitializeDXGI() {
    // STEP 1: Find GPU Adapter
    QPointer<IDXGIFactory1> pFactory;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory);
    if (FAILED(hr)) return false;

    QPointer<IDXGIAdapter1> targetAdapter;
    QPointer<IDXGIOutput> targetOutput;

    for (UINT adapterIdx = 0; pFactory->EnumAdapters1(adapterIdx, &targetAdapter) != DXGI_ERROR_NOT_FOUND; ++adapterIdx) {
        if (targetAdapter->EnumOutputs(0, &targetOutput) != DXGI_ERROR_NOT_FOUND) {
            break;
        }
        targetAdapter->Release();
    }

    if (!targetAdapter || !targetOutput) {
        qCritical() << "Failed to find GPU adapter connected to a display output.";
        return false;
    }

    // STEP 2: Create D3D11 Device
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevel;

    hr = D3D11CreateDevice(
        targetAdapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr, 
        createDeviceFlags, 
        nullptr, 0, 
        D3D11_SDK_VERSION, 
        &m_d3d11Device, 
        &featureLevel, 
        &m_d3d11Context
    );

    if (FAILED(hr) || !m_d3d11Device) {
        qCritical() << "D3D11CreateDevice failed. hr:" << Qt::hex << hr;
        return false;
    }

    // Enable Thread Protection on D3D11 Context
    QPointer<ID3D10Multithread> pMultithread;
    if (SUCCEEDED(m_d3d11Device->QueryInterface(__uuidof(ID3D10Multithread), (void**)&pMultithread))) {
        pMultithread->SetMultithreadProtected(TRUE);
    }

    // STEP 3: Bind Desktop Duplication
    QPointer<IDXGIOutput1> dxgiOutput1;
    hr = targetOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&dxgiOutput1);
    if (FAILED(hr)) return false;

    hr = dxgiOutput1->DuplicateOutput(m_d3d11Device, &m_desktopDuplication);
    if (FAILED(hr)) {
        qCritical() << "DuplicateOutput failed with hr:" << Qt::hex << hr;
        return false;
    }

    // STEP 4: Media Foundation Hardware Encoder Setup
    hr = CoCreateInstance(CLSID_CMSH264EncoderMFT, nullptr, CLSCTX_INPROC_SERVER, IID_IMFTransform, (void**)&pEncoderTransform);
    if (FAILED(hr) || !pEncoderTransform) return false;

    QPointer<IMFDXGIDeviceManager> pDXGIManager;
    UINT resetToken = 0;
    if (SUCCEEDED(MFCreateDXGIDeviceManager(&resetToken, &pDXGIManager))) {
        pDXGIManager->ResetDevice(m_d3d11Device, resetToken);
        pEncoderTransform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, ULONG_PTR(pDXGIManager.p));
    }

    // Output Type (H.264)
    QPointer<IMFMediaType> pOutputType;
    MFCreateMediaType(&pOutputType);
    pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    pOutputType->SetUINT32(MF_MT_AVG_BITRATE, 4000000);
    MFSetAttributeSize(pOutputType, MF_MT_FRAME_SIZE, 1920, 1080);
    MFSetAttributeRatio(pOutputType, MF_MT_FRAME_RATE, 60, 1);
    pOutputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    if (FAILED(pEncoderTransform->SetOutputType(0, pOutputType, 0))) return false;

    // Input Type (NV12)
    QPointer<IMFMediaType> pInputType;
    MFCreateMediaType(&pInputType);
    pInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pInputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(pInputType, MF_MT_FRAME_SIZE, 1920, 1080);
    MFSetAttributeRatio(pInputType, MF_MT_FRAME_RATE, 60, 1);
    pInputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    if (FAILED(pEncoderTransform->SetInputType(0, pInputType, 0))) return false;

    pEncoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    pEncoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    pEncoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

    // STEP 5: Pre-Allocate NV12 Texture with RENDER_TARGET & SHADER_RESOURCE flags
    D3D11_TEXTURE2D_DESC nv12Desc = {};
    nv12Desc.Width = 1920;
    nv12Desc.Height = 1080;
    nv12Desc.MipLevels = 1;
    nv12Desc.ArraySize = 1;
    nv12Desc.Format = DXGI_FORMAT_NV12;
    nv12Desc.SampleDesc.Count = 1;
    nv12Desc.Usage = D3D11_USAGE_DEFAULT;
    nv12Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = m_d3d11Device->CreateTexture2D(&nv12Desc, nullptr, &m_nv12Texture);
    if (FAILED(hr)) return false;

    // STEP 6: Pre-Allocate Readable BGRA Texture
    D3D11_TEXTURE2D_DESC bgraDesc = {};
    bgraDesc.Width = 1920;
    bgraDesc.Height = 1080;
    bgraDesc.MipLevels = 1;
    bgraDesc.ArraySize = 1;
    bgraDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    bgraDesc.SampleDesc.Count = 1;
    bgraDesc.Usage = D3D11_USAGE_DEFAULT;
    bgraDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = m_d3d11Device->CreateTexture2D(&bgraDesc, nullptr, &m_readableBgraTexture);
    if (FAILED(hr)) return false;

    // STEP 7: Video Device and Processor Setup
    hr = m_d3d11Device->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&m_videoDevice);
    HRESULT hrCtx = m_d3d11Context->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&m_videoContext);

    if (FAILED(hr) || FAILED(hrCtx)) return false;

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC vpDesc = {};
    vpDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    vpDesc.InputWidth = 1920;
    vpDesc.InputHeight = 1080;
    vpDesc.OutputWidth = 1920;
    vpDesc.OutputHeight = 1080;
    vpDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    hr = m_videoDevice->CreateVideoProcessorEnumerator(&vpDesc, &m_vpEnum);
    if (FAILED(hr)) return false;

    hr = m_videoDevice->CreateVideoProcessor(m_vpEnum, 0, &m_videoProcessor);
    if (FAILED(hr)) return false;

    return true;
}

void ScreenRecord::takeFrame(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* dxgiTexture, LARGE_INTEGER timestamp) {
    if (!context || !m_videoContext || !m_videoProcessor || !m_vpEnum || !m_nv12Texture || !dxgiTexture) return;

    // 1. Copy desktop surface to BGRA processing texture
    context->CopyResource(m_readableBgraTexture, dxgiTexture);

    // 2. Setup Video Processor Views
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = {};
    inputViewDesc.FourCC = 0;
    inputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inputViewDesc.Texture2D.MipSlice = 0;

    QPointer<ID3D11VideoProcessorInputView> inputView;
    HRESULT hr = m_videoDevice->CreateVideoProcessorInputView(m_readableBgraTexture, m_vpEnum, &inputViewDesc, &inputView);
    if (FAILED(hr)) {
        qWarning() << "CreateVideoProcessorInputView failed with hr:" << Qt::hex << hr;
        return;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = {};
    outputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outputViewDesc.Texture2D.MipSlice = 0;

    QPointer<ID3D11VideoProcessorOutputView> outputView;
    hr = m_videoDevice->CreateVideoProcessorOutputView(m_nv12Texture, m_vpEnum, &outputViewDesc, &outputView);
    if (FAILED(hr)) {
        qWarning() << "CreateVideoProcessorOutputView failed with hr:" << Qt::hex << hr;
        return;
    }

    // 3. Convert BGRA to NV12 via GPU Video Processor Blt
    D3D11_VIDEO_PROCESSOR_STREAM stream = {};
    stream.Enable = TRUE;
    stream.pInputSurface = inputView;

    hr = m_videoContext->VideoProcessorBlt(m_videoProcessor, outputView, 0, 1, &stream);
    if (SUCCEEDED(hr)) {
        compressFrame(m_nv12Texture);
    } else {
        qWarning() << "VideoProcessorBlt failed with hr:" << Qt::hex << hr;
    }
}

void ScreenRecord::compressFrame(ID3D11Texture2D* text2D) {
    if (!pEncoderTransform || !text2D) return;

    // STEP 1: Wrap NV12 Texture in an IMFMediaBuffer
    QPointer<IMFMediaBuffer> pBuffer;
    HRESULT hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), text2D, 0, FALSE, &pBuffer);
    if (FAILED(hr) || !pBuffer) {
        qWarning() << "MFCreateDXGISurfaceBuffer failed with hr:" << Qt::hex << hr;
        return;
    }

    // STEP 2: Create IMFSample and attach timing metadata
    QPointer<IMFSample> pSample;
    hr = MFCreateSample(&pSample);
    if (FAILED(hr) || !pSample) return;

    pSample->AddBuffer(pBuffer);

    m_sampleTime += 166666; // ~60 FPS in 100-ns units
    pSample->SetSampleTime(m_sampleTime);
    pSample->SetSampleDuration(166666);

    // STEP 3: Pass Sample to Hardware MFT Encoder
    hr = pEncoderTransform->ProcessInput(0, pSample, 0);
    if (hr == MF_E_NOTACCEPTING) {
        drainEncoder();
        hr = pEncoderTransform->ProcessInput(0, pSample, 0);
    }

    if (FAILED(hr)) {
        qWarning() << "ProcessInput failed with hr:" << Qt::hex << hr;
        return;
    }

    // STEP 4: Retrieve Encoded H.264 Packets
    drainEncoder();
}

void ScreenRecord::extractAndQueueCompressedBytes(IMFSample* pSample) {
    if (!pSample) return;

    QPointer<IMFMediaBuffer> pMediaBuffer;
    HRESULT hr = pSample->GetBufferByIndex(0, &pMediaBuffer);
    if (FAILED(hr) || !pMediaBuffer) return;

    BYTE* pRawData = nullptr;
    DWORD currentLength = 0;
    hr = pMediaBuffer->Lock(&pRawData, nullptr, &currentLength);

    if (SUCCEEDED(hr) && currentLength > 0 && pRawData != nullptr) {
        CompressedFrame frame;
        frame.h264Bytes.assign(pRawData, pRawData + currentLength);

        LONGLONG sampleTime = 0;
        pSample->GetSampleTime(&sampleTime);
        frame.timestamp.QuadPart = sampleTime;

        UINT32 isKeyframe = 0;
        pSample->GetUINT32(MFSampleExtension_CleanPoint, &isKeyframe);
        frame.isKeyFrame = (isKeyframe != 0);

        {
            accessMutex.lock();
            if (pressedFrameBelt.size() >= static_cast<size_t>(maxQueueL)) {
                pressedFrameBelt.pop_back();
            }
            pressedFrameBelt.push_front(frame);
        }
    }

    pMediaBuffer->Unlock();
}

void ScreenRecord::drainEncoder() {
    if (!pEncoderTransform) return;

    MFT_OUTPUT_STREAM_INFO streamInfo = {};
    pEncoderTransform->GetOutputStreamInfo(0, &streamInfo);

    while (true) {
        MFT_OUTPUT_DATA_BUFFER outputContainer = {};
        outputContainer.dwStreamID = 0;

        QPointer<IMFSample> pOutSample;
        if (!(streamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
            MFCreateSample(&pOutSample);
            QPointer<IMFMediaBuffer> pOutBuf;
            DWORD bufSize = (streamInfo.cbSize > 0) ? streamInfo.cbSize : (1024 * 1024);
            MFCreateMemoryBuffer(bufSize, &pOutBuf);
            pOutSample->AddBuffer(pOutBuf);
            outputContainer.pSample = pOutSample;
        }

        DWORD status = 0;
        HRESULT mftHr = pEncoderTransform->ProcessOutput(0, 1, &outputContainer, &status);

        if (mftHr == MF_E_TRANSFORM_NEED_MORE_INPUT || FAILED(mftHr)) break;

        IMFSample* sampleToProcess = outputContainer.pSample ? outputContainer.pSample : pOutSample.p;
        extractAndQueueCompressedBytes(sampleToProcess);
    }
}

HRESULT ScreenRecord::InitializeSinkWriter(const wchar_t* outputFilePath) {
    QPointer<IMFAttributes> pAttributes;
    MFCreateAttributes(&pAttributes, 2);
    pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    pAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);

    HRESULT hr = MFCreateSinkWriterFromURL(outputFilePath, nullptr, pAttributes, &pSinkWriter);
    if (FAILED(hr)) return hr;

    // 1. MP4 Output Container Type
    QPointer<IMFMediaType> pMediaType;
    MFCreateMediaType(&pMediaType);
    pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    pMediaType->SetUINT32(MF_MT_AVG_BITRATE, 4000000);
    MFSetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, 1920, 1080);
    MFSetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, 60, 1);
    pMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    hr = pSinkWriter->AddStream(pMediaType, &streamIndex);
    if (FAILED(hr)) return hr;

    // 2. Set identical input type for pre-compressed H.264 passthrough
    hr = pSinkWriter->SetInputMediaType(streamIndex, pMediaType, nullptr);
    if (FAILED(hr)) return hr;

    return pSinkWriter->BeginWriting();
}

HRESULT ScreenRecord::compileFrames(const wchar_t* outputFilePath, uint32_t width, uint32_t height, uint32_t fps) {
    // 1. Flush any lingering encoder frames into the queue
    if (pEncoderTransform) {
        pEncoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        drainEncoder();
    }

    accessMutex.lock();
    if (pressedFrameBelt.empty()) {
        qWarning() << "Cannot write MP4: Frame queue is empty!";
        return E_FAIL;
    }

    // Discard leading non-keyframes so video playback begins smoothly
    while (!pressedFrameBelt.empty() && !pressedFrameBelt.front().isKeyFrame) {
        pressedFrameBelt.pop_back();
    }

    if (pressedFrameBelt.empty()) {
        qWarning() << "No keyframes were captured in the stream!";
        return E_FAIL;
    }

    HRESULT hr = InitializeSinkWriter(outputFilePath);
    if (FAILED(hr)) {
        qWarning() << "InitializeSinkWriter failed with hr:" << Qt::hex << hr;
        return hr;
    }

    LONGLONG writeTimestamp = 0;
    LONGLONG frameDuration = 10000000 / fps;
    int writtenCount = 0;

    while (!pressedFrameBelt.empty()) {
        CompressedFrame frame = std::move(pressedFrameBelt.front());
        pressedFrameBelt.pop_back();

        if (frame.h264Bytes.empty()) continue;

        QPointer<IMFMediaBuffer> pBuffer;
        hr = MFCreateMemoryBuffer(static_cast<DWORD>(frame.h264Bytes.size()), &pBuffer);
        if (FAILED(hr)) break;

        BYTE* pDest = nullptr;
        if (SUCCEEDED(pBuffer->Lock(&pDest, nullptr, nullptr))) {
            memcpy(pDest, frame.h264Bytes.data(), frame.h264Bytes.size());
            pBuffer->Unlock();
            pBuffer->SetCurrentLength(static_cast<DWORD>(frame.h264Bytes.size()));
        }

        QPointer<IMFSample> pSample;
        hr = MFCreateSample(&pSample);
        if (FAILED(hr)) break;
        pSample->AddBuffer(pBuffer);

        pSample->SetSampleTime(writeTimestamp);
        pSample->SetSampleDuration(frameDuration);
        writeTimestamp += frameDuration;

        if (frame.isKeyFrame) {
            pSample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
        }

        hr = pSinkWriter->WriteSample(streamIndex, pSample);
        if (FAILED(hr)) break;

        writtenCount++;
    }

    if (writtenCount > 0) {
        hr = pSinkWriter->Finalize();
        qDebug() << "Successfully created video with" << writtenCount << "frames.";
    }

    return hr;
}

void ScreenRecord::compress() {
    StopRecording();

    char* userpath = getenv("USERPROFILE");
    if (!userpath) {
        qWarning() << "No user path found";
        return;
    }

    std::string path = std::string(userpath) + "\\Pictures\\Videos\\Testt.mp4";
    std::wstring wPath = QString::fromStdString(path).toStdWString();

    compileFrames(wPath.c_str(), 1920, 1080, 60);
}

void ScreenRecord::StartRecording(int fps) {
    if (!m_desktopDuplication) return;

    int intervalMs = 1000 / fps;
    connect(m_timer, &QTimer::timeout, this, &ScreenRecord::CaptureNextFrame);
    m_timer->start(intervalMs);
    qDebug() << "Recording started at" << fps << "FPS.";
}

void ScreenRecord::StopRecording() {
    if (m_timer->isActive()) {
        m_timer->stop();
        m_timer->disconnect();
    }
}

void ScreenRecord::CaptureNextFrame() {
    if (!m_desktopDuplication) return;

    DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
    QPointer<IDXGIResource> desktopResource;

    HRESULT hr = m_desktopDuplication->AcquireNextFrame(10, &frameInfo, &desktopResource);

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        // Feed previous NV12 texture to encoder during static screen content
        if (m_nv12Texture) {
            compressFrame(m_nv12Texture);
        }
        return;
    } else if (FAILED(hr) || !desktopResource) {
        return;
    }

    QPointer<ID3D11Texture2D> frameTexture;
    hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&frameTexture);

    if (SUCCEEDED(hr) && frameTexture) {
        takeFrame(m_d3d11Device, m_d3d11Context, frameTexture, frameInfo.LastPresentTime);
    }

    m_desktopDuplication->ReleaseFrame();
}