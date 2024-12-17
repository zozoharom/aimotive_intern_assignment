#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "implot.h"
#include <d3d9.h>
#include <tchar.h>
#include <cmath>

#include <iostream> 
#include <fstream>   
#include <vector>    
#include <string>    
#include <sstream>  
#include <iomanip>   
#include <exception> 
#include <algorithm> 

static LPDIRECT3D9              g_pD3D = nullptr;
static LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
static bool                     g_DeviceLost = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

std::vector<std::vector<double>> yVals; 
std::vector<double> xVals;             

bool IsIMU = true;

// IMU 
std::vector<double> xValsIMU;                      
std::vector<std::vector<double>> yValsIMU;       
std::vector<double> gyro_x, gyro_y, gyro_z;        
std::vector<double> acc_x, acc_y, acc_z;        
std::vector<double> conf_gyro, conf_acc;         

// DBW 
std::vector<double> xValsDBW;                      
std::vector<std::vector<double>> yValsDBW;         
std::vector<double> speed;                         
std::vector<double> yaw_rate;                      
std::vector<double> v_front_left, v_front_right;   
std::vector<double> v_rear_left, v_rear_right;     

void FileOpenError(const std::string& path);
std::vector<std::string> ReadFromCSV(const std::string& path);
void ParseCSVtoDouble(const std::vector<std::string>& csvRecordStrings);

void FileOpenError(const std::string& path) 
{
    std::cout << "Could not open file: '" << path << "'" << std::endl;
    std::cin.get();
    exit(EXIT_FAILURE);
}

std::vector<std::string> ReadFromCSV(const std::string& path) 
{
    std::string totalCSVContentsString;

    std::ostringstream myStringStream = std::ostringstream{};

    std::ifstream inputFile(path);
    if (!inputFile.is_open()) 
    {
        FileOpenError(path);
    }

    myStringStream << inputFile.rdbuf();

    totalCSVContentsString = myStringStream.str();

    std::istringstream myCSVStringStream(totalCSVContentsString);
    std::vector<std::string> records;
    std::string record;

    while (std::getline(myCSVStringStream, record)) 
    {
        records.push_back(record);
    }

    std::cout << "Number of rows " << path << " = " << records.size() << std::endl;

    return records;
}

void ParseCSVtoDouble(const std::vector<std::string>& csvRecordStrings,
    std::vector<double>& xVals,
    std::vector<std::vector<double>>& yVals,
    bool IsIMU)
{
    size_t expectedColumns = IsIMU ? 9 : 7;

    for (size_t i = 0; i < csvRecordStrings.size(); i++)
    {
        if (i == 0) 
        {
            std::cout << "Header row: " << csvRecordStrings[i] << std::endl;
            continue;
        }

        std::stringstream sstream(csvRecordStrings[i]);
        std::string value;
        std::vector<double> currentRow;

        while (std::getline(sstream, value, ','))
        {
            try {
                currentRow.push_back(std::stod(value));
            }
            catch (const std::exception& e)
            {
                std::cerr << "Invalid value at line " << i + 1 << ": '" << value << "'\n";
                currentRow.push_back(std::numeric_limits<double>::quiet_NaN());
            }
        }

        if (currentRow.size() != expectedColumns)
        {
            std::cerr << "Error: Line " << i + 1 << " has " << currentRow.size()
                << " columns, expected " << expectedColumns << ". Skipping line.\n";
            continue;
        }

        xVals.push_back(currentRow[0]);
        currentRow.erase(currentRow.begin());
        yVals.push_back(currentRow);
    }

    if (IsIMU) //imu
    {
        for (size_t i = 0; i < xVals.size(); i++)
        {
            std::cout << "X[" << i << "] = " << xVals[i] << "\tY[" << i << "] = ";

            for (size_t j = 0; j < yVals[i].size(); j++)
            {
                std::cout << yVals[i][j];
                if (j < yVals[i].size() - 1)
                    std::cout << ", ";

                switch (j)
                {
                case 0: gyro_x.push_back(yVals[i][j]); break;
                case 1: gyro_y.push_back(yVals[i][j]); break;
                case 2: gyro_z.push_back(yVals[i][j]); break;
                case 3: acc_x.push_back(yVals[i][j]); break;
                case 4: acc_y.push_back(yVals[i][j]); break;
                case 5: acc_z.push_back(yVals[i][j]); break;
                case 6: conf_gyro.push_back(yVals[i][j]); break;
                case 7: conf_acc.push_back(yVals[i][j]); break;
                }
            }
            std::cout << std::endl;
        }

        double last_valid = 1.0;
        for (size_t i = 0; i < conf_gyro.size(); i++) 
        {
            if (std::isnan(conf_gyro[i])) 
            {
                std::cout << "conf_gyro missing element detected at index " << i << std::endl;
                conf_gyro[i] = last_valid;
            }
            else 
            {
                last_valid = conf_gyro[i];
            }
        }

        auto min_max_gyro_x = std::minmax_element(gyro_x.begin(), gyro_x.end());
        std::cout << "gyro_x Min: " << *(min_max_gyro_x.first) << ", Max: " << *(min_max_gyro_x.second) << std::endl;

        auto min_max_acc_x = std::minmax_element(acc_x.begin(), acc_x.end());
        std::cout << "acc_x Min: " << *(min_max_acc_x.first) << ", Max: " << *(min_max_acc_x.second) << std::endl;

    }
    else //dbw
    {
        for (size_t i = 0; i < xVals.size(); i++)
        {
            std::cout << "X[" << i << "] = " << xVals[i] << "\tY[" << i << "] = ";

            for (size_t j = 0; j < yVals[i].size(); j++)
            {
                std::cout << yVals[i][j];
                if (j < yVals[i].size() - 1)
                    std::cout << ", ";

                switch (j)
                {
                case 0: speed.push_back(yVals[i][j]); break;
                case 1: yaw_rate.push_back(yVals[i][j]); break;
                case 2: v_front_left.push_back(yVals[i][j]); break;
                case 3: v_front_right.push_back(yVals[i][j]); break;
                case 4: v_rear_left.push_back(yVals[i][j]); break;
                case 5: v_rear_right.push_back(yVals[i][j]); break;
                }
            }
            std::cout << std::endl;
        }

        auto min_max_speed = std::minmax_element(speed.begin(), speed.end());
        std::cout << "Speed Min: " << *(min_max_speed.first) << ", Max: " << *(min_max_speed.second) << std::endl;

        auto min_max_yaw_rate = std::minmax_element(yaw_rate.begin(), yaw_rate.end());
        std::cout << "Yaw Rate Min: " << *(min_max_yaw_rate.first) << ", Max: " << *(min_max_yaw_rate.second) << std::endl;

    }
}

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int main(int, char**)
{
    std::vector<std::string> imuRecords = ReadFromCSV("imu.csv");
    ParseCSVtoDouble(imuRecords, xValsIMU, yValsIMU, true);

    std::vector<std::string> dbwRecords = ReadFromCSV("dbw.csv");
    ParseCSVtoDouble(dbwRecords, xValsDBW, yValsDBW, false);

    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Aimotive Intern Task", WS_OVERLAPPEDWINDOW, 100, 100, 1500, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();  
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

      ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(195, 177, 225, 255);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    bool testbool = false;
    int inttest = 10;

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle lost D3D9 device
        if (g_DeviceLost)
        {
            HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST)
            {
                ::Sleep(10);
                continue;
            }
            if (hr == D3DERR_DEVICENOTRESET)
                ResetDevice();
            g_DeviceLost = false;
        }

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            g_d3dpp.BackBufferWidth = g_ResizeWidth;
            g_d3dpp.BackBufferHeight = g_ResizeHeight;
            g_ResizeWidth = g_ResizeHeight = 0;
            ResetDevice();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ImPlot ------------------------------------------------------------------------------------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver); 
        ImGui::SetNextWindowSize(ImVec2(960, 540), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Gyroscope"))
        {
            if (ImPlot::BeginPlot("IMU Gyroscope Data", ImVec2(-1, 0)))
            {
                ImPlot::SetupAxes("Time", "Gyroscope Values", ImPlotAxisFlags_None, ImPlotAxisFlags_None);
                ImPlot::SetupAxis(ImAxis_Y2, "Gyro Confidence", ImPlotAxisFlags_None);
                ImPlot::SetupAxisLimits(ImAxis_Y2, 0.0, 1.0);

                ImPlot::PlotLine("Gyro X", xValsIMU.data(), gyro_x.data(), xValsIMU.size());
                ImPlot::PlotLine("Gyro Y", xValsIMU.data(), gyro_y.data(), xValsIMU.size());
                ImPlot::PlotLine("Gyro Z", xValsIMU.data(), gyro_z.data(), xValsIMU.size());

                ImPlot::SetAxis(ImAxis_Y2);
                ImPlot::PlotLine("Gyro Confidence", xValsIMU.data(), conf_gyro.data(), xValsIMU.size());

                ImPlot::EndPlot();
            }
            ImGui::End();
        }

        ImGui::SetNextWindowPos(ImVec2(0, 960), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(960, 540), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Acceleration"))
        {
            if (ImPlot::BeginPlot("IMU Acceleration Data", ImVec2(-1, 0)))
            {
                ImPlot::SetupAxes("Time", "Acceleration Values", ImPlotAxisFlags_None, ImPlotAxisFlags_None);
                ImPlot::SetupAxis(ImAxis_Y2, "Acceleration Confidence", ImPlotAxisFlags_None);
                ImPlot::SetupAxisLimits(ImAxis_Y2, 0.0, 1.0);

                ImPlot::PlotLine("Acc X", xValsIMU.data(), acc_x.data(), xValsIMU.size());
                ImPlot::PlotLine("Acc Y", xValsIMU.data(), acc_y.data(), xValsIMU.size());
                ImPlot::PlotLine("Acc Z", xValsIMU.data(), acc_z.data(), xValsIMU.size());

                ImPlot::SetAxis(ImAxis_Y2);
                ImPlot::PlotLine("Acc Confidence", xValsIMU.data(), conf_acc.data(), xValsIMU.size());

                ImPlot::EndPlot();
            }
            ImGui::End();
        }

        ImGui::SetNextWindowPos(ImVec2(540, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(960, 540), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Vehicle Speed and Yaw Rate"))
        {
            if (ImPlot::BeginPlot("DBW Speed and Yaw Rate", ImVec2(-1, 0)))
            {
                ImPlot::SetupAxes("Time", "Speed", ImPlotAxisFlags_None, ImPlotAxisFlags_None);
                ImPlot::SetupAxis(ImAxis_Y2, "Yaw Rate", ImPlotAxisFlags_None);
                ImPlot::SetupAxisLimits(ImAxis_Y2, -0.02, 0.1);

                ImPlot::PlotLine("Vehicle Speed", xValsDBW.data(), speed.data(), xValsDBW.size());
                ImPlot::SetAxis(ImAxis_Y2);
                ImPlot::PlotLine("Yaw Rate", xValsDBW.data(), yaw_rate.data(), xValsDBW.size());

                ImPlot::EndPlot();
            }
            ImGui::End();
        }

        ImGui::SetNextWindowPos(ImVec2(540, 960), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(960, 540), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Wheel Speed"))
        {
            if (ImPlot::BeginPlot("DBW Wheel Speeds", ImVec2(-1, 0)))
            {
                ImPlot::SetupAxes("Time", "Wheel Speeds", ImPlotAxisFlags_None, ImPlotAxisFlags_None);

                ImPlot::PlotLine("Front Left", xValsDBW.data(), v_front_left.data(), xValsDBW.size());
                ImPlot::PlotLine("Front Right", xValsDBW.data(), v_front_right.data(), xValsDBW.size());
                ImPlot::PlotLine("Rear Left", xValsDBW.data(), v_rear_left.data(), xValsDBW.size());
                ImPlot::PlotLine("Rear Right", xValsDBW.data(), v_rear_right.data(), xValsDBW.size());

                ImPlot::EndPlot();
            }
            ImGui::End();
        }

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * clear_color.w * 255.0f), (int)(clear_color.y * clear_color.w * 255.0f), (int)(clear_color.z * clear_color.w * 255.0f), (int)(clear_color.w * 255.0f));
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (result == D3DERR_DEVICELOST)
            g_DeviceLost = true;
    }

    // Cleanup
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    ImPlot::DestroyContext();  
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    // Create the D3DDevice
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}