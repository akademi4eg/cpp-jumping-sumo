#include <string>
#include "CDeviceController.h"

const std::string c_sSUMO_HOST = "192.168.2.1";
const int c_iSUMO_PORT = 44444;
const std::string DEVICE_STATES[] = {"STOPPED", "STARTING", "RUNNING", "PAUSED", "STOPPING", "UNKNOWN"};

CDeviceController::CDeviceController()
{
    ARSAL_Sem_Init(m_pStateSem, 0, 0);
    ARSAL_Print_SetCallback(LogWrapper);
    Discover();
}

CDeviceController::~CDeviceController()
{
    Cleanup();
    ARSAL_Sem_Destroy(m_pStateSem);
}

bool CDeviceController::GoForward(int8_t speed)
{
    eARCONTROLLER_ERROR error_flag, error_speed;
    error_flag = m_pController->jumpingSumo->setPilotingPCMDFlag(m_pController->jumpingSumo, 1);
    error_speed = m_pController->jumpingSumo->setPilotingPCMDSpeed(m_pController->jumpingSumo, speed);
    if (error_flag != ARCONTROLLER_OK || error_speed != ARCONTROLLER_OK)
    {
        Log(WARNING, "Failed to send move command!");
        return false;
    }
    return true;
}

bool CDeviceController::GoBackward(int8_t speed)
{
    return GoForward(-speed);
}

bool CDeviceController::RotateRight(int8_t speed)
{
    eARCONTROLLER_ERROR error_flag, error_turn;
    error_flag = m_pController->jumpingSumo->setPilotingPCMDFlag(m_pController->jumpingSumo, 1);
    error_turn = m_pController->jumpingSumo->setPilotingPCMDTurn(m_pController->jumpingSumo, speed);
    if (error_flag != ARCONTROLLER_OK || error_turn != ARCONTROLLER_OK)
    {
        Log(WARNING, "Failed to send rotate command!");
        return false;
    }
    return true;
}

bool CDeviceController::RotateLeft(int8_t speed)
{
    return RotateRight(-speed);
}

bool CDeviceController::Standby()
{
    eARCONTROLLER_ERROR error_flag, error_speed, error_turn;
    error_flag = m_pController->jumpingSumo->setPilotingPCMDFlag(m_pController->jumpingSumo, 0);
    error_speed = m_pController->jumpingSumo->setPilotingPCMDSpeed(m_pController->jumpingSumo, 0);
    error_turn = m_pController->jumpingSumo->setPilotingPCMDTurn(m_pController->jumpingSumo, 0);
    if (error_flag != ARCONTROLLER_OK || error_speed != ARCONTROLLER_OK || error_turn != ARCONTROLLER_OK)
    {
        Log(WARNING, "Failed to send standby command!");
        return false;
    }
    return true;
}

bool CDeviceController::Jump(bool bJumpHigh)
{
    eARCONTROLLER_ERROR error;
    eARCOMMANDS_JUMPINGSUMO_ANIMATIONS_JUMP_TYPE type = bJumpHigh?ARCOMMANDS_JUMPINGSUMO_ANIMATIONS_JUMP_TYPE_HIGH:
                                                                  ARCOMMANDS_JUMPINGSUMO_ANIMATIONS_JUMP_TYPE_LONG;
    error = m_pController->jumpingSumo->sendAnimationsJump(m_pController->jumpingSumo, type);
    if (error != ARCONTROLLER_OK)
    {
        Log(WARNING, "Failed to send jump command!");
        return false;
    }
    return true;
}

bool CDeviceController::Discover()
{
    eARDISCOVERY_ERROR error = ARDISCOVERY_OK;

    Log(DEBUG, "Starting device discovery.");
    m_pDevice = ARDISCOVERY_Device_New(&error);
    if (error != ARDISCOVERY_OK)
    {
        Log(ERROR, "Discovery device creation failed: %s.", ARDISCOVERY_Error_ToString(error));
        return false;
    }

    Log(DEBUG, "Starting WiFi based discovery.");
    error = ARDISCOVERY_Device_InitWifi(m_pDevice, ARDISCOVERY_PRODUCT_JS, "JS", c_sSUMO_HOST.c_str(), c_iSUMO_PORT);
    if (error != ARDISCOVERY_OK)
    {
        Log(ERROR, "WiFi discovery failed: %s.", ARDISCOVERY_Error_ToString(error));
        Cleanup();
        return false;
    }

    eARCONTROLLER_ERROR controller_error = ARCONTROLLER_OK;

    Log(DEBUG, "Creating controller.");
    m_pController = ARCONTROLLER_Device_New (m_pDevice, &controller_error);
    if (controller_error != ARCONTROLLER_OK)
    {
        Log(ERROR, "Failed to create controller.");
        Cleanup();
        return false;
    }

    return true;
}

bool CDeviceController::Start()
{
    eARCONTROLLER_ERROR controller_error;

    Log(INFO, "Starting device!");
    controller_error = ARCONTROLLER_Device_Start(m_pController);
    if (controller_error != ARCONTROLLER_OK)
    {
        Log(ERROR, "Start failed: %s.", ARCONTROLLER_Error_ToString(controller_error));
        Cleanup();
        return false;
    }

    controller_error = ARCONTROLLER_Device_AddStateChangedCallback(
        m_pController, [](eARCONTROLLER_DEVICE_STATE newState, eARCONTROLLER_ERROR error, void *pCustomData) {
            static_cast<CDeviceController *>(pCustomData)->StateChanged(newState);
        }, this);
    if (controller_error != ARCONTROLLER_OK)
    {
        Log(ERROR, "Failed to add state change callback!");
        Cleanup();
        return false;
    }

    controller_error = ARCONTROLLER_Device_AddCommandReceivedCallback(
        m_pController, [](eARCONTROLLER_DICTIONARY_KEY commandKey,
                          ARCONTROLLER_DICTIONARY_ELEMENT_t *elementDictionary, void *pCustomData) {
            static_cast<CDeviceController *>(pCustomData)->CommandReceived(commandKey, elementDictionary);
        }, this);
    if (controller_error != ARCONTROLLER_OK)
    {
        Log(ERROR, "Failed to add command receiving callback!");
        Cleanup();
        return false;
    }

    return true;
}

void CDeviceController::StateChanged(eARCONTROLLER_DEVICE_STATE newState)
{
    Log(DEBUG, "State changed to %s.", DEVICE_STATES[newState].c_str());
    switch (newState)
    {
        case ARCONTROLLER_DEVICE_STATE_STOPPED:
            ARSAL_Sem_Post(m_pStateSem);
            break;

        case ARCONTROLLER_DEVICE_STATE_RUNNING:
            ARSAL_Sem_Post(m_pStateSem);
            break;

        default: // ignore other states
            break;
    }
}

void CDeviceController::CommandReceived(eARCONTROLLER_DICTIONARY_KEY commandKey, ARCONTROLLER_DICTIONARY_ELEMENT_t *elementDictionary)
{
    Log(DEBUG, "Received command %d.", commandKey);
    if (commandKey == ARCONTROLLER_DICTIONARY_KEY_COMMON_COMMONSTATE_BATTERYSTATECHANGED)
    {
        Log(DEBUG, "Battery state changed.");
        ARCONTROLLER_DICTIONARY_ARG_t *arg = nullptr;
        ARCONTROLLER_DICTIONARY_ELEMENT_t *singleElement = nullptr;

        if (elementDictionary)
        {
            // get the command received in the device controller
            HASH_FIND_STR(elementDictionary, ARCONTROLLER_DICTIONARY_SINGLE_KEY, singleElement);
            if (singleElement)
            {
                HASH_FIND_STR (singleElement->arguments,
                               ARCONTROLLER_DICTIONARY_KEY_COMMON_COMMONSTATE_BATTERYSTATECHANGED_PERCENT, arg);
                if (arg)
                    Log(INFO, "Current battery state is %d%%.", int(arg->value.U8));
                else
                    Log(ERROR, "Empty command parameters!");
            }
            else
                Log(ERROR, "Command decoding failed!");
        }
        else
            Log(ERROR, "Empty command!");
    }
}

void CDeviceController::Cleanup()
{
    Log(DEBUG, "Performing cleanup.");
    eARCONTROLLER_ERROR controller_error = ARCONTROLLER_OK;
    eARCONTROLLER_DEVICE_STATE state;
    state = ARCONTROLLER_Device_GetState(m_pController, &controller_error);
    if ((controller_error == ARCONTROLLER_OK) && (state != ARCONTROLLER_DEVICE_STATE_STOPPED))
    {
        Log(INFO, "Stopping device!");
        controller_error = ARCONTROLLER_Device_Stop(m_pController);
        if (controller_error == ARCONTROLLER_OK)
            ARSAL_Sem_Wait(m_pStateSem);
    }
    if (m_pDevice)
    {
        ARDISCOVERY_Device_Delete(&m_pDevice);
        m_pDevice = nullptr;
    }
    if (m_pController)
    {
        ARCONTROLLER_Device_Delete(&m_pController);
        m_pController = nullptr;
    }
}

void CDeviceController::Log(LogLevel level, const char *cszLogMe, ...)
{
    printf("(%c) ", level);
    va_list args;
    va_start(args, cszLogMe);
    vfprintf(stdout, cszLogMe, args);
    va_end(args);
    printf("\n");
}

void CDeviceController::Log(LogLevel level, const char *cszLogMe, va_list args)
{
    printf("(%c) ", level);
    vfprintf(stdout, cszLogMe, args);
    // TODO add proper new line handling
}

int LogWrapper(eARSAL_PRINT_LEVEL level, const char *tag, const char *format, va_list va)
{
    LogLevel logLevel;
    switch (level)
    {
        case ARSAL_PRINT_FATAL:
        case ARSAL_PRINT_ERROR:
            logLevel = ERROR;
            break;
        case ARSAL_PRINT_WARNING:
            logLevel = WARNING;
            break;
        case ARSAL_PRINT_INFO:
            logLevel = INFO;
            break;
        case ARSAL_PRINT_DEBUG:
        case ARSAL_PRINT_VERBOSE:
        default:
            logLevel = DEBUG;
            break;
    }
    std::string message = std::string("[") + tag + std::string("] ") + format;
    CDeviceController::Log(logLevel, message.c_str(), va);
    return 1;
}
