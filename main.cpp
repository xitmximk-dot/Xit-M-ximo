#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

constexpr float PI = 3.14159265358979323846f;

struct Vector3 {
    float x{};
    float y{};
    float z{};
};

uintptr_t LocalPlayerPtr = 0x100;
uintptr_t EntityListPtr = 0x200;
uintptr_t ViewAnglesOffset = 0x4D0;

#ifdef _WIN32
using ProcessHandle = HANDLE;
#else
using ProcessHandle = int;
static std::vector<uint8_t> fakeMemory(0x2000);
#endif

template <typename T>
T ReadMemory(ProcessHandle process, uintptr_t address) {
    T value{};
#ifdef _WIN32
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), &value, sizeof(value), &bytesRead) || bytesRead != sizeof(value)) {
        std::cerr << "Falha ao ler memória em 0x" << std::hex << address << std::dec << "\n";
    }
#else
    if (address + sizeof(T) <= fakeMemory.size()) {
        std::memcpy(&value, fakeMemory.data() + address, sizeof(T));
    } else {
        std::cerr << "ReadMemory fora do limite: 0x" << std::hex << address << std::dec << "\n";
    }
#endif
    return value;
}

template <typename T>
bool WriteMemory(ProcessHandle process, uintptr_t address, const T& value) {
#ifdef _WIN32
    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(process, reinterpret_cast<LPVOID>(address), &value, sizeof(value), &bytesWritten) || bytesWritten != sizeof(value)) {
        std::cerr << "Falha ao escrever memória em 0x" << std::hex << address << std::dec << "\n";
        return false;
    }
    return true;
#else
    if (address + sizeof(T) <= fakeMemory.size()) {
        std::memcpy(fakeMemory.data() + address, &value, sizeof(T));
        return true;
    }
    std::cerr << "WriteMemory fora do limite: 0x" << std::hex << address << std::dec << "\n";
    return false;
#endif
}

Vector3 GetBonePos(ProcessHandle process, uintptr_t entityPtr, int boneIndex) {
    uintptr_t boneBase = entityPtr + 0x400;
    uintptr_t boneAddress = boneBase + boneIndex * sizeof(Vector3);
    return ReadMemory<Vector3>(process, boneAddress);
}

Vector3 NormalizeAngles(const Vector3& angles) {
    Vector3 result = angles;
    if (result.x < -89.0f) result.x = -89.0f;
    if (result.x > 89.0f) result.x = 89.0f;
    while (result.y < -180.0f) result.y += 360.0f;
    while (result.y > 180.0f) result.y -= 360.0f;
    result.z = 0.0f;
    return result;
}

Vector3 CalculateAimAngles(const Vector3& from, const Vector3& to) {
    Vector3 delta{to.x - from.x, to.y - from.y, to.z - from.z};
    float distanceHorizontal = std::hypot(delta.x, delta.y);
    Vector3 angles{};
    angles.x = -std::atan2(delta.z, distanceHorizontal) * (180.0f / PI);
    angles.y = std::atan2(delta.y, delta.x) * (180.0f / PI);
    return NormalizeAngles(angles);
}

Vector3 SmoothAngles(const Vector3& current, const Vector3& target, float strength) {
    return {
        current.x + (target.x - current.x) * strength,
        current.y + (target.y - current.y) * strength,
        0.0f,
    };
}

uintptr_t FindTargetEntity(ProcessHandle /*process*/) {
#ifndef _WIN32
    uintptr_t entityAddress = 0x300;
    WriteMemory<uintptr_t>(0, EntityListPtr, entityAddress);
    return entityAddress;
#else
    // Em um projeto real, faça leitura de EntityListPtr e escolha o melhor alvo.
    return 0xBEEFB00F;
#endif
}

void SetupFakeGameMemory() {
#ifndef _WIN32
    Vector3 localPlayerPosition{100.0f, 150.0f, 50.0f};
    Vector3 localView{0.0f, 0.0f, 0.0f};
    Vector3 enemyNeckPosition{130.0f, 180.0f, 55.0f};

    WriteMemory<Vector3>(0, LocalPlayerPtr + 0x10, localPlayerPosition);
    WriteMemory<Vector3>(0, LocalPlayerPtr + ViewAnglesOffset, localView);
    uintptr_t enemyEntity = 0x300;
    WriteMemory<Vector3>(0, enemyEntity + 0x400 + 6 * sizeof(Vector3), enemyNeckPosition);
#endif
}

void AimlockLogic(ProcessHandle process, uintptr_t targetEntity) {
    Vector3 myPos = ReadMemory<Vector3>(process, LocalPlayerPtr + 0x10);
    Vector3 enemyNeck = GetBonePos(process, targetEntity, 6);
    Vector3 currentView = ReadMemory<Vector3>(process, LocalPlayerPtr + ViewAnglesOffset);

    Vector3 desiredView = CalculateAimAngles(myPos, enemyNeck);
    Vector3 finalView = SmoothAngles(currentView, desiredView, 1.0f);

    bool success = WriteMemory<Vector3>(process, LocalPlayerPtr + ViewAnglesOffset, finalView);
    if (success) {
        std::cout << "Novo ângulo aplicado: Pitch=" << finalView.x << " Yaw=" << finalView.y << "\n";
    }
}

int main() {
#ifndef _WIN32
    std::cout << "=== Aimlock Simulation ===\n";
    SetupFakeGameMemory();

    ProcessHandle fakeProcess = 0;
    uintptr_t targetEntity = FindTargetEntity(fakeProcess);
    AimlockLogic(fakeProcess, targetEntity);

    Vector3 applied = ReadMemory<Vector3>(fakeProcess, LocalPlayerPtr + ViewAnglesOffset);
    std::cout << "Saída final (simulada): Pitch=" << applied.x << " Yaw=" << applied.y << "\n";
    return 0;
#else
    std::cout << "=== Aimlock Example ===\n";
    std::cout << "Digite o PID do processo do jogo: ";

    DWORD pid{};
    std::cin >> pid;
    if (pid == 0) {
        std::cerr << "PID inválido.\n";
        return 1;
    }

    ProcessHandle process = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
    if (process == nullptr) {
        std::cerr << "Não foi possível abrir o processo com PID " << pid << ".\n";
        return 1;
    }

    uintptr_t targetEntity = FindTargetEntity(process);
    if (targetEntity == 0) {
        std::cerr << "Nenhum alvo válido encontrado.\n";
        CloseHandle(process);
        return 1;
    }

    AimlockLogic(process, targetEntity);
    CloseHandle(process);
    return 0;
#endif
}
