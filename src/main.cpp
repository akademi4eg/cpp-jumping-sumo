#include <memory>
#include <termios.h>

#include "CDeviceController.h"

bool g_bShutdown = false;

using STerminos = struct termios;

STerminos init_terminal()
{
    STerminos storedSettings;
    STerminos newSettings;
    tcgetattr(0, &storedSettings);
    newSettings = storedSettings;
    newSettings.c_lflag &= (~ICANON);
    newSettings.c_lflag &= (~ECHO);
    newSettings.c_cc[VTIME] = 0;
    newSettings.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &newSettings);
    return storedSettings;
}

void restore_terminal(STerminos& settings)
{
    tcsetattr(0, TCSANOW, &settings);
}

int main(int argc, char **argv)
{
    CDeviceController::Log(INFO, "Initializing device controller.");
    std::unique_ptr<CDeviceController> pDevice(new CDeviceController());
    pDevice->Start();

    CDeviceController::Log(INFO, "Starting main loop.");
    STerminos previousSettings = init_terminal();
    while (!g_bShutdown)
    {
        int c = getc(stdin);
        switch (c)
        {
            case 'q':
                g_bShutdown = true;
                break;
            case 'w':
                pDevice->GoForward();
                break;
            case 's':
                pDevice->GoBackward();
                break;
            case 'a':
                pDevice->RotateLeft();
                break;
            case 'd':
                pDevice->RotateRight();
                break;
            case ' ':
                pDevice->Jump();
                break;
            default:
                pDevice->Standby();
                break;
        }
    }
    restore_terminal(previousSettings);

    CDeviceController::Log(INFO, "Done!");
    return 0;
}
