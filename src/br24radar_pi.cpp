/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Navico BR24 Radar Plugin
 * Author:   David Register
 *           Dave Cowell
 *           Kees Verruijt
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register              bdbcat@yahoo.com *
 *   Copyright (C) 2012-2013 by Dave Cowell                                *
 *   Copyright (C) 2012-2013 by Kees Verruijt         canboat@verruijt.net *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */

// Kees says:
// The original implementation used a 'gathering' scan buffer and a 'display'
// buffer, but the only thing done between the two is a full copy without any
// locking, so glitches could still occur.
// For now let's disable the code but leave it around.
#undef BR24_DOUBLE_BUFFERING

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
#include "wx/wx.h"
#endif                          //precompiled headers

#include <wx/socket.h>
#include "wx/apptrait.h"
#include <wx/glcanvas.h>
#include "wx/sckaddr.h"
#include "wx/datetime.h"
#include <wx/fileconf.h>
#include <fstream>

using namespace std;

#ifdef __WXGTK__
#include <netinet/in.h>
#include <sys/ioctl.h>
#endif

#ifdef __WXMSW__
#include "GL/glu.h"
#else
typedef char byte;
#endif

#include "br24radar_pi.h"
#include "ocpndc.h"

/* A marker that uniquely identifies BR24 generation scanners, as opposed to 4G(eneration) */
static unsigned char BR24MARK[] = { 0x00, 0x44, 0x0d, 0x0e };

enum {
    // process ID's
    ID_OK,
    ID_OVERLAYDISPLAYOPTION,
    ID_DISPLAYTYPE,
    ID_INTERVALSLIDER,
    ID_HEADINGSLIDER,
};

union packet_buf {
    radar_frame_pkt br_frame_pkt;
    unsigned char   buf[20000];     // 32 lines of 512 = 16,384, DO NOT ALTER from 20000!
} br_packet;

radar_line br_line;

unsigned char     br_scan_buf[360][512];        //scan buffer

#ifdef BR24_DOUBLE_BUFFERED
unsigned char     br_static_buf[360][512];      //static buffer for GL3 rendering
#else
# define br_static_buf br_scan_buf
#endif

long              br_scan_distribution[255];    // intensity distribution

double            br_range_meters = 0;      // current range for radar
double            br_scan_range_meters;
long              br_previous_range = 0;

int               max_angle = 0;
int               min_angle = 361;


wxDateTime        br_static_timestamp;


//Ranges are metric for BR24 - the hex codes are little endian = 10 X range value

range_settings br24_range_settings[] = {
    {50, {(byte)0x03, (byte)0xc1, (byte)0xf4, (byte)0x01, 0, 0}},
    {75, {(byte)0x03, (byte)0xc1, (byte)0xee, (byte)0x02, 0, 0}},
    {100, {(byte)0x03, (byte)0xc1, (byte)0xee, (byte)0x03, 0, 0}},
    {250, {(byte)0x03, (byte)0xc1, (byte)0xc4, (byte)0x09, 0, 0}},
    {500, {(byte)0x03, (byte)0xc1, (byte)0x88, (byte)0x13, 0, 0}},
    {750, {(byte)0x03, (byte)0xc1, (byte)0x4c, (byte)0x1d, 0, 0}},
    {1000, {(byte)0x03, (byte)0xc1, (byte)0x10, (byte)0x27, 0, 0}},
    {1500, {(byte)0x03, (byte)0xc1, (byte)0x98, (byte)0x3a, 0, 0}},
    {2000, {(byte)0x03, (byte)0xc1, (byte)0x20, (byte)0x4e, 0, 0}},
    {3000, {(byte)0x03, (byte)0xc1, (byte)0x30, (byte)0x75, 0, 0}},
    {4000, {(byte)0x03, (byte)0xc1, (byte)0x40, (byte)0x9c, 0, 0}},
    {6000, {(byte)0x03, (byte)0xc1, (byte)0x60, (byte)0xea, 0, 0}},
    {8000, {(byte)0x03, (byte)0xc1, (byte)0x80, (byte)0x38, 1, 0}},
    {12000, {(byte)0x03, (byte)0xc1, (byte)0xc0, (byte)0xd4, 1, 0}},
    {16000, {(byte)0x03, (byte)0xc1, (byte)0x00, (byte)0x71, 2, 0}},
    {24000, {(byte)0x03, (byte)0xc1, (byte)0x80, (byte)0xa9, 3, 0}}
};

const int n_ranges = sizeof(br24_range_settings)/sizeof(br24_range_settings[0]);

bool br_bpos_set;
double br_ownship_lat;
double br_ownship_lon;
double br_hdm;
double br_hdt;

int   br_radar_state;
int   br_scanner_state;
long  br_display_interval(0);
int   br_sweep_count;
wxDateTime  br_dt_last_tick;
wxDateTime  br_dt_stayalive;
int   br_scan_packets_per_tick;

bool  br_bshown_dc_message;

// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr)
{
    return new br24radar_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}

int nseq;

//---------------------------------------------------------------------------------------------------------
//
//    BR24Radar PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

#include "icons.h"
//#include "default_pi.xpm"
#include "icons.cpp"

//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

br24radar_pi::br24radar_pi(void *ppimgr)
    : opencpn_plugin_18(ppimgr)
{
    // Create the PlugIn icons
    initialize_images();
    m_pdeficon = new wxBitmap(*_img_radar_blank);

}

int br24radar_pi::Init(void)
{
    m_pControlDialog = NULL;
    m_pManualDialog = NULL;

    br_radar_state = RADAR_OFF;
    br_scanner_state = RADAR_OFF;                 // Radar scanner is off
    br_sweep_count = 0;
    br_dt_last_tick = wxDateTime::Now();
    br_dt_stayalive = wxDateTime::Now();

    m_dt_last_render = wxDateTime::Now();
    m_ptemp_icon = NULL;
    m_sent_bm_id_normal = -1;
    m_sent_bm_id_rollover =  -1;

    m_hdt_source = 0;
    m_hdt_prev_source = 0;

//******************************************************************************************/
//******************************************************************************************/
    settings.display_mode = 0;
    settings.master_mode = false;                 // we're not the master controller at startup
    settings.auto_range_mode = true;                    // starts with auto range change
    settings.overlay_transparency = DEFAULT_OVERLAY_TRANSPARENCY;

//      Set default parameters for controls displays
    m_BR24Controls_dialog_x = 0;
    m_BR24Controls_dialog_y = 0;
    m_BR24Controls_dialog_sx = 200;
    m_BR24Controls_dialog_sy = 200;

    m_BR24Manual_dialog_x = 0;
    m_BR24Manual_dialog_y = 0;
    m_BR24Manual_dialog_sx = 200;
    m_BR24Manual_dialog_sy = 200;

    ::wxDisplaySize(&m_display_width, &m_display_height);

//****************************************************************************************
    //    Get a pointer to the opencpn configuration object
    m_pconfig = GetOCPNConfigObject();

    //    And load the configuration items
    LoadConfig();

    // Get a pointer to the opencpn display canvas, to use as a parent for the UI dialog
    m_parent_window = GetOCPNCanvasWindow();

    //    This PlugIn needs a toolbar icon
    {
        m_tool_id  = InsertPlugInTool(wxT(""), _img_radar_red, _img_radar_red, wxITEM_NORMAL,
                                      wxT("BR24Radar"), wxT(""), NULL,
                                      BR24RADAR_TOOL_POSITION, 0, this);

        CacheSetToolbarToolBitmaps(BM_ID_RED, BM_ID_BLANK);
    }

    wxIPV4address dummy;                // Do this to initialize the wxSocketBase tables

    //    Create the control socket for the Radar data receiver

    wxIPV4address addr101;
    addr101.AnyAddress();
    addr101.Service(wxT("6678"));
    m_out_sock101 = new wxDatagramSocket(addr101, wxSOCKET_REUSEADDR | wxSOCKET_NOWAIT);

    //    Create the THREAD for Multicast radar data reception
    m_quit = false;
    m_receiveThread = new MulticastRXThread(this, &m_quit, wxT("236.6.7.8"), wxT("6678"));
    m_receiveThread->Run();


    m_pmenu = new wxMenu();            // this is a dummy menu
    // required by Windows as parent to item created
    wxMenuItem *pmi = new wxMenuItem(m_pmenu, -1, _("Radar Control"));
    int miid = AddCanvasContextMenuItem(pmi, this);
    SetCanvasContextMenuItemViz(miid, true);

    return (WANTS_DYNAMIC_OPENGL_OVERLAY_CALLBACK |
            WANTS_OPENGL_OVERLAY_CALLBACK |
            WANTS_OVERLAY_CALLBACK     |
            WANTS_CURSOR_LATLON        |
            WANTS_TOOLBAR_CALLBACK     |
            INSTALLS_TOOLBAR_TOOL      |
            INSTALLS_CONTEXTMENU_ITEMS |
            WANTS_CONFIG               |
            WANTS_NMEA_EVENTS          |
            WANTS_PREFERENCES
           );
}

bool br24radar_pi::DeInit(void)
{
    wxLogMessage(wxT("BR24 radar plugin is stopping."));
    SaveConfig();
    wxLogMessage(wxT("BR24 radar config saved."));

    if (m_receiveThread) {
        wxLogMessage(wxT("BR24 radar thread waiting ...."));
        m_quit = true;
        m_receiveThread->Wait();
        wxLogMessage(wxT("BR24 radar thread is stopped."));
        delete m_receiveThread;
        wxLogMessage(wxT("BR24 radar thread is deleted."));
    }

    return true;
}

int br24radar_pi::GetAPIVersionMajor()
{
    return MY_API_VERSION_MAJOR;
}

int br24radar_pi::GetAPIVersionMinor()
{
    return MY_API_VERSION_MINOR;
}

int br24radar_pi::GetPlugInVersionMajor()
{
    return PLUGIN_VERSION_MAJOR;
}

int br24radar_pi::GetPlugInVersionMinor()
{
    return PLUGIN_VERSION_MINOR;
}

wxBitmap *br24radar_pi::GetPlugInBitmap()
{
    return m_pdeficon;
}

wxString br24radar_pi::GetCommonName()
{
    return wxT("BR24Radar");
}


wxString br24radar_pi::GetShortDescription()
{
    return _("Navico Radar PlugIn for OpenCPN");
}


wxString br24radar_pi::GetLongDescription()
{
    return _("Navico Broadband BR24/3G/4G Radar PlugIn for OpenCPN\n");
}

void br24radar_pi::SetDefaults(void)
{
    // If the config somehow says NOT to show the icon, override it so the user gets good feedback
    if (!m_bShowIcon) {
        m_bShowIcon = true;

        m_tool_id  = InsertPlugInTool(wxT(""), _img_radar_red, _img_radar_red, wxITEM_CHECK,
                                      wxT("BR24Radar"), wxT(""), NULL,
                                      BR24RADAR_TOOL_POSITION, 0, this);
    }

}

void br24radar_pi::ShowPreferencesDialog(wxWindow* parent)
{
    m_pOptionsDialog = new BR24DisplayOptionsDialog;
    m_pOptionsDialog->Create(m_parent_window, this);
    m_pOptionsDialog->Show();
}

void logBinaryData(const wxString& what, const char * data, int size)
{
    wxString explain;
    int i;

    explain.Alloc(size * 3 + 50);
    explain += wxT("BR24radar_pi: ");
    explain += what;
    explain += wxString::Format(wxT(" %d bytes: "), size);
    for (i = 0; i < size; i++)
    {
      explain += wxString::Format(wxT(" %02X"), (unsigned char) data[i]);
    }
    wxLogMessage(explain);
}

//*********************************************************************************
// Display Preferences Dialog
//*********************************************************************************
IMPLEMENT_CLASS(BR24DisplayOptionsDialog, wxDialog)

BEGIN_EVENT_TABLE(BR24DisplayOptionsDialog, wxDialog)

    EVT_CLOSE(BR24DisplayOptionsDialog::OnClose)
    EVT_BUTTON(ID_OK, BR24DisplayOptionsDialog::OnIdOKClick)
    EVT_RADIOBUTTON(ID_DISPLAYTYPE, BR24DisplayOptionsDialog::OnDisplayModeClick)
    EVT_RADIOBUTTON(ID_OVERLAYDISPLAYOPTION, BR24DisplayOptionsDialog::OnDisplayOptionClick)

END_EVENT_TABLE()

BR24DisplayOptionsDialog::BR24DisplayOptionsDialog()
{
    Init();
}

BR24DisplayOptionsDialog::~BR24DisplayOptionsDialog()
{
}

void BR24DisplayOptionsDialog::Init()
{
}

bool BR24DisplayOptionsDialog::Create(wxWindow *parent, br24radar_pi *ppi)
{
    pParent = parent;
    pPlugIn = ppi;
//      wxString msg;
//      msg.Printf(wxT("OptionsDialog: Creating Dialog"));
//      wxLogMessage(msg);

//  wxDialog *PreferencesDialog = new wxDialog( parent, wxID_ANY, _("BR24 Target Display Preferences"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE );
    if (!wxDialog::Create(parent, wxID_ANY, _("BR24 Target Display Preferences"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)) {
        return false;
    }

    int border_size = 4;
    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(topSizer);

    wxBoxSizer* DisplayOptionsBox = new wxBoxSizer(wxVERTICAL);
    topSizer->Add(DisplayOptionsBox, 0, wxALIGN_CENTER_HORIZONTAL | wxALL | wxEXPAND, 2);

    //  BR24 toolbox icon checkbox
    wxStaticBox* DisplayOptionsCheckBox = new wxStaticBox(this, wxID_ANY, _(""));
    wxStaticBoxSizer* DisplayOptionsCheckBoxSizer = new wxStaticBoxSizer(DisplayOptionsCheckBox, wxVERTICAL);
    DisplayOptionsBox->Add(DisplayOptionsCheckBoxSizer, 0, wxEXPAND | wxALL, border_size);

/// Option settings
    wxString Overlay_Display_Options[] = {
        _("Monocolor-Red"),
        _("Multi-color"),
        _("GL3+"),
    };

    pOverlayDisplayOptions = new wxRadioBox(this, ID_OVERLAYDISPLAYOPTION, _("Overlay Display Options"),
                                            wxDefaultPosition, wxDefaultSize,
                                            3, Overlay_Display_Options, 1, wxRA_SPECIFY_COLS);

    DisplayOptionsCheckBoxSizer->Add(pOverlayDisplayOptions, 0, wxALL | wxEXPAND, 2);

    pOverlayDisplayOptions->Connect(wxEVT_COMMAND_RADIOBOX_SELECTED,
                                    wxCommandEventHandler(BR24DisplayOptionsDialog::OnDisplayOptionClick), NULL, this);

    pOverlayDisplayOptions->SetSelection(pPlugIn->settings.display_option);

//  Display Options
    wxStaticBox* itemStaticBoxSizerDisOptStatic = new wxStaticBox(this, wxID_ANY, _("Display Options"));
    wxStaticBoxSizer* itemStaticBoxSizerDisOpt = new wxStaticBoxSizer(itemStaticBoxSizerDisOptStatic, wxVERTICAL);
    DisplayOptionsCheckBoxSizer->Add(itemStaticBoxSizerDisOpt, 0, wxEXPAND | wxALL, border_size);

    wxString DisplayUpdateStrings[] = {
        _("Radar Chart Overlay"),
        _("Radar Standalone"),
        _("Spectrum"),
    };

    pDisplayMode = new wxRadioBox(this, ID_DISPLAYTYPE, _("Radar Display"),
                                  wxDefaultPosition, wxDefaultSize,
                                  3, DisplayUpdateStrings, 1, wxRA_SPECIFY_COLS);

    itemStaticBoxSizerDisOpt->Add(pDisplayMode, 0, wxALL | wxEXPAND, 2);

    pDisplayMode->Connect(wxEVT_COMMAND_RADIOBOX_SELECTED,
                          wxCommandEventHandler(BR24DisplayOptionsDialog::OnDisplayModeClick), NULL, this);

    pDisplayMode->SetSelection(pPlugIn->settings.display_mode);

//interval slider
    wxStaticBox* interval_sliderbox = new wxStaticBox(this, wxID_ANY, _("Interval 1 sec-6 min"));
    wxStaticBoxSizer* interval_sliderboxsizer = new wxStaticBoxSizer(interval_sliderbox, wxVERTICAL);
    DisplayOptionsBox->Add(interval_sliderboxsizer, 0, wxALL | wxEXPAND, 2);

    pIntervalSlider = new wxSlider(this, ID_INTERVALSLIDER, 50 , 10, 100, wxDefaultPosition,  wxDefaultSize,
                                   wxSL_HORIZONTAL,  wxDefaultValidator, _(""));

    interval_sliderboxsizer->Add(pIntervalSlider, 0, wxALL | wxEXPAND, 2);

    pIntervalSlider->Connect(wxEVT_SCROLL_THUMBRELEASE,
                             wxCommandEventHandler(BR24DisplayOptionsDialog::OnIntervalSlider), NULL, this);

    pIntervalSlider->SetValue(br_display_interval / 36);
    if (pPlugIn->settings.display_mode != 1) {
        interval_sliderbox->Hide();
        pIntervalSlider->Hide();
    }

//  Calibration
    wxStaticBox* itemStaticBoxCalibration = new wxStaticBox(this, wxID_ANY, _("Calibration"));
    wxStaticBoxSizer* itemStaticBoxSizerCalibration = new wxStaticBoxSizer(itemStaticBoxCalibration, wxVERTICAL);
    DisplayOptionsCheckBoxSizer->Add(itemStaticBoxSizerCalibration, 0, wxEXPAND | wxALL, border_size);

    // Range Factor
    wxStaticText *pStatic_Range_Calibration = new wxStaticText(this, wxID_ANY, _("Range factor"));
    itemStaticBoxSizerCalibration->Add(pStatic_Range_Calibration, 1, wxALIGN_LEFT | wxALL, 2);

    pText_Range_Calibration_Value = new wxTextCtrl(this, wxID_ANY);
    itemStaticBoxSizerCalibration->Add(pText_Range_Calibration_Value, 1, wxALIGN_LEFT | wxALL, 5);
    wxString m_temp;
    m_temp.Printf(wxT("%2.5f"), pPlugIn->settings.range_calibration);
    pText_Range_Calibration_Value->SetValue(m_temp);
    pText_Range_Calibration_Value->Connect(wxEVT_COMMAND_TEXT_UPDATED,
                                           wxCommandEventHandler(BR24DisplayOptionsDialog::OnRange_Calibration_Value), NULL, this);

    // Heading correction slider
    wxStaticBox* headingBox = new wxStaticBox(this, wxID_ANY, _("Heading correction"));
    wxStaticBoxSizer* headingBoxSizer = new wxStaticBoxSizer(headingBox, wxVERTICAL);
    itemStaticBoxSizerCalibration->Add(headingBoxSizer, 0, wxALL | wxEXPAND, 2);

    pHeadingSlider = new wxSlider(this, ID_HEADINGSLIDER, 0 , -180, +180, wxDefaultPosition,  wxDefaultSize,
                               wxSL_HORIZONTAL | wxSL_LABELS,  wxDefaultValidator, _("slider"));

    headingBoxSizer->Add(pHeadingSlider, 0, wxALL | wxEXPAND, 2);

    pHeadingSlider->Connect(wxEVT_SCROLL_CHANGED, wxCommandEventHandler(BR24DisplayOptionsDialog::OnHeadingSlider), NULL, this);
    pHeadingSlider->SetValue(pPlugIn->settings.heading_correction);

// Accept/Reject button
    wxStdDialogButtonSizer* DialogButtonSizer = wxDialog::CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    DisplayOptionsBox->Add(DialogButtonSizer, 0, wxALIGN_RIGHT | wxALL, 5);


    DimeWindow(this);

    Fit();
    SetMinSize(GetBestSize());

    return true;
}

void BR24DisplayOptionsDialog::OnDisplayOptionClick(wxCommandEvent &event)
{
    pPlugIn->settings.display_option = pOverlayDisplayOptions->GetSelection();
}

void BR24DisplayOptionsDialog::OnDisplayModeClick(wxCommandEvent &event)
{
    pPlugIn->SetDisplayMode(pDisplayMode->GetSelection());
}

void BR24DisplayOptionsDialog::OnRange_Calibration_Value(wxCommandEvent &event)
{
    wxString temp = pText_Range_Calibration_Value->GetValue();
    temp.ToDouble(&pPlugIn->settings.range_calibration);
}

void BR24DisplayOptionsDialog::OnIntervalSlider(wxCommandEvent &event)
{
    br_display_interval = pIntervalSlider->GetValue() * 36;
}

void BR24DisplayOptionsDialog::OnHeadingSlider(wxCommandEvent &event)
{
    pPlugIn->settings.heading_correction = pHeadingSlider->GetValue();
}

void BR24DisplayOptionsDialog::OnClose(wxCloseEvent& event)
{
    pPlugIn->SaveConfig();
}

void BR24DisplayOptionsDialog::OnIdOKClick(wxCommandEvent& event)
{
    pPlugIn->SaveConfig();
}

//********************************************************************************
// Operation Dialogs - Control, Manual, and Options

void br24radar_pi::OnContextMenuItemCallback(int id)
{
    if (!m_pControlDialog) {
        m_pControlDialog = new BR24ControlsDialog;
        m_pControlDialog->Create(m_parent_window, this);
        m_pControlDialog->Hide();
    }

    if (m_pControlDialog->IsShown()) {
        m_pControlDialog->Hide();
    } else {
        m_pControlDialog->Show();
        m_pControlDialog->SetSize(m_BR24Controls_dialog_x, m_BR24Controls_dialog_y,
                                  m_BR24Controls_dialog_sx, m_BR24Controls_dialog_sy);
    }
    if (!m_pManualDialog) {
        m_pManualDialog = new BR24ManualDialog;
        m_pManualDialog->Create(m_parent_window, this);
        m_pManualDialog->Hide();
    }
}

void br24radar_pi::OnBR24ControlDialogClose()
{
    if (m_pControlDialog) {
        m_pControlDialog->Hide();
    }

    SaveConfig();
}

void br24radar_pi::OnBR24ManualDialogShow()
{
    if (m_pManualDialog) {
        m_pManualDialog->Show();
        m_pManualDialog->SetSize(m_BR24Manual_dialog_x, m_BR24Manual_dialog_y,
                                 m_BR24Manual_dialog_sx, m_BR24Manual_dialog_sy);
    }
}
void br24radar_pi::OnBR24ManualDialogClose()
{
    if (m_pManualDialog) {
        m_pManualDialog->Hide();
    }

    SaveConfig();
}

void br24radar_pi::SetDisplayMode(int mode)
{
    settings.display_mode = mode;
}

void br24radar_pi::SetRangeMode(int mode)
{
    settings.auto_range_mode = (mode == 1);
    if (!settings.auto_range_mode) {
        m_pManualDialog->Show();
    }
}

void br24radar_pi::UpdateDisplayParameters(void)
{
    wxTimeSpan hr(1, 0, 0, 0);
    RequestRefresh(GetOCPNCanvasWindow());
}

void br24radar_pi::SetRange(int index)
{
    settings.range_index = index;
    Select_Range(settings.range_index);
}

//*******************************************************************************
// ToolBar Actions

int br24radar_pi::GetToolbarToolCount(void)
{
    return 1;
}

void br24radar_pi::OnToolbarToolCallback(int id)
{
    if (br_radar_state == RADAR_OFF) {  // turned off
        br_radar_state = RADAR_ON;
        if (br_scanner_state == RADAR_OFF) {
            settings.master_mode = true;
            wxLogMessage(wxT("BR24radar_pi: Master mode on"));
            RadarStayAlive();
            RadarTxOn();
        }
    } else {
        br_radar_state = RADAR_OFF;
        if (settings.master_mode == true) {
            RadarTxOff();
            settings.master_mode = false;
            wxLogMessage(wxT("BR24radar_pi: Master mode off"));
        }
    }

    UpdateState();
}
//*********************************************************************************
// Keeps Radar scanner on line if master and radar on -  run by RenderGLOverlay

void br24radar_pi::DoTick(void)
{
    wxDateTime now = wxDateTime::Now();

    //    If no data appears to be coming in,
    //    switch to Master mode and Turn on Radar

    /*if(m_dt_last_render.IsValid())
       {
          wxDateTime now = wxDateTime::Now();
          wxTimeSpan ts = now.Subtract(m_dt_last_render);
          long delta_t = ts.GetSeconds().ToLong();
          wxString msg;
          msg.Printf(wxT("ROGL: TimeSpan: %g -> %g"), delta_t, br_display_interval);
          wxLogMessage(msg);

          if(delta_t < br_display_interval )
                br_time_render = false;
       }
    */
    if (br_scan_packets_per_tick > 0) { // Something coming from radar unit?
        br_scanner_state = RADAR_ON ;
        if (settings.master_mode && br_dt_stayalive.IsValid()) {
            wxTimeSpan sats = now.Subtract(br_dt_stayalive);
            long delta_t = sats.GetSeconds().ToLong();          // send out stayalive every 5 secs
            if (delta_t > 5) {
                br_dt_stayalive = now;
                RadarStayAlive();
            }
        }
    } else {
        br_scanner_state = RADAR_OFF;
    }
    br_scan_packets_per_tick = 0;
}

void br24radar_pi::UpdateState(void)   // -  run by RenderGLOverlay
{
    /*
        wxString msg;
        msg.Printf(wxT("UpdateState:  Master State: %d  Radar state: %d   Scanner state:  %d "), settings.master_mode, br_radar_state, br_scanner_state);
        wxLogMessage(msg);
    */
    if (br_radar_state == RADAR_ON) {
        if (br_scanner_state  == RADAR_ON) {
            CacheSetToolbarToolBitmaps(BM_ID_GREEN, BM_ID_GREEN);     // ON
        } else {
            CacheSetToolbarToolBitmaps(BM_ID_AMBER, BM_ID_AMBER);     // Amber when scanner != radar
        }
    } else {
        if (br_scanner_state == RADAR_ON) {
            CacheSetToolbarToolBitmaps(BM_ID_AMBER, BM_ID_AMBER);
        } else {
            CacheSetToolbarToolBitmaps(BM_ID_RED, BM_ID_RED);     // OFF
        }
    }
}

//***********************************************************************************************************
// Radar Image Graphic Display Processes
//***********************************************************************************************************

bool br24radar_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
    if (0 == br_bshown_dc_message) {
        br_bshown_dc_message = 1;
        wxString message(_("The Radar Overlay PlugIn requires the Accelerated Graphics (OpenGL) mode to be activated in Options->Display->Chart Display Options"));
        wxMessageDialog dlg(GetOCPNCanvasWindow(),  message, wxT("br24radar message"), wxOK);
        dlg.ShowModal();
    }

    return false;
}

// Called by Plugin Manager on main system process cycle

bool br24radar_pi::RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
    br_bshown_dc_message = 0;             // show message box if RenderOverlay() is called again
    wxString msg;

    // this is expected to be called at least once per second

    DoTick();

    UpdateState();

//   br_time_render = true;

    if ((br_scanner_state == RADAR_ON)  && (br_radar_state == RADAR_ON)) {
        m_dt_last_render = wxDateTime::Now();

        double c_dist, lat, lon;
        double max_distance = 0;
        wxPoint center_screen(vp->pix_width / 2, vp->pix_height / 2);
        wxPoint boat_center;

        if (br_bpos_set) {
            wxPoint pp;
            GetCanvasPixLL(vp, &pp, br_ownship_lat, br_ownship_lon);
            boat_center = pp;
        } else {
            boat_center = center_screen;
        }

        // cc1->m_bFollow = FALSE; or ClearbFollow(); to keep boat centered on radar (not implemented)

        if (settings.auto_range_mode) {  // Calculate the "optimum" radar range setting in meters so Radar just fills Screen
            if (br_bpos_set) {   // br_bpos_set is only set by NMEA input
                GetCanvasLLPix(vp, wxPoint(0, 0), &lat, &lon);       // Dist to Lower Left Corner
                c_dist = radar_distance(lat, lon, br_ownship_lat, br_ownship_lon, 'K');
                max_distance = c_dist;

                GetCanvasLLPix(vp, wxPoint(0, vp->pix_height), &lat, &lon);       // Dist to Upper Left Corner
                c_dist = radar_distance(lat, lon, br_ownship_lat, br_ownship_lon, 'K');
                max_distance = wxMax(max_distance, c_dist);

                GetCanvasLLPix(vp, wxPoint(vp->pix_width, vp->pix_height), &lat, &lon);  // Upper Right Corner
                c_dist = radar_distance(lat, lon, br_ownship_lat, br_ownship_lon, 'K');
                max_distance = wxMax(max_distance, c_dist);

                GetCanvasLLPix(vp, wxPoint(vp->pix_width, 0), &lat, &lon);
                c_dist = radar_distance(lat, lon, br_ownship_lat, br_ownship_lon, 'K');
                /*
                      msg.Printf(wxT("Max_distance to (vert,horiz) %g > %g ,%g, %g, ,%g, %g\n"),
                          c_dist, max_distance,lat, lon, br_ownship_lat, br_ownship_lon);
                      grLogMessage(msg);
                */
                max_distance = wxMax(max_distance, c_dist) * 1000;                      // meters
            } else {
                // If ownship position is not valid, use the ViewPort center
                GetCanvasLLPix(vp, wxPoint(vp->pix_width / 2, vp->pix_height / 2), &br_ownship_lat, &br_ownship_lon);
                GetCanvasLLPix(vp, wxPoint(0, 0), &lat, &lon);
                max_distance = radar_distance(lat, lon, br_ownship_lat, br_ownship_lon, 'K') * 1000;
            }

            /*     msg.Printf(wxT("Max_distance is  %g (%g, %g) to (%g, %g) \n"),
                      max_distance, lat, lon, br_ownship_lat, br_ownship_lon);
                   grLogMessage(msg);
            */
            // now look in the list of available ranges to find the smallest range
            // that is larger than the required range

            long req_range = br24_range_settings[n_ranges - 1].range_meters;      // pre-set to largest scale
            int req_range_index = 0;
            for (int i = 0 ; i < n_ranges ; i++) {
                if (br24_range_settings[i].range_meters >= max_distance) {      // radar works in meters
                    req_range = br24_range_settings[i].range_meters;
                    req_range_index = i;
                    break;
                }
            }

            // do we need to change it?
            if (br_previous_range != req_range) {     // don't care if scanner range is sometimes slightly different
                msg.Printf(wxT("RenderGLOverlay: In Auto Mode - Previous Range: %d  Requested Range %d \n"), br_previous_range, req_range);
                wxLogMessage(msg);
                br_previous_range = req_range;
                Select_Range(req_range_index);      // command radar to optimum range for display
            }
        }

        //    Calculate image scale factor
        double ulat, llat, ulon, llon;
        GetCanvasLLPix(vp, wxPoint(0, vp->pix_height), &ulat, &ulon);  // is pix_height a mapable coordinate?
        GetCanvasLLPix(vp, wxPoint(0, 0), &llat, &llon);
        double dist_y = radar_distance(ulat, ulon, llat, llon, 'K') * 1000; // Distance of height of display - meters

        double v_scale_ppm = 1.0;
        if (dist_y > 0.) {
            v_scale_ppm = vp->pix_height / dist_y ;    // pixel height of screen div by equivalent meters
        }
        v_scale_ppm = v_scale_ppm * settings.range_calibration;

//      msg.Printf(wxT("RenderGLOverlay: v_scale_ppm: %d = (vp->pix_height: %d / Dist_y: %d) pixs/meter Update = %d\n "),
//        v_scale_ppm, vp->pix_height, dist_y, settings.display_mode);
//      wxLogMessage(msg);


        switch (settings.display_mode) {
            case 0:                                         // direct realtime sweep render mode
                RenderRadarOverlay(boat_center, v_scale_ppm, vp);
                break;
            case 1:                                     // plain scope look
                //      RenderRadarStandalone(center_screen, v_scale_ppm, vp);
                // radar_pi::OnToolbarToolCallback
                break;
            case 2:
                RenderSpectrum(center_screen, v_scale_ppm, vp);
                break;
        }
    }
    return true;
}

void br24radar_pi::RenderRadarOverlay(wxPoint radar_center, double v_scale_ppm, PlugIn_ViewPort *vp)
{
//    wxString msg;
//    msg.Printf(wxT("RenderRR)Swept: OpenGL 1.2 method.\n"));
//      wxLogMessage(msg);

    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_HINT_BIT);      //Save state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA);
    glPushMatrix();

    glTranslated(radar_center.x, radar_center.y, 0);

    double heading = fmod(br_hdt + settings.heading_correction + 360.0, 360.0);
    wxLogMessage(wxT("Rotating image for HDT=%f Correction=%d Result=%f"), br_hdt, settings.heading_correction, heading);
    glRotatef(heading - 90.0, 0, 0, 1);        //correction for boat heading -90 for base north

    // scaling...
    double radar_pixels_per_meter = 512. / br_scan_range_meters;
    double scale_factor =  v_scale_ppm / radar_pixels_per_meter;  // screen pix/radar pix

    glScaled(scale_factor, scale_factor, 1.);

    int red, green, blue, alpha;
    for (int angle = 0 ; angle < 360 ; ++angle) {
        for (int radius = 0; radius < 512; ++radius) {
            alpha = br_scan_buf[angle][radius];
            switch (settings.display_option) {
                case 0:
                    alpha = alpha * (MAX_OVERLAY_TRANSPARENCY - settings.overlay_transparency) / MAX_OVERLAY_TRANSPARENCY;
                    glColor4ub(255, 0, 0, (GLubyte)alpha);  // red, blue, green, alpha
                    draw_blob_gl(angle, radius,  1, .75);  // angle, radius, blob radius, arc legnth
                    draw_blob_gl(angle + 0.5, radius,  1, .75);
                    break;
                case 1:
                    red = 0;
                    green = 0;
                    blue = 0;

                    if (alpha > 200) {
                        red = 255;
                    }

                    if (alpha > 100 && alpha < 201) {
                        green = 255;
                    }

                    if (50 < alpha && alpha < 101) {
                        blue = 255;
                    }
                    alpha = alpha * (MAX_OVERLAY_TRANSPARENCY - settings.overlay_transparency) / MAX_OVERLAY_TRANSPARENCY;

                    glColor4ub((GLubyte)red, (GLubyte)green, (GLubyte)blue, (GLubyte)alpha);    // red, blue, green
                    draw_blob_gl(angle, radius,  1, .75);
                    draw_blob_gl(angle + 0.5, radius,  1, .75);
                    break;
                case 2:
                    red = 0;
                    green = 0;
                    blue = 0;

                    if (alpha > 175) {
                        red = 255;
                    }

                    if (alpha > 100 && alpha < 176) {
                        green = 255;
                    }

                    if (50 < alpha && alpha < 101) {
                        blue = 255;
                    }
                    /*
                            for(int angle=0 ; angle < 360 ; angle++)                // scanbuf [360][512] has the even lines filled in already by process buffer
                            {
                                for(int radius=0; radius < 512; radius++)
                                {
                                    glColor4ub(255, 0, 0, br_static_buf[angle][radius]);    // red, blue, green
                                    draw_blob_gl(angle, radius, 1.0, 0.5);
                                    draw_blob_gl(angle+0.5, radius, 1.0, 0.5);

                                    wxColour c1(br_static_buf[angle][radius], 0, 0);    // red, blue, green intensities
                                    wxPen p1(c1);
                                    wxBrush b1(c1);
                                    pdc->SetPen(p1);
                                    pdc->SetBrush(b1);
                                    draw_blob_dc(*pdc, angle, radius, 1.0, 0.5, 4., width/2, height/2);

                                    wxColour c2(br_scan_buf[angle][radius], 0, 0);
                                    wxPen p2(c2);
                                    wxBrush b2(c2);
                                    pdc->SetPen(p2);
                                    pdc->SetBrush(b2);
                                    draw_blob_dc(*pdc, angle+0.5, radius, 1.0, 0.5, 4., width/2, height/2); // half a degree for filling in matrix?
                                }
                            }
                        }
                    */
                    glColor4ub((GLubyte)red, (GLubyte)green, (GLubyte)blue, (GLubyte)alpha);    // red, blue, green
                    draw_blob_gl(angle, radius, 1.0, 1.0);
                    draw_blob_gl(angle + 0.5, radius, 1.0, 1.0);
                    break;
            }
        }
    }

    glPopMatrix();
    glPopAttrib();

}

void br24radar_pi::RenderRadarStandalone(wxPoint radar_center, double v_scale_ppm, PlugIn_ViewPort *vp)
{
    wxDateTime br_last_timestamp;
    if (br_static_timestamp != br_last_timestamp) {
        glPushAttrib(GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_HINT_BIT);      //Save state
        glPushMatrix();
        glEnable(GL_BLEND);
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA);
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glTranslated(radar_center.x, radar_center.y, 0);

        glRotatef(- 90, 0, 0, 1);        // display up for heading

        // scaling...
        double radar_pixels_per_meter = 512. / br_scan_range_meters;
        double scale_factor = v_scale_ppm / radar_pixels_per_meter;   // screen pix/radar pix

        glScaled(scale_factor, scale_factor, 1.);
        int alpha;
        for (int angle = 0 ; angle < 360 ; ++angle) {
            for (int radius = 0; radius < 512; ++radius) {
                alpha = br_static_buf[angle][radius];
                glColor4ub(255, 0, 0, (GLubyte)alpha);  // red, blue, green, alpha
                draw_blob_gl(angle, radius, 1, .75);
                draw_blob_gl(angle + 0.5, radius, 1, .75);
            }
        }

        glPopMatrix();
        glPopAttrib();

        br_last_timestamp = br_static_timestamp;
    }
}
void br24radar_pi::RenderSpectrum(wxPoint radar_center, double v_scale_ppm, PlugIn_ViewPort *vp)
{
    int alpha;
    memset(&br_scan_distribution[0], 0, 255);

    for (int angle = 0 ; angle < 360 ; angle += 2) {
        for (int radius = 1; radius < 510; ++radius) {
            alpha = br_static_buf[angle][radius];
            if (alpha > 0 && alpha < 255) {
                br_scan_distribution[0] += 1;
                br_scan_distribution[alpha] += 1;
            }
        }
    }

    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_HINT_BIT);
    glPushMatrix();

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glTranslated(10, vp->pix_height - 100, 0);
    glScaled(.5, -.5, 1);

    int x, y;
    for (x = 0; x < 254; ++x) {
        y = (int)(br_scan_distribution[x] * 100 / br_scan_distribution[0]);
        glColor4ub(x, 0, 255 - x, 255); // red, green, blue, alpha
        draw_histogram_column(x, y);
    }

    glPopMatrix();
    glPopAttrib();
}

void br24radar_pi::draw_histogram_column(int x, int y)  // x=0->255 => 0->1020, y=0->100 =>0->400
{
    int xa = 5 * x;
    int xb = xa + 5;
    y = 4 * y;

    glBegin(GL_TRIANGLES);        // draw blob in two triangles
    glVertex2i(xa, 0);
    glVertex2i(xb, 0);
    glVertex2i(xa, y);

    glVertex2i(xb, 0);
    glVertex2i(xb, y);
    glVertex2i(xa, y);

    glEnd();

}

void br24radar_pi::draw_blob_gl(double angle, double radius, double blob_r, double arc_length)
{
    double ca = cos(angle * PI / 180.);
    double sa = sin(angle * PI / 180.);

    double xm1 = (radius) * ca;       //    Calculate the blob center x,y
    double ym1 = (radius) * sa;

    double blob_width = abs((arc_length / 360.) * (radius * 2.0 * PI)); // arc length = 1 pi/180 => 254 pi/180 or .0174 => 4.45 pixels

    if (blob_width < blob_r) {
        blob_width = blob_r;
    }

    double xa = xm1 + blob_width; // final coordinates for blob LR corner
    double ya = ym1 - blob_width;

    double xb = xm1 - blob_width; // final coordinates for blob LL corner
    double yb = ym1 - blob_width;

    double xc = xm1 - blob_width; // final coordinates for blob UL corner
    double yc = ym1 + blob_width;

    double xd = xm1 + blob_width; // final coordinates for blob UR corner
    double yd = ym1 + blob_width;

    glBegin(GL_TRIANGLES);        // draw blob in two triangles
    glVertex2d(xa, ya);
    glVertex2d(xb, yb);
    glVertex2d(xc, yc);

    glVertex2f(xa, ya);
    glVertex2f(xc, yc);
    glVertex2f(xd, yd);
    glEnd();
}

//****************************************************************************


//****************************************************************************

bool br24radar_pi::LoadConfig(void)
{

    wxFileConfig *pConf = m_pconfig;

    if (pConf) {
        pConf->SetPath(wxT("/Plugins/BR24Radar"));
        if (pConf->Read(wxT("DisplayOption"), &settings.display_option, 0))
        {
            pConf->Read(wxT("DisplayMode"),  &settings.display_mode, 0);
            pConf->Read(wxT("Transparency"),  &settings.overlay_transparency, DEFAULT_OVERLAY_TRANSPARENCY);
            pConf->Read(wxT("Gain"),  &settings.gain, 50);
            pConf->Read(wxT("RainGain"),  &settings.rain_clutter_gain, 50);
            pConf->Read(wxT("ClutterGain"),  &settings.sea_clutter_gain, 50);
            pConf->Read(wxT("RangeCalibration"),  &settings.range_calibration, 1.0);
            pConf->Read(wxT("HeadingCorrection"),  &settings.heading_correction, 0);
            pConf->Read(wxT("Interface"), &settings.radar_interface, wxT("0.0.0.0"));

            pConf->Read(wxT("ControlsDialogSizeX"), &m_BR24Controls_dialog_sx, 300L);
            pConf->Read(wxT("ControlsDialogSizeY"), &m_BR24Controls_dialog_sy, 540L);
            pConf->Read(wxT("ControlsDialogPosX"), &m_BR24Controls_dialog_x, 20L);
            pConf->Read(wxT("ControlsDialogPosY"), &m_BR24Controls_dialog_y, 170L);

            pConf->Read(wxT("ManualDialogSizeX"), &m_BR24Manual_dialog_sx, 300L);
            pConf->Read(wxT("ManualDialogSizeY"), &m_BR24Manual_dialog_sy, 540L);
            pConf->Read(wxT("ManualDialogPosX"), &m_BR24Manual_dialog_x, 20L);
            pConf->Read(wxT("ManualDialogPosY"), &m_BR24Manual_dialog_y, 170L);

            SaveConfig();
            return true;
        }
    }

    // Read the old location with old paths
    if (pConf) {
        pConf->SetPath(wxT("/Settings"));
        if (pConf->Read(wxT("BR24RadarDisplayOption"), &settings.display_option, 0))
            pConf->DeleteEntry(wxT("BR24RadarDisplayOption"));
        if (pConf->Read(wxT("BR24RadarDisplayMode"),  &settings.display_mode, 0))
            pConf->DeleteEntry(wxT("BR24RadarDisplayMode"));
        if (pConf->Read(wxT("BR24RadarTransparency"),  &settings.overlay_transparency, DEFAULT_OVERLAY_TRANSPARENCY))
            pConf->DeleteEntry(wxT("BR24RadarTransparency"));
        if (pConf->Read(wxT("BR24RadarGain"),  &settings.gain, 50))
            pConf->DeleteEntry(wxT("BR24RadarGain"));
        if (pConf->Read(wxT("BR24RadarRainGain"),  &settings.rain_clutter_gain, 50))
            pConf->DeleteEntry(wxT("BR24RadarRainGain"));
        if (pConf->Read(wxT("BR24RadarClutterGain"),  &settings.sea_clutter_gain, 50))
            pConf->DeleteEntry(wxT("BR24RadarClutterGain"));
        if (pConf->Read(wxT("BR24RadarRangeCalibration"),  &settings.range_calibration, 1.0))
            pConf->DeleteEntry(wxT("BR24RadarRangeCalibration"));
        if (pConf->Read(wxT("BR24RadarHeadingCorrection"),  &settings.heading_correction, 0))
            pConf->DeleteEntry(wxT("BR24RadarHeadingCorrection"));
        if (pConf->Read(wxT("BR24RadarInterface"), &settings.radar_interface, wxT("0.0.0.0")))
            pConf->DeleteEntry(wxT("BR24RadarInterface"));

        if (pConf->Read(wxT("BR24ControlsDialogSizeX"), &m_BR24Controls_dialog_sx, 300L))
            pConf->DeleteEntry(wxT("BR24ControlsDialogSizeX"));
        if (pConf->Read(wxT("BR24ControlsDialogSizeY"), &m_BR24Controls_dialog_sy, 540L))
            pConf->DeleteEntry(wxT("BR24ControlsDialogSizeY"));
        if (pConf->Read(wxT("BR24ControlsDialogPosX"), &m_BR24Controls_dialog_x, 20L))
            pConf->DeleteEntry(wxT("BR24ControlsDialogPosX"));
        if (pConf->Read(wxT("BR24ControlsDialogPosY"), &m_BR24Controls_dialog_y, 170L))
            pConf->DeleteEntry(wxT("BR24ControlsDialogPosY"));

        if (pConf->Read(wxT("BR24ManualDialogSizeX"), &m_BR24Manual_dialog_sx, 300L))
            pConf->DeleteEntry(wxT("BR24ManualDialogSizeX"));
        if (pConf->Read(wxT("BR24ManualDialogSizeY"), &m_BR24Manual_dialog_sy, 540L))
            pConf->DeleteEntry(wxT("BR24ManualDialogSizeY"));
        if (pConf->Read(wxT("BR24ManualDialogPosX"), &m_BR24Manual_dialog_x, 20L))
            pConf->DeleteEntry(wxT("BR24ManualDialogPosX"));
        if (pConf->Read(wxT("BR24ManualDialogPosY"), &m_BR24Manual_dialog_y, 170L))
            pConf->DeleteEntry(wxT("BR24ManualDialogPosY"));

        SaveConfig();
        return true;
    }

    return false;
}

bool br24radar_pi::SaveConfig(void)
{

    wxFileConfig *pConf = m_pconfig;

    if (pConf) {
        pConf->SetPath(wxT("/Plugins/BR24Radar"));
        pConf->Write(wxT("DisplayOption"), settings.display_option);
        pConf->Write(wxT("DisplayMode"), settings.display_mode);
        pConf->Write(wxT("Transparency"), settings.overlay_transparency);
        pConf->Write(wxT("Gain"), settings.gain);
        pConf->Write(wxT("RainGain"), settings.rain_clutter_gain);
        pConf->Write(wxT("ClutterGain"), settings.sea_clutter_gain);
        pConf->Write(wxT("RangeCalibration"),  settings.range_calibration);
        pConf->Write(wxT("HeadingCorrection"),  settings.heading_correction);
        pConf->Write(wxT("Interface"),  settings.radar_interface);

        pConf->Write(wxT("ControlsDialogSizeX"),  m_BR24Controls_dialog_sx);
        pConf->Write(wxT("ControlsDialogSizeY"),  m_BR24Controls_dialog_sy);
        pConf->Write(wxT("ControlsDialogPosX"),   m_BR24Controls_dialog_x);
        pConf->Write(wxT("ControlsDialogPosY"),   m_BR24Controls_dialog_y);

        pConf->Write(wxT("ManualDialogSizeX"),  m_BR24Manual_dialog_sx);
        pConf->Write(wxT("ManualDialogSizeY"),  m_BR24Manual_dialog_sy);
        pConf->Write(wxT("ManualDialogPosX"),   m_BR24Manual_dialog_x);
        pConf->Write(wxT("ManualDialogPosY"),   m_BR24Manual_dialog_y);
        pConf->Flush();
        wxLogMessage(wxT("BR24radar_pi: saved config"));

        return true;
    }

    return false;
}

// Positional Data passed from NMEA to plugin
void br24radar_pi::SetPositionFix(PlugIn_Position_Fix &pfix)
{
    br_ownship_lat = pfix.Lat;
    br_ownship_lon = pfix.Lon;

    br_bpos_set = true;

}
void br24radar_pi::SetPositionFixEx(PlugIn_Position_Fix_Ex &pfix)
{
    br_ownship_lat = pfix.Lat;
    br_ownship_lon = pfix.Lon;

    m_hdt_source = 0;
    if (!wxIsNaN(pfix.Hdt))
    {
        br_hdt = pfix.Hdt;
        m_hdt_source = 1;
    }
    else if (!wxIsNaN(pfix.Hdm) && !wxIsNaN(pfix.Var))
    {
        br_hdt = pfix.Hdm + pfix.Var;
        m_hdt_source = 2;
    }
    else if (!wxIsNaN(pfix.Cog))
    {
        br_hdt = pfix.Cog;
        m_hdt_source = 3;
    }

    if (m_hdt_source != m_hdt_prev_source)
    {
        switch (m_hdt_source)
        {
        case 0: wxLogMessage(wxT("BR24radar_pi: No Heading source; keeping old value %f"), br_hdt); break;
        case 1: wxLogMessage(wxT("BR24radar_pi: Heading source is now HDT")); break;
        case 2: wxLogMessage(wxT("BR24radar_pi: Heading source is now HDM")); break;
        case 3: wxLogMessage(wxT("BR24radar_pi: Heading source is now COG")); break;
        }
        m_hdt_prev_source = m_hdt_source;
    }

    br_bpos_set = true;
}

//************************************************************************
// Plugin Command Data Streams
//************************************************************************
void br24radar_pi::TransmitCmd(char* msg, int size)
{
    wxIPV4address addr101;
    addr101.Service(6680);        //(wxT("6680"));      addr101.Service(wxT("6680"));
    addr101.Hostname(wxT("236.6.7.10"));
    m_out_sock101->SendTo(addr101, msg, size);

    if (m_out_sock101->Error()) {
        wxLogMessage(wxT("BR24radar_pi: unable to transmit command, error %d\n"), m_out_sock101->LastError());
        return;
    };

    logBinaryData(wxT("sent command"), msg, size);
};

void br24radar_pi::RadarTxOff(void)
{
    wxLogMessage(wxT("BR24 Radar turned Off manually."));

    char pck[3] = {(byte)0x00, (byte)0xc1, (byte)0x00};
    TransmitCmd(pck, sizeof(pck));

    pck[0] = (byte)0x01;
    TransmitCmd(pck, sizeof(pck));
}

void br24radar_pi::RadarTxOn(void)
{
    wxLogMessage(wxT("BR24 Radar turned ON manually."));

    char pck[3] = {(byte)0x00, (byte)0xc1, (byte)0x01};               // ON
    TransmitCmd(pck, sizeof(pck));

    pck[0] = (byte)0x01;
    TransmitCmd(pck, sizeof(pck));
}

void br24radar_pi::RadarStayAlive(void)
{
    if (settings.master_mode && (br_radar_state == RADAR_ON)) {
        char pck[] = {(byte)0xA0, (byte)0xc1};
        TransmitCmd(pck, sizeof(pck));
    }
}

void br24radar_pi::Select_Range(int req_range_index)
{
    req_range_index = min(req_range_index, 15);
    if (settings.master_mode) {
        wxLogMessage(wxT("SelectRange: %g meters\n"), br24_range_settings[req_range_index].range_meters);
        TransmitCmd(br24_range_settings[req_range_index].range_command, sizeof(br24_range_settings[0].range_command));
    }
    else
    {
        wxLogMessage(wxT("Not master, ignore SelectRange: %g meters\n"), br24_range_settings[req_range_index].range_meters);
    }
}

void br24radar_pi::SetFilterProcess(int br_process, int sel_gain)
{
    wxString msg;

    if (settings.master_mode) {
        msg.Printf(wxT("SetFilterProcess: %d %d"), br_process, sel_gain);
        wxLogMessage(msg);

        switch (br_process) {
            case 0: {                        // Auto Gain
                    char cmd[] = {
                        (byte)0x06,
                        (byte)0xc1,
                        0, 0, 0, 0, (byte)0x01,
                        0, 0, 0, (byte)0xa1
                    };
                    msg.Printf(wxT("AutoGain: %o"), cmd);
                    wxLogMessage(msg);
                    TransmitCmd(cmd, sizeof(cmd));
                    break;
                }
            case 1: {                        // Manual Gain
                    char cmd[] = {
                        (byte)0x06,
                        (byte)0xc1,
                        0, 0, 0, 0, 0, 0, 0, 0,
                        (char)(sel_gain)
                    };
                    msg.Printf(wxT("ManualGain: %o"), cmd);
                    wxLogMessage(msg);
                    TransmitCmd(cmd, sizeof(cmd));
                    break;
                }
            case 2: {                       // Rain Clutter - Manual
                    char cmd[] = {
                        (byte)0x06,
                        (byte)0xc1,
                        (byte)0x04,
                        0, 0, 0, 0, 0, 0, 0,
                        (char)(sel_gain / 2)
                    };
                    msg.Printf(wxT("RainClutter: %o"), cmd);
                    wxLogMessage(msg);
                    TransmitCmd(cmd, sizeof(cmd));
                    break;
                }
            case 3: {                       // Sea Clutter - Auto
                    char cmd[11] = {
                        (byte)0x06,
                        (byte)0xc1,
                        (byte)0x02,
                        0, 0, 0, (byte)0x01,
                        0, 0, 0, (byte)0xd3
                    };
                    msg.Printf(wxT("SeaClutter: %o"), cmd);
                    wxLogMessage(msg);
                    TransmitCmd(cmd, sizeof(cmd));
                    break;
                }
            case 4: {                       // Sea Clutter - Manual
                    char cmd[] = {
                        (byte)0x06,
                        (byte)0xc1,
                        (byte)0x02,
                        0, 0, 0, 0, 0, 0, 0,
                        (char)(sel_gain)
                    };
                    msg.Printf(wxT("SeaClutter: %o"), cmd);
                    wxLogMessage(msg);
                    TransmitCmd(cmd, sizeof(cmd));
                    break;
                }
        };
    }
    else
    {
        wxLogMessage(wxT("Not master, so ignore SetFilterProcess: %d %d"), br_process, sel_gain);
    }
}

void br24radar_pi::SetRejectionMode(int mode)
{
    settings.rejection = mode;
    if (settings.master_mode) {
        char br_rejection_cmd[] = {
            (byte)0x08,
            (byte)0x0c,
            0, 0, 0, 0, 0, 0, 0, 0,
            (char) settings.rejection
        };
        TransmitCmd(br_rejection_cmd, sizeof(br_rejection_cmd));
        wxString msg;
        msg.Printf(wxT("Rejection: %o"), br_rejection_cmd);
        wxLogMessage(msg);
    }
}

//*****************************************************************************************************
void br24radar_pi::CacheSetToolbarToolBitmaps(int bm_id_normal, int bm_id_rollover)
{
    if ((bm_id_normal == m_sent_bm_id_normal) && (bm_id_rollover == m_sent_bm_id_rollover)) {
        return;    // no change needed
    }

    if ((bm_id_normal == -1) || (bm_id_rollover == -1)) {         // don't do anything, caller's responsibility
        m_sent_bm_id_normal = bm_id_normal;
        m_sent_bm_id_rollover = bm_id_rollover;
        return;
    }

    m_sent_bm_id_normal = bm_id_normal;
    m_sent_bm_id_rollover = bm_id_rollover;

    wxBitmap *pnormal = NULL;
    wxBitmap *prollover = NULL;

    switch (bm_id_normal) {
        case BM_ID_RED:
            pnormal = _img_radar_red;
            break;
        case BM_ID_RED_SLAVE:
            pnormal = _img_radar_red_slave;
            break;
        case BM_ID_GREEN:
            pnormal = _img_radar_green;
            break;
        case BM_ID_GREEN_SLAVE:
            pnormal = _img_radar_green_slave;
            break;
        case BM_ID_AMBER:
            pnormal = _img_radar_amber;
            break;
        case BM_ID_AMBER_SLAVE:
            pnormal = _img_radar_amber_slave;
            break;
        case BM_ID_BLANK:
            pnormal = _img_radar_blank;
            break;
        case BM_ID_BLANK_SLAVE:
            pnormal = _img_radar_blank_slave;
            break;
        default:
            break;
    }

    switch (bm_id_rollover) {
        case BM_ID_RED:
            prollover = _img_radar_red;
            break;
        case BM_ID_RED_SLAVE:
            prollover = _img_radar_red_slave;
            break;
        case BM_ID_GREEN:
            prollover = _img_radar_green;
            break;
        case BM_ID_GREEN_SLAVE:
            prollover = _img_radar_green_slave;
            break;
        case BM_ID_AMBER:
            prollover = _img_radar_amber;
            break;
        case BM_ID_AMBER_SLAVE:
            prollover = _img_radar_amber_slave;
            break;
        case BM_ID_BLANK:
            prollover = _img_radar_blank;
            break;
        case BM_ID_BLANK_SLAVE:
            prollover = _img_radar_blank_slave;
            break;
        default:
            break;
    }

    if ((pnormal) && (prollover)) {
        SetToolbarToolBitmaps(m_tool_id, pnormal, prollover);
    }
}



// Ethernet packet stuff *************************************************************

MulticastRXThread::~MulticastRXThread()
{
    delete m_sock;
}

void MulticastRXThread::OnExit()
{
  wxLogMessage(wxT("BR24radar_pi: radar thread is stopping."));
}

void *MulticastRXThread::Entry(void)
{

    //    Create a datagram socket for receiving data
    m_myaddr.AnyAddress();
    m_myaddr.Service(m_service_port);
    wxString msg;

    m_sock = new wxDatagramSocket(m_myaddr, wxSOCKET_REUSEADDR);

    if (!m_sock) {
        wxLogMessage(wxT("BR24radar_pi: Unable to listen on port %ls"), m_service_port.c_str());
        return 0;
    }
    m_sock->SetFlags(wxSOCKET_BLOCK); // We use blocking but use Wait() to avoid timeouts.

    //    Subscribe to a multicast group
    unsigned int mcAddr;
    unsigned int recvAddr;

#ifndef __WXMSW__
    GAddress gaddress;
    _GAddress_Init_INET(&gaddress);
    GAddress_INET_SetHostName(&gaddress, m_ip.mb_str());

    struct in_addr *addr;
    addr = &(((struct sockaddr_in *)gaddress.m_addr)->sin_addr);
    mcAddr = addr->s_addr;

    _GAddress_Init_INET(&gaddress);
    GAddress_INET_SetHostName(&gaddress, pPlugIn->settings.radar_interface.mb_str());
    addr = &(((struct sockaddr_in *)gaddress.m_addr)->sin_addr);
    recvAddr = addr->s_addr;

#else
    mcAddr = inet_addr(m_ip.mb_str());
    recvAddr = inet_addr(pPlugIn->settings.radar_interface.mb_str());
#endif

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = mcAddr;
    mreq.imr_interface.s_addr = recvAddr;

    bool bam = m_sock->SetOption(IPPROTO_IP, IP_ADD_MEMBERSHIP, (const void *)&mreq, sizeof(mreq));
    if (!bam) {
        wxLogError(wxT("Unable to listen to multicast group: %d"), m_sock->LastError());
        // wxMessageBox(wxT("Unable to listen to radar data"), wxT("BR24radar"), wxOK | wxICON_EXCLAMATION);
        return 0;
    }

    wxIPV4address rx_addr;
    rx_addr.Hostname(wxT("0.0.0.0"));   // UDP sender broadcast IP is dynamically assigned else standard IGMPV4
    // just so long as it looks like an IP

    //    The big while....
    int n_rx_once = 0;
    while (!*m_quit) {
        if (m_sock->Wait(1, 0))
        {
            m_sock->RecvFrom(rx_addr, br_packet.buf, sizeof(br_packet.buf));
            if (m_sock->LastCount()) {
                if (0 == n_rx_once) {
                    wxLogMessage(wxT("BR24radar_pi: First Packet Received."));
                    n_rx_once++;
                }
                br_scan_packets_per_tick++;
                process_buffer();
            }
        }
    }
    return 0;
}

void MulticastRXThread::process_buffer(void)
// We only get Data packets of fixed length from PORT (6678), see structure in .h file
// Sequence Header - 8 bytes
// 32 line header/data sets per packet
//      Line Header - 24 bytes
//      Line Data - 512 bytes
{
    for (int scanline = 0; scanline < 32 ; scanline++) {
        int scan_start = scanline * (24 + 512);

        //  sort Data in Packet - this is a workaround for not being able to do Union-structures directly

        memcpy(&br_line, &br_packet.br_frame_pkt.scan_lines[scan_start], 536);

        double range_raw = 0;
        int angleraw;

        bool model4G = false;
        short int large_range;
        short int small_range;

        if (memcmp(br_line.br24.mark, BR24MARK, sizeof(BR24MARK)) == 0) {

            range_raw = ((br_line.br24.range[2] & 0xff) << 16 | (br_line.br24.range[1] & 0xff) << 8 | (br_line.br24.range[0] & 0xff)); // Little -> big endian
            angleraw = (br_line.br24.angle[1] << 8) | br_line.br24.angle[0];
            br_scan_range_meters =  range_raw * 10 ;  // max 24 km => 24000
        } else {
            large_range = (br_line.br4g.largerange[1] << 8) | br_line.br4g.largerange[0];
            small_range = (br_line.br4g.smallrange[1] << 8) | br_line.br4g.smallrange[0];

            if (large_range == 0x80) {
                if (small_range == -1) {
                    wxLogMessage(wxT("BR24radar_pi:  Neither 4G range field set\n"));
                } else {
                    range_raw = small_range;
                }
            } else {
                range_raw = large_range * 348;
            }
            angleraw = (br_line.br4g.angle[1] << 8) | br_line.br4g.angle[0];
            br_scan_range_meters = range_raw * 0.1 * sqrt((double) 2.0);
            model4G = true;
        }

        // Range change desired?
        if (br_range_meters != br_scan_range_meters) {
            if (model4G)
            {
              logBinaryData(wxT("4G line header"), (char *) &br_line, sizeof(br_line.br4g));
              wxLogMessage(wxT("4G header large_range=%x %x small_range=%x %x")
                          , br_line.br4g.largerange[0], br_line.br4g.largerange[1]
                          , br_line.br4g.smallrange[0], br_line.br4g.smallrange[1]
                          );
              wxLogMessage(wxT("4G model range_raw=%f large_range=%x small_range=%x"), range_raw, large_range, small_range);
            }
            else
            {
              logBinaryData(wxT("BR24 line header"), (char *) &br_line, sizeof(br_line.br24));
              wxLogMessage(wxT("BR24 header range=%x %x %x angle=%x %x")
                          , br_line.br24.range[0]
                          , br_line.br24.range[1]
                          , br_line.br24.range[2]
                          , br_line.br24.angle[0]
                          , br_line.br24.angle[1]
                          );
              wxLogMessage(wxT("BR24 model range_raw=%f angle_raw=%f"), range_raw, angleraw);
            }

            wxLogMessage(wxT("BR24radar_pi:  Range Change: %g --> %g meters"), br_range_meters, br_scan_range_meters);

            br_range_meters = br_scan_range_meters;
            br_sweep_count = 0;

        }

        long angle = angleraw * 360 / 4096;
        //wxLogMessage(wxT("RXThreadProcessBuffer:  Angle: %d %d"),angle, angleraw);

        if (angle < min_angle) {            // setting new LB and fixing UB
            min_angle = angle;
        }
        if (angle > max_angle) {            // setting new UB and fixing LB
            max_angle = angle;
        }

        if (angle < max_angle) {
            br_sweep_count++;
            max_angle = 0;
            min_angle = 361;
        }
//      wxLogMessage(wxT("RXThreadProcessBuffer:  Scan test: %d %d %d"),min_angle, max_angle, br_sweep_count);

        unsigned char *dest_data1 = &br_scan_buf[angle][0];  // start address of destination in scan_buf
//      unsigned char *dest_data2 = &br_scan_buf[angle+1][0]; // next line
        memcpy(dest_data1, br_line.data, 512);
//      memcpy(dest_data2, br_line.data, 512);              // both lines get same scanner data
        br_scan_buf[angle][511] = (byte)0xff;               // Max Range Line

//          if (outfile.is_open())
//              outfile.write((const char*)dest_data1, 512); // data line copied to scan buffer.

        int alpha, alpha2;                              // Filling in odd radar bearing values
        for (int angle = 0 ; angle < 358 ; angle += 2) {
            for (int radius = 0; radius < 512; ++radius) {
                alpha = br_scan_buf[angle][radius];
                alpha2 = br_scan_buf[(angle + 2) % 360][radius];
                if (alpha2 > 50) {                      // 50 is the lower threshold
                    br_scan_buf[angle + 1][radius] = (alpha + alpha2) / 2;
                }
            }
        }

        if ((pPlugIn->settings.display_mode == 1) && (br_sweep_count > 2)) {    // after a couple of sweeps update static display screen

#ifdef BR24_DOUBLE_BUFFERED
            memcpy(br_static_buf, br_scan_buf, sizeof(br_static_buf));
            memset(br_scan_buf, 0, sizeof(br_scan_buf));
#endif

            br_static_timestamp = wxDateTime::Now();
            br_sweep_count = 0;
        }
    }

//          if(outfile.is_open())
//            outfile.close();
}


/********************************************************************************************************/
//   Distance measurement for simple sphere
/********************************************************************************************************/

double deg2rad(double deg)
{
    return (deg * PI / 180.0);
}

double rad2deg(double rad)
{
    return (rad * 180.0 / PI);
}

double radar_distance(double lat1, double lon1, double lat2, double lon2, char unit)
{
    // Spherical Law of Cosines
    double theta, dist;

    theta = lon2 - lon1;
    dist = sin(deg2rad(lat1)) * sin(deg2rad(lat2)) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta));
    dist = acos(dist);         // radians
    dist = rad2deg(dist);
    dist = fabs(dist) * 60;    // nautical miles/degree
    switch (unit) {
        case 'M':              // statute miles
            dist = dist * 1.1515;
            break;
        case 'K':              // kilometers
            dist = dist * 1.852;
            break;
        case 'N':              // nautical miles
            break;
    }
    return dist;
}
