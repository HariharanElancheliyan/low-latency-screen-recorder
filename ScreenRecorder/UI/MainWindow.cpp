#include <set>
#include <locale>
#include <codecvt>


#include <wx/app.h>
#include <wx/combobox.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/colour.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/msgdlg.h>
#include <wx/icon.h>
#include <wx/timer.h>
#include <wx/dirdlg.h>
#include <wx/slider.h>

#include <shellscalingapi.h>

#include "ScreenRecorder.h"


#define BITRATE 1000000

using namespace std;

struct MonitorInfo 
{
    HMONITOR handle;
    std::wstring device_name;
    RECT monitor_rect;
    RECT work_rect;
    DWORD flags;
	int monitor_index;
    int refresh_rate;

    MonitorInfo()
    {
		handle = nullptr;
		device_name = L"";
		monitor_rect = { 0, 0, 0, 0 };
		work_rect = { 0, 0, 0, 0 };
		flags = 0;
		monitor_index = 0;
		refresh_rate = 60;
    }

	MonitorInfo(const MonitorInfo& other)
		: handle(other.handle), device_name(other.device_name), monitor_rect(other.monitor_rect),
		work_rect(other.work_rect), flags(other.flags), monitor_index(other.monitor_index), refresh_rate(other.refresh_rate)
    {
	}

	MonitorInfo& operator=(const MonitorInfo& other)
	{
		if (this != &other)
		{
			handle = other.handle;
			device_name = other.device_name;
			monitor_rect = other.monitor_rect;
			work_rect = other.work_rect;
			flags = other.flags;
			monitor_index = other.monitor_index;
			refresh_rate = other.refresh_rate;
		}
		return *this;
	}
};

struct ApplicationInfo
{
    HWND handle;
    std::wstring window_title;

	ApplicationInfo()
	{
		handle = nullptr;
		window_title = L"";
	}

	ApplicationInfo(const ApplicationInfo& other)
		: handle(other.handle), window_title(other.window_title)
	{
	}

	ApplicationInfo& operator=(const ApplicationInfo& other)
	{
		if (this != &other)
		{
			handle = other.handle;
			window_title = other.window_title;
		}
		return *this;
	}
};


std::string WStringToString(const std::wstring& wide_string) 
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wide_string);
}

wxString WStringToWxString(const std::wstring& wide_string) 
{
    return wxString(wide_string);
}

BOOL CALLBACK MonitorEnumProc(HMONITOR h_monitor, HDC hdc_monitor, LPRECT lprc_monitor, LPARAM dw_data) 
{
    MONITORINFOEX monitor_info_ex = {};
    monitor_info_ex.cbSize = sizeof(MONITORINFOEX);

    if (GetMonitorInfo(h_monitor, &monitor_info_ex)) 
    {
        MonitorInfo info;
        info.handle = h_monitor;
        info.device_name = monitor_info_ex.szDevice;
        info.monitor_rect = monitor_info_ex.rcMonitor;
        info.work_rect = monitor_info_ex.rcWork;
        info.flags = monitor_info_ex.dwFlags;

        DEVMODE dev_mode = {};
        dev_mode.dmSize = sizeof(DEVMODE);
        if (EnumDisplaySettings(monitor_info_ex.szDevice, ENUM_CURRENT_SETTINGS, &dev_mode))
        {
            info.refresh_rate = dev_mode.dmDisplayFrequency;
            RECT actual_rect;
            actual_rect.left = 0;
            actual_rect.top = 0;
            actual_rect.right = dev_mode.dmPelsWidth;
            actual_rect.bottom = dev_mode.dmPelsHeight;
            

            info.monitor_rect = actual_rect;
        }
        else
        {
            info.refresh_rate = 60; // Default to 60 if unable to retrieve
        }

        auto* monitor_list = reinterpret_cast<std::vector<MonitorInfo>*>(dw_data);
		info.monitor_index = static_cast<int>(monitor_list->size()) + 1;
        monitor_list->push_back(info);
    }

    return TRUE;  
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    if (IsWindowVisible(hwnd))
    {
        wchar_t title[256];
        GetWindowText(hwnd, title, sizeof(title) / sizeof(wchar_t));

        if (wcslen(title) > 0)
        {
            ApplicationInfo app_info;
            app_info.handle = hwnd;
            app_info.window_title = title;

            auto* app_list = reinterpret_cast<std::vector<ApplicationInfo>*>(lParam);
            app_list->push_back(app_info);
        }
    }
    return TRUE;
}


namespace SimpleScreenRecorder 
{

    // Utility class for managing monitors and applications
    class DataManager 
    {
    public:

        static void EnumerateMonitors(std::vector<MonitorInfo>& monitors) 
        {
            monitors.clear();
            EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));
        }

        static void EnumerateApplications(std::vector<ApplicationInfo>& applications) 
        {
            applications.clear();
            std::set<std::wstring> seen_titles;
            std::vector<ApplicationInfo> temp_applications;
            
            EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&temp_applications));
            
            // Filter out duplicates based on window title
            for (const auto& app : temp_applications)
            {
                if (seen_titles.find(app.window_title) == seen_titles.end())
                {
                    seen_titles.insert(app.window_title);
                    applications.push_back(app);
                }
            }
        }

        static std::vector<wxString> ConvertToWxStringList(const std::vector<MonitorInfo>& monitors) 
        {
            std::vector<wxString> monitor_strings;

            int index = 1;
            for (const auto& monitor : monitors)
            {
                std::wstring monitor_name = L"Monitor - " + std::to_wstring(index);
                std::wstring width_height = L" [" + std::to_wstring(monitor.monitor_rect.right - monitor.monitor_rect.left) 
                                            + L" x " + std::to_wstring(monitor.monitor_rect.bottom - monitor.monitor_rect.top) + L"]";

                monitor_name.append(width_height);

                if (monitor.flags & MONITORINFOF_PRIMARY)
                {
                    monitor_name.append(L" (Primary)");
                }

                monitor_strings.emplace_back(WStringToWxString(monitor_name));

                index++;
            }

			monitor_strings.insert(monitor_strings.begin(), L"Select a monitor");
            return monitor_strings;
        }

        static std::vector<wxString> ConvertToWxStringList(const std::vector<ApplicationInfo>& applications)
        {
            std::vector<wxString> application_strings;

            for (const auto& apps : applications)
            {
                application_strings.emplace_back(WStringToWxString(apps.window_title));
            }

			application_strings.insert(application_strings.begin(), L"Select an application");

            return application_strings;
        }
    };

    class Frame : public wxFrame 
    {
    public:
        Frame() : wxFrame(nullptr, wxID_ANY, "LowLatencyScreenRecorder", wxDefaultPosition, wxSize(600, 300), wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER & ~wxMAXIMIZE_BOX)
        {
            HideWindowFromCapturing();
            InitializeUI();
            LoadData();
            SetAppIcon();
        }

		~Frame()
		{
			screen_recorder.StopCapture();
			StopTimer();
		}

    private:

        void HideWindowFromCapturing()
        {
            HWND hwnd = reinterpret_cast<HWND>(GetHandle());
            SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
        }

		void SetAppIcon()
		{
            wxIcon icon;
            icon.LoadFile("IDI_ICON1", wxBITMAP_TYPE_ICO_RESOURCE);

            if (icon.IsOk())
			{
				SetIcon(icon);
			}
		}

        void StartTimer()
        {
            elapsed_seconds = 0;
            timer_label->SetLabel("Duration : 00:00:00");
            timer.Start(1000);
        }

		void StopTimer()
		{
			timer.Stop();
			elapsed_seconds = 0;
            timer_label->SetLabel("Duration : 00:00:00");
		}

        void OnTimer(wxTimerEvent&) 
        {
            elapsed_seconds++;

            int hours = elapsed_seconds / 3600;
            int minutes = (elapsed_seconds % 3600) / 60;
            int seconds = elapsed_seconds % 60;

            wxString timeStr = wxString::Format("Duration: %02d:%02d:%02d", hours, minutes, seconds);
            timer_label->SetLabel(timeStr);
        }

        wxString SelectFolder() 
        {
            wxDirDialog dirDialog(this, "Select a folder", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

            if (dirDialog.ShowModal() == wxID_OK) 
            {
                return dirDialog.GetPath();
            }

            return "";
        }

        void InitializeUI() 
        {
            SetClientSize(600, 300);
            SetAppIcon();

            auto monitor_or_app_label = new wxStaticText(panel.get(), wxID_ANY, "Select Monitor / Application",
                wxPoint(10, 10), wxDefaultSize, wxALIGN_LEFT);

            auto capture_item_list_label = new wxStaticText(panel.get(), wxID_ANY, "Select an item from the list:",
                wxPoint(10, 80), wxDefaultSize, wxALIGN_LEFT);

            auto select_folder_label = new wxStaticText(panel.get(), wxID_ANY, "Select Output Folder",
                wxPoint(360, 10), wxDefaultSize, wxALIGN_RIGHT);

            auto encoder_label = new wxStaticText(panel.get(), wxID_ANY, "Select Encoder",
                wxPoint(360, 80), wxDefaultSize, wxALIGN_RIGHT);

            auto quality_label = new wxStaticText(panel.get(), wxID_ANY, "Quality",
                wxPoint(360, 150), wxDefaultSize, wxALIGN_LEFT);

            wxFont bold_font(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
            wxFont timer_font(13, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

            monitor_or_app_label->SetFont(bold_font);
            capture_item_list_label->SetFont(bold_font);
			select_folder_label->SetFont(bold_font);
            encoder_label->SetFont(bold_font);
            quality_label->SetFont(bold_font);

			select_folder_button = std::make_shared<wxButton>(panel.get(), wxID_ANY, "Select Folder", wxPoint(360, 40), wxSize(180, 25), wxALIGN_RIGHT);
            select_folder_button->Bind(wxEVT_BUTTON, &Frame::OnSelectFolderButtonClicked, this);

            encoder_cb = std::make_shared<wxComboBox>(panel.get(), wxID_ANY, wxEmptyString, wxPoint(360, 110), wxSize(180, 25), 0, nullptr, wxCB_READONLY);
            encoder_cb->Append("VP8");
            encoder_cb->Append("VP9");
			encoder_cb->Append("H264");
            encoder_cb->SetSelection(0); // Default to VP8
            encoder_cb->Bind(wxEVT_COMBOBOX, &Frame::OnEncoderSelectionChanged, this);

            quality_value_label = std::make_shared<wxStaticText>(panel.get(), wxID_ANY, "5", wxPoint(450, 150), wxSize(30, 25), wxALIGN_CENTER);
            quality_value_label->SetFont(bold_font);
            quality_value_label->SetForegroundColour(wxColour(0, 122, 204));

            quality_slider = std::make_shared<wxSlider>(panel.get(), wxID_ANY, 5, 1, 10, wxPoint(395, 180), wxSize(130, 30), wxSL_HORIZONTAL);
            quality_slider->Bind(wxEVT_SLIDER, &Frame::OnQualitySliderChanged, this);

            auto quality_low_label = new wxStaticText(panel.get(), wxID_ANY, "Low",
                wxPoint(360, 185), wxDefaultSize, wxALIGN_LEFT);

            auto quality_high_label = new wxStaticText(panel.get(), wxID_ANY, "High",
                wxPoint(530, 185), wxDefaultSize, wxALIGN_RIGHT);

            monitor_or_app_cb->Append("Monitor");
            monitor_or_app_cb->Append("Application");
            monitor_or_app_cb->SetSelection(0);
            monitor_or_app_cb->Bind(wxEVT_COMBOBOX, &Frame::OnMonitorOrAppSelectionChanged, this);

            capture_item_list->Hide();

            start_stop_button = std::make_shared<wxButton>(panel.get(), wxID_ANY, "Start Recording", wxPoint(10, 230), wxSize(200, 50), wxALIGN_LEFT);
            start_stop_button->SetBackgroundColour(wxColour(0, 122, 204));
            start_stop_button->SetForegroundColour(*wxWHITE);
			start_stop_button->SetFont(bold_font);

            start_stop_button->Bind(wxEVT_BUTTON, &Frame::OnStartStopButtonClicked, this);
            start_stop_button->Bind(wxEVT_ENTER_WINDOW, &Frame::OnButtonHover, this);
            start_stop_button->Bind(wxEVT_LEAVE_WINDOW, &Frame::OnButtonLeave, this);

            timer_label = std::make_shared<wxStaticText>(panel.get(), wxID_ANY, "Duration : 00:00:00", wxPoint(250, 240), wxSize(200, 50), wxALIGN_CENTER);
			timer_label->SetFont(timer_font);
			timer_label->SetForegroundColour(wxColour(0, 122, 204));

            timer.Bind(wxEVT_TIMER, &Frame::OnTimer, this);
        }

        void LoadData() 
        {
            DataManager::EnumerateMonitors(monitors);
            DataManager::EnumerateApplications(applications);
            UpdateCaptureItemList();
        }

        void UpdateCaptureItemList() 
        {
            capture_item_list->Clear();

            if (monitor_or_app_cb->GetSelection() == 0) 
            {
                auto monitor_strings = DataManager::ConvertToWxStringList(monitors);
                capture_item_list->Append(monitor_strings);
            }
            else 
            {
                auto application_strings = DataManager::ConvertToWxStringList(applications);
                capture_item_list->Append(application_strings);
            }

			capture_item_list->SetSelection(0);
            capture_item_list->Show();
            capture_item_list->Bind(wxEVT_COMBOBOX, &Frame::OnCaptureItemSelected, this);
        }

        void OnMonitorOrAppSelectionChanged(wxCommandEvent& e) 
        {
            UpdateCaptureItemList();
        }

        void OnEncoderSelectionChanged(wxCommandEvent& e)
        {
            int selected_index = encoder_cb->GetSelection();
            
            switch (selected_index)
            {
                case 0:
                    encoder_type = EncoderType::VPX_VP8;
                    break;
                case 1:
                    encoder_type = EncoderType::VPX_VP9;
                    break;
                case 2:
                    encoder_type = EncoderType::MFT_H264;
                    break;
                case 3:
                    encoder_type = EncoderType::VPX_VP9_444;
                    break;
				default:
					encoder_type = EncoderType::VPX_VP8;
					break;
            }
        }

        void OnQualitySliderChanged(wxCommandEvent& e)
        {
            int quality = quality_slider->GetValue();
            quality_value_label->SetLabel(wxString::Format("%d", quality));
        }

		void OnCaptureItemSelected(wxCommandEvent& e)
		{
			int selected_index = capture_item_list->GetSelection();
            capture_item_list->SetSelection(selected_index);
			selected_monitor = nullptr;
			selected_app = nullptr;

            if (selected_index == wxNOT_FOUND || selected_index == 0)
            {
                return;
            }

            if (monitor_or_app_cb->GetSelection() == 0)
            {
                // Adjust index for monitors (For "Select a monitor" at index 0)
                selected_monitor = monitors[selected_index - 1].handle;
            }
            else
            {
                // Adjust index for applications (For "Select an application" at index 0)
                selected_app = applications[selected_index - 1].handle;
				std::wstring test = applications[selected_index - 1].window_title;
                is_monitor_capture = false;
            }
		}

        void OnStartStopButtonClicked(wxCommandEvent& event) 
        {
            if (is_recording) 
            {
                start_stop_button->SetLabel("Start Recording");
                start_stop_button->SetBackgroundColour(wxColour(0, 122, 204)); 

				screen_recorder.StopCapture();
                StopTimer();

				wxString message = "Recording stopped. File stored in \n";
				message.append(screen_recorder.GetOutputPath());

				wxMessageBox(message, "Info", wxOK | wxICON_INFORMATION, this);
				capture_item_list->Enable();
				monitor_or_app_cb->Enable();

                if (selected_monitor != nullptr)
                {
					selected_monitor = nullptr;
				}

                capture_item_list->SetSelection(0);
                is_recording = !is_recording;
			}
			else if (monitor_or_app_cb->GetSelection() == 0 && selected_monitor == nullptr)
			{
				wxMessageBox("Please select a monitor", "Error", wxOK | wxICON_ERROR);
			}
			else if (monitor_or_app_cb->GetSelection() == 1 && selected_app == nullptr)
			{
				wxMessageBox("Please select an application", "Error", wxOK | wxICON_ERROR);
            }
            else 
            {
                DWORD flag = 0;
				HMONITOR monitor_to_capture = is_monitor_capture ? selected_monitor : MonitorFromPoint(POINT(0,0), flag);
				MonitorInfo monitor_info = GetMonitorDetailsFromHMONITOR(monitor_to_capture);

				int monitor_number = monitor_info.monitor_index;
				int width = monitor_info.monitor_rect.right - monitor_info.monitor_rect.left;
				int height = monitor_info.monitor_rect.bottom - monitor_info.monitor_rect.top;
				int quality = (quality_slider->GetValue() <= 0) ? 5 : quality_slider->GetValue();
				int bitrate = BITRATE * quality;
                int fps = monitor_info.refresh_rate;

				screen_recorder.CreateOutputFolder(output_folder_path);
                bool result = screen_recorder.Initialize(encoder_type, monitor_number, width, height, fps, bitrate);

                if (!result)
                {
                    wxMessageBox("Error occured in screen recorder", "Error", wxOK | wxICON_ERROR);
					is_recording = !is_recording;
					return;
                }

				if (is_monitor_capture)
				{
					screen_recorder.StartMonitorCapture(selected_monitor);
				}
				else
				{
                    SetForegroundWindow(selected_app);
					screen_recorder.StartWindowCapture(selected_app);
				}

                start_stop_button->SetLabel("Stop Recording");
                start_stop_button->SetBackgroundColour(wxColour(204, 0, 0));

                StartTimer();
				//wxMessageBox("Recording started", "Info", wxOK | wxICON_INFORMATION);

				capture_item_list->Disable();
				monitor_or_app_cb->Disable();

                is_recording = !is_recording;
            }

        }

        void OnSelectFolderButtonClicked(wxCommandEvent& event) 
        {
            wxString selected_folder = SelectFolder();
            if (!selected_folder.IsEmpty())
            {
                output_folder_path = selected_folder.wc_str();
                
                select_folder_button->SetLabel(wxString::FromUTF8("Selected"));
                select_folder_button->SetBackgroundColour(wxColour(0, 128, 0)); // Green color
                select_folder_button->SetForegroundColour(*wxWHITE);
            }
        }

        void OnButtonHover(wxMouseEvent& event) 
        {
            start_stop_button->SetForegroundColour(*wxBLACK);
            event.Skip();
        }

        void OnButtonLeave(wxMouseEvent& event) 
        {
            event.Skip();
            start_stop_button->SetForegroundColour(*wxWHITE);
        }

        MonitorInfo GetMonitorDetailsFromHMONITOR(HMONITOR target_monitor)
        {
            MonitorInfo monitor_info;

            if (!monitors.empty() && target_monitor)
            {
                for (auto& monitor : monitors)
                {
                    if (monitor.handle == target_monitor)
                    {
                        monitor_info = monitor;
                        break;
                    }

                    monitor_info = monitor;
                }
            }

            return monitor_info;
        }

        private:
		
        std::shared_ptr<wxPanel> panel = std::make_shared<wxPanel>(this, wxID_ANY);
        std::shared_ptr<wxComboBox> monitor_or_app_cb = std::make_shared<wxComboBox>(panel.get(), wxID_ANY, wxEmptyString, wxPoint(10, 40), wxSize(300, 170));
        std::shared_ptr<wxComboBox> capture_item_list = std::make_shared<wxComboBox>(panel.get(), wxID_ANY, wxEmptyString, wxPoint(10, 120), wxSize(300, 170));
        std::shared_ptr<wxComboBox> encoder_cb = nullptr;
        std::shared_ptr<wxButton> start_stop_button = nullptr;
        std::shared_ptr<wxButton> select_folder_button = nullptr;
        std::shared_ptr<wxSlider> quality_slider = nullptr;
        std::shared_ptr<wxStaticText> quality_value_label = nullptr;
        bool is_recording = false;
		bool is_monitor_capture = true;

        HMONITOR selected_monitor;
        HWND selected_app;

        std::vector<MonitorInfo> monitors;
        std::vector<ApplicationInfo> applications;

		std::wstring output_folder_path;
		EncoderType encoder_type = EncoderType::VPX_VP8;
        wxTimer timer;
        int elapsed_seconds;
        std::shared_ptr<wxStaticText> timer_label = nullptr;
        ScreenRecorder screen_recorder;
    };

    class Application : public wxApp 
    {
        void SetDpiAwareness()
        {
            if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE))
            {
                SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
            }
        }

        bool OnInit() override 
        {
            SetDpiAwareness();
            return (new Frame)->Show();
        }
    };
}

wxIMPLEMENT_APP(SimpleScreenRecorder::Application);

