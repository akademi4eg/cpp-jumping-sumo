#pragma once

extern "C" {
    #include <libARController/ARController.h>
    #include <libARSAL/ARSAL.h>
}

enum LogLevel
{
    ERROR = 'E', WARNING = 'W', INFO = 'I', DEBUG = 'D'
};

int LogWrapper(eARSAL_PRINT_LEVEL level, const char *tag, const char *format, va_list va);

class CDeviceController
{
public:
    CDeviceController();
    virtual ~CDeviceController();

    bool Start();

    bool GoForward(int8_t speed = 50);
    bool GoBackward(int8_t speed = 50);
    bool RotateLeft(int8_t speed = 50);
    bool RotateRight(int8_t speed = 50);
    bool Standby();
    bool Jump(bool bJumpHigh = true);

    static void Log(LogLevel level, const char *cszLogMe, ...);
    static void Log(LogLevel level, const char *cszLogMe, va_list args);
private:
    bool Discover();
    void Cleanup();
    // CALLBACKS
    void StateChanged(eARCONTROLLER_DEVICE_STATE newState);
    void CommandReceived(eARCONTROLLER_DICTIONARY_KEY commandKey, ARCONTROLLER_DICTIONARY_ELEMENT_t *elementDictionary);

private:
    ARDISCOVERY_Device_t *m_pDevice = nullptr;
    ARCONTROLLER_Device_t *m_pController = nullptr;
    // Semaphore for multiprocess device state access.
    ARSAL_Sem_t *m_pStateSem = nullptr;
};
