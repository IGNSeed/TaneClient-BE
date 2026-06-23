#include <Windows.h>

namespace tane::launcher {

int RunLauncher(HINSTANCE instance, int showCommand);

}  // namespace tane::launcher

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    return tane::launcher::RunLauncher(instance, showCommand);
}
