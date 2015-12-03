/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Navico br24 Radar Plugin
 * Author:   David Register
 *           Dave Cowell
 *           Kees Verruijt
 *           Douwe Fokkema
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

#include "br24ControlsDialog.h"

enum {                                      // process ID's
    ID_TEXTCTRL1 = 10000,
    ID_BACK,
    ID_PLUS_TEN,
    ID_PLUS,
    ID_VALUE,
    ID_MINUS,
    ID_MINUS_TEN,
    ID_AUTO,
    ID_MULTISWEEP,

    ID_MSG_BACK,
    ID_RDRONLY,

    ID_ADVANCED_BACK,
    ID_INSTALLATION_BACK,
    ID_TRANSPARENCY,
    ID_INTERFERENCE_REJECTION,
    ID_TARGET_BOOST,
    ID_NOISE_REJECTION,
    ID_TARGET_SEPARATION,
    ID_REFRESHRATE,
    ID_SCAN_SPEED,
    ID_INSTALLATION,
    ID_TIMED_IDLE,

    ID_BEARING_ALIGNMENT,
    ID_ANTENNA_HEIGHT,
    ID_LOCAL_INTERFERENCE_REJECTION,
    ID_SIDE_LOBE_SUPPRESSION,

    ID_RADAR_STATE,
    ID_RADAR_OVERLAY,
    ID_RANGE,
    ID_GAIN,
    ID_SEA,
    ID_RAIN,
    ID_ADVANCED,
    ID_ZONE1,
    ID_ZONE2,

    ID_MESSAGE,
    ID_BPOS,
    ID_HEADING,
    ID_RADAR,
    ID_DATA

};

//---------------------------------------------------------------------------------------
//          Radar Control Implementation
//---------------------------------------------------------------------------------------
IMPLEMENT_CLASS(br24ControlsDialog, wxDialog)

BEGIN_EVENT_TABLE(br24ControlsDialog, wxDialog)

EVT_CLOSE(br24ControlsDialog::OnClose)
EVT_BUTTON(ID_BACK, br24ControlsDialog::OnBackClick)
EVT_BUTTON(ID_PLUS_TEN,  br24ControlsDialog::OnPlusTenClick)
EVT_BUTTON(ID_PLUS,  br24ControlsDialog::OnPlusClick)
EVT_BUTTON(ID_MINUS, br24ControlsDialog::OnMinusClick)
EVT_BUTTON(ID_MINUS_TEN, br24ControlsDialog::OnMinusTenClick)
EVT_BUTTON(ID_AUTO,  br24ControlsDialog::OnAutoClick)
EVT_BUTTON(ID_MULTISWEEP,  br24ControlsDialog::OnMultiSweepClick)

EVT_BUTTON(ID_ADVANCED_BACK,  br24ControlsDialog::OnAdvancedBackButtonClick)

EVT_BUTTON(ID_INSTALLATION_BACK, br24ControlsDialog::OnInstallationBackButtonClick)
EVT_BUTTON(ID_TRANSPARENCY, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_INTERFERENCE_REJECTION, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_TARGET_BOOST, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_NOISE_REJECTION, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_TARGET_SEPARATION, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_REFRESHRATE, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_SCAN_SPEED, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_INSTALLATION, br24ControlsDialog::OnInstallationButtonClick)
EVT_BUTTON(ID_TIMED_IDLE, br24ControlsDialog::OnRadarControlButtonClick)

EVT_BUTTON(ID_BEARING_ALIGNMENT, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_ANTENNA_HEIGHT, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_LOCAL_INTERFERENCE_REJECTION, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_SIDE_LOBE_SUPPRESSION, br24ControlsDialog::OnRadarControlButtonClick)

EVT_BUTTON(ID_RADAR_STATE, br24ControlsDialog::OnRadarStateButtonClick)
EVT_BUTTON(ID_RADAR_OVERLAY, br24ControlsDialog::OnRadarOverlayButtonClick)
EVT_BUTTON(ID_RANGE, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_GAIN, br24ControlsDialog::OnRadarGainButtonClick)
EVT_BUTTON(ID_SEA, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_RAIN, br24ControlsDialog::OnRadarControlButtonClick)
EVT_BUTTON(ID_ADVANCED, br24ControlsDialog::OnAdvancedButtonClick)
EVT_BUTTON(ID_ZONE1, br24ControlsDialog::OnZone1ButtonClick)
EVT_BUTTON(ID_ZONE2, br24ControlsDialog::OnZone2ButtonClick)
EVT_BUTTON(ID_MESSAGE, br24ControlsDialog::OnMessageButtonClick)

EVT_MOVE(br24ControlsDialog::OnMove)
EVT_SIZE(br24ControlsDialog::OnSize)

END_EVENT_TABLE()

class br24RadarControlButton: public wxButton
{
public:
    br24RadarControlButton()
    {

    };

    br24RadarControlButton(br24ControlsDialog *parent,
                       wxWindowID id,
                       const wxString& label,
                       ControlType ct,
                       bool newHasAuto,
                       int newValue
                       )
    {
        Create(parent, id, label + wxT("\n"), wxDefaultPosition, g_buttonSize, 0, wxDefaultValidator, label);

        m_parent = parent;
        minValue = 0;
        maxValue = 100;
        value = 0;
        if (ct == CT_GAIN)
        {
          value = 50;
        }
        hasAuto = newHasAuto;
        firstLine = label;
        names = 0;
        controlType = ct;
        if (hasAuto) {
            SetLocalAuto();
        } else {
            SetLocalValue(newValue);
        }

        this->SetFont(m_parent->m_pi->m_font);
    }

    virtual void SetValue(int value);
    virtual void SetAuto();
    virtual void SetLocalValue(int newValue);
    virtual void SetLocalAuto();
    const wxString  *names;

    wxString   firstLine;

    br24ControlsDialog * m_parent;

    int        value;

    int        minValue;
    int        maxValue;
    bool       hasAuto;
    ControlType controlType;

};

class br24RadarRangeControlButton: public br24RadarControlButton
{
public:
    br24RadarRangeControlButton(br24ControlsDialog *parent,
                            wxWindowID id,
                            const wxString& label
                            )
    {
        Create(parent, id, label + wxT("\n"), wxDefaultPosition, g_buttonSize, 0, wxDefaultValidator, label);

        m_parent = parent;
        minValue = 0;
        maxValue = 0;
        auto_range_index = 0;
        value = -1; // means: never set
        hasAuto = true;
        firstLine = label;
        names = 0;
        controlType = CT_RANGE;

        this->SetFont(m_parent->m_pi->m_font);
    }

    virtual void SetValue(int value);
    virtual void SetAuto();
    int SetValueInt(int value);

    br24ControlsDialog * m_parent;
    bool isRemote; // Set by some other computer or MFD
    int auto_range_index;
};

struct RadarRanges {
  int meters;
  int actual_meters;
  const char * name;
};

static const RadarRanges g_ranges_metric[] =
{
  /* Nautical (mixed) first */
    {    50,    98, "50 m" },
    {    75,   146, "75 m" },
    {   100,   195, "100 m" },
    {   250,   488, "1/4 km" },
    {   500,   808, "1/2 km" },
    {   750,  1154, "3/4 km" },
    {  1000,  1616, "1 km" },
    {  1500,  2308, "1.5 km" },
    {  2000,  3366, "2 km" },
    {  3000,  4713, "3 km" },
    {  4000,  5655, "4 km" },
    {  6000,  9408, "6 km" },
    {  8000, 12096, "8 km" },
    { 12000, 18176, "12 km" },
    { 16000, 26240, "16 km" },
    { 24000, 36352, "24 km" },
    { 36000, 52480, "36 km" },
    { 48000, 72704, "48 km" }
};

static const RadarRanges g_ranges_nautic[] =
{
   {    50,            98, "50 m" },
   {    75,           146, "75 m" },
   {   100,           195, "100 m" },
   {  1852 / 8,       451, "1/8 NM" },
   {  1852 / 4,       673, "1/4 NM" },
   {  1852 / 2,      1346, "1/2 NM" },
   {  1852 * 3 / 4,  2020, "3/4 NM" },
   {  1852 * 1,      2693, "1 NM" },
   {  1852 * 3 / 2,  4039, "1.5 NM" },
   {  1852 * 2,      5655, "2 NM" },
   {  1852 * 3,      8079, "3 NM" },
   {  1852 * 4,     10752, "4 NM" },
   {  1852 * 6,     16128, "6 NM" },
   {  1852 * 8,     22208, "8 NM" },
   { 1852 * 12,     36352, "12 NM" },
   { 1852 * 16,     44416, "16 NM" },
   { 1852 * 24,     72704, "24 NM" }
};

static const int METRIC_RANGE_COUNT = ARRAY_SIZE(g_ranges_metric);
static const int NAUTIC_RANGE_COUNT = ARRAY_SIZE(g_ranges_nautic);

static const int g_range_maxValue[2] = { NAUTIC_RANGE_COUNT - 1, METRIC_RANGE_COUNT - 1 };

wxString interference_rejection_names[4];
wxString target_separation_names[4];
wxString noise_rejection_names[3];
wxString target_boost_names[3];
wxString scan_speed_names[2];
wxString timed_idle_times[8];

extern size_t convertRadarMetersToIndex(int * range_meters, int units, RadarType radarType)
{
    const RadarRanges * ranges;
    int myrange = *range_meters;
    size_t n;

    n = g_range_maxValue[units];
    ranges = units ? g_ranges_metric : g_ranges_nautic;

    if (radarType != RT_4G) {
        n--; // only 4G has longest ranges
    }
    for (; n > 0; n--) {
        if (ranges[n].actual_meters <= myrange) {  // step down until past the right range value
            break;
        }
    }
    *range_meters = ranges[n].meters;

    return n;
}

extern size_t convertMetersToRadarAllowedValue(int * range_meters, int units, RadarType radarType)
{
    const RadarRanges * ranges;
    int myrange = *range_meters;
    size_t n;

    n = g_range_maxValue[units];
    ranges = units ? g_ranges_metric : g_ranges_nautic;

    if (radarType != RT_4G) {
        n--; // only 4G has longest ranges
    }
    for (; n > 0; n--) {
        if (ranges[n].meters <= myrange) {  // step down until past the right range value
            break;
        }
    }
    *range_meters = ranges[n].meters;

    return n;
}

void br24RadarControlButton::SetValue(int newValue)
{
    SetLocalValue(newValue);
    m_parent->m_ri->SetControlValue(controlType, value);
}

void br24RadarControlButton::SetLocalValue(int newValue)
{            // sets value in the button without sending new value to the radar
    if (newValue < minValue) {
        value = minValue;
    }
    else if (newValue > maxValue) {
        value = maxValue;
    }
    else {
        value = newValue;
    }
    wxString label;

    if (names) {
        label.Printf(wxT("%s\n%s"), firstLine.c_str(), names[value].c_str());
    }
    else {
        label.Printf(wxT("%s\n%d"), firstLine.c_str(), value);
    }

    this->SetLabel(label);

}

void br24RadarControlButton::SetAuto()
{
    SetLocalAuto();
    m_parent->m_ri->SetControlValue(controlType, -1);
}

void br24RadarControlButton::SetLocalAuto()
{      // sets auto in the button without sending new value to the radar
    wxString label;

    label << firstLine << wxT("\n") << _("Auto");
    this->SetLabel(label);
}


int br24RadarRangeControlButton::SetValueInt(int newValue)
{
    // newValue is the new range index number
    // sets the new range label in the button and returns the new range in meters
    int units = m_parent->m_pi->m_settings.range_units;

    maxValue = g_range_maxValue[units];
    int oldValue = value;  // for debugging only
    if (newValue >= minValue && newValue <= maxValue) {
        value = newValue;
    }
    else if (m_parent->m_ri->auto_range_mode) {
        value = auto_range_index;
    } else if (value > maxValue) {
        value = maxValue;
    }

    int meters = units ? g_ranges_metric[value].meters : g_ranges_nautic[value].meters;
    wxString label;
    wxString rangeText = value < 0 ? wxT("?") : wxString::FromUTF8(units ? g_ranges_metric[value].name : g_ranges_nautic[value].name);
    if (isRemote) {
        label << firstLine << wxT("\n") << _("Remote") << wxT(" (") << rangeText << wxT(")");
    }
    else if (m_parent->m_ri->auto_range_mode) {
        label << firstLine << wxT("\n") << _("Auto") << wxT(" (") << rangeText << wxT(")");
    }
    else {
        label << firstLine << wxT("\n") << rangeText;
    }
    this->SetLabel(label);
    if (m_parent->m_pi->m_settings.verbose > 0) {
        wxLogMessage(wxT("br24radar_pi: Range label '%s' auto=%d remote=%d unit=%d max=%d new=%d old=%d"),
            rangeText.c_str(), m_parent->m_ri->auto_range_mode, isRemote, units, maxValue, newValue, oldValue);
    }

    return meters;
}



void br24RadarRangeControlButton::SetValue(int newValue)
{
    // newValue is the index of the new range
    // sends the command for the new range to the radar
    isRemote = false;
    m_parent->m_ri->auto_range_mode = false;

    int meters = SetValueInt(newValue);   // do not display the new value now, will be done by receive thread when frame with new range is received
    m_parent->m_ri->SetRangeMeters(meters);        // send new value to the radar
}

void br24RadarRangeControlButton::SetAuto()
{
    m_parent->m_ri->auto_range_mode = true;
    /*discard*/ SetValueInt(-1);
}

br24ControlsDialog::br24ControlsDialog()
{
    Init();
}

br24ControlsDialog::~br24ControlsDialog()
{
}


void br24ControlsDialog::Init()
{
    // Initialize all members that need initialization
}

bool br24ControlsDialog::Create(wxWindow *parent, br24radar_pi *ppi, RadarInfo * ri,
                                wxWindowID id, const wxString& caption,
                                const wxPoint& pos, const wxSize& size, long style)
{
    m_parent = parent;
    m_pi = ppi;
    m_ri = ri;

#ifdef wxMSW
    long wstyle = wxSYSTEM_MENU | wxCLOSE_BOX | wxCAPTION | wxCLIP_CHILDREN;
#else
    long wstyle =                 wxCLOSE_BOX | wxCAPTION | wxRESIZE_BORDER;
#endif

    if (!wxDialog::Create(parent, id, caption, pos, wxDefaultSize, wstyle)) {
        return false;
    }

    CreateControls();
    return true;
}


void br24ControlsDialog::CreateControls()
{
    static int BORDER = 0;

    // A top-level sizer
    m_top_sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(m_top_sizer);

    /*
     * Here be dragons... Since I haven't been able to create buttons that adapt up, and at the same time
     * calculate the biggest button, and I want all buttons to be the same width I use a trick.
     * I generate a multiline StaticText containing all the (long) button labels and find out what the width
     * of that is, and then generate the buttons using that width.
     * I know, this is a hack, but this way it works relatively nicely even with translations.
     */

    wxBoxSizer * testBox = new wxBoxSizer(wxVERTICAL);
    m_top_sizer->Add(testBox, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, BORDER);

    wxString label;
    label << _("Transparency") << wxT("\n");
    label << _("Interference rejection") << wxT("\n");
    label << _("Target separation") << wxT("\n");
    label << _("Noise rejection") << wxT("\n");
    label << _("Target boost") << wxT("\n");
    label << _("Scan speed") << wxT("\n");
    label << _("Scan age") << wxT("\n");
    label << _("Timed Transmit") << wxT("\n");
    label << _("Gain") << wxT("\n");
    label << _("Sea clutter") << wxT("\n");
    label << _("Rain clutter") << wxT("\n");
    label << _("Auto") << wxT(" (1/20 NM)\n");
    label << _("Overlay") << wxT("\n") << _("Off\n");

    wxStaticText * testMessage = new wxStaticText(this, ID_BPOS, label, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE | wxST_NO_AUTORESIZE);
    testBox->Add(testMessage, 0, wxALL, 2);
    testMessage->SetFont(m_pi->m_font);

    m_top_sizer->Fit(this);
    m_top_sizer->Layout();
    int width = m_top_sizer->GetSize().GetWidth() + 10;
    wxSize bestSize = GetBestSize();
    if (width < bestSize.GetWidth()) {
        width = bestSize.GetWidth();
    }
    if (width < 100) {
        width = 100;
    }
    if (width > 300) {
        width = 300;
    }
    g_buttonSize = wxSize(width, 40);  // was 50, buttons a bit lower now
    if (m_pi->m_settings.verbose) {
        wxLogMessage(wxT("br24radar_pi: Dynamic button width = %d"), g_buttonSize.GetWidth());
    }
    m_top_sizer->Hide(testBox);
    m_top_sizer->Remove(testBox);
    // delete testBox; -- this core dumps!
    // Determined desired button width

    g_buttonSize = wxSize(width, 50);
    //**************** EDIT BOX ******************//
     // A box sizer to contain RANGE button
    m_edit_sizer = new wxBoxSizer(wxVERTICAL);
    m_top_sizer->Add(m_edit_sizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, BORDER);

    // A box sizer to contain RANGE button
    m_edit_sizer = new wxBoxSizer(wxVERTICAL);
    m_top_sizer->Add(m_edit_sizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, BORDER);

    // The <<Back button
    wxButton * back_button = new wxButton(this, ID_BACK, _("<<\nBack"), wxDefaultPosition, g_buttonSize, 0);
    m_edit_sizer->Add(back_button, 0, wxALL, BORDER);
    back_button->SetFont(m_pi->m_font);

    // The +10 button
    m_plus_ten_button = new wxButton(this, ID_PLUS_TEN, _("+10"), wxDefaultPosition, g_buttonSize, 0);
    m_edit_sizer->Add(m_plus_ten_button, 0, wxALL, BORDER);
    m_plus_ten_button->SetFont(m_pi->m_font);

    // The + button
    m_plus_button = new wxButton(this, ID_PLUS, _("+"), wxDefaultPosition, g_buttonSize, 0);
    m_edit_sizer->Add(m_plus_button, 0, wxALL, BORDER);
    m_plus_button->SetFont(m_pi->m_font);

    // The VALUE button
    m_value_text = new wxStaticText(this, ID_VALUE, _("Value"), wxDefaultPosition, g_buttonSize, wxALIGN_CENTRE | wxST_NO_AUTORESIZE);
    m_edit_sizer->Add(m_value_text, 0, wxALL, BORDER);
    m_value_text->SetFont(m_pi->m_fat_font);
    m_value_text->SetBackgroundColour(*wxLIGHT_GREY);

    // The - button
    m_minus_button = new wxButton(this, ID_MINUS, _("-"), wxDefaultPosition, g_buttonSize, 0);
    m_edit_sizer->Add(m_minus_button, 0, wxALL, BORDER);
    m_minus_button->SetFont(m_pi->m_font);

    // The -10 button
    m_minus_ten_button = new wxButton(this, ID_MINUS_TEN, _("-10"), wxDefaultPosition, g_buttonSize, 0);
    m_edit_sizer->Add(m_minus_ten_button, 0, wxALL, BORDER);
    m_minus_ten_button->SetFont(m_pi->m_font);

    // The Auto button
    m_auto_button = new wxButton(this, ID_AUTO, _("Auto"), wxDefaultPosition, g_buttonSize, 0);
    m_edit_sizer->Add(m_auto_button, 0, wxALL, BORDER);
    m_auto_button->SetFont(m_pi->m_font);

    // The Multi Sweep Filter button
    wxString labelMS;
    labelMS << _("Multi Sweep Filter") << wxT("\n") << _("Off");
    m_multi_sweep_button = new wxButton(this, ID_MULTISWEEP, labelMS, wxDefaultPosition, wxSize(width, 40), 0);
    m_edit_sizer->Add(m_multi_sweep_button, 0, wxALL, BORDER);
    m_multi_sweep_button->SetFont(m_pi->m_font);

    m_top_sizer->Hide(m_edit_sizer);


    //**************** ADVANCED BOX ******************//
    // These are the controls that the users sees when the Advanced button is selected

    m_advanced_sizer = new wxBoxSizer(wxVERTICAL);
    m_top_sizer->Add(m_advanced_sizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, BORDER);

    // The Back button
    wxString backButtonStr;
    backButtonStr << wxT("<<\n") << _("Back");
    wxButton * bAdvancedBack = new wxButton(this, ID_ADVANCED_BACK, backButtonStr, wxDefaultPosition, g_buttonSize, 0);
    m_advanced_sizer->Add(bAdvancedBack, 0, wxALL, BORDER);
    bAdvancedBack->SetFont(m_pi->m_font);

    // The TRANSPARENCY button
    m_transparency_button = new br24RadarControlButton(this, ID_TRANSPARENCY, _("Transparency"), CT_TRANSPARENCY, false, m_pi->m_settings.overlay_transparency);
    m_advanced_sizer->Add(m_transparency_button, 0, wxALL, BORDER);
    m_transparency_button->minValue = MIN_OVERLAY_TRANSPARENCY;
    m_transparency_button->maxValue = MAX_OVERLAY_TRANSPARENCY;

    // The REJECTION button

    interference_rejection_names[0] = _("Off");
    interference_rejection_names[1] = _("Low");
    interference_rejection_names[2] = _("Medium");
    interference_rejection_names[3] = _("High");

    m_interference_rejection_button = new br24RadarControlButton(this, ID_INTERFERENCE_REJECTION, _("Interference rejection"),
        CT_INTERFERENCE_REJECTION, false, m_ri->interference_rejection.button);
    m_advanced_sizer->Add(m_interference_rejection_button, 0, wxALL, BORDER);
    m_interference_rejection_button->minValue = 0;
    m_interference_rejection_button->maxValue = ARRAY_SIZE(interference_rejection_names) - 1;
    m_interference_rejection_button->names = interference_rejection_names;
    m_interference_rejection_button->SetLocalValue(m_ri->interference_rejection.button); // redraw after adding names

    // The TARGET BOOST button
    target_boost_names[0] = _("Off");
    target_boost_names[1] = _("Low");
    target_boost_names[2] = _("High");
    m_target_boost_button = new br24RadarControlButton(this, ID_TARGET_BOOST, _("Target boost"), CT_TARGET_BOOST, false,
        m_ri->target_boost.button);
    m_advanced_sizer->Add(m_target_boost_button, 0, wxALL, BORDER);
    m_target_boost_button->minValue = 0;
    m_target_boost_button->maxValue = ARRAY_SIZE(target_boost_names) - 1;
    m_target_boost_button->names = target_boost_names;
    m_target_boost_button->SetLocalValue(m_ri->target_boost.button); // redraw after adding names


    // The NOISE REJECTION button
    noise_rejection_names[0] = _("Off");
    noise_rejection_names[1] = _("Low");
    noise_rejection_names[2] = _("High");

    m_noise_rejection_button = new br24RadarControlButton(this, ID_NOISE_REJECTION, _("Noise rejection"), CT_NOISE_REJECTION, false,
        m_ri->noise_rejection.button);
    m_advanced_sizer->Add(m_noise_rejection_button, 0, wxALL, BORDER);
    m_noise_rejection_button->minValue = 0;
    m_noise_rejection_button->maxValue = ARRAY_SIZE(noise_rejection_names) - 1;
    m_noise_rejection_button->names = noise_rejection_names;
    m_noise_rejection_button->SetLocalValue(m_ri->noise_rejection.button); // redraw after adding names

    m_advanced_4G_sizer = new wxBoxSizer(wxVERTICAL);
    m_advanced_sizer->Add(m_advanced_4G_sizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 0);

    // The TARGET SEPARATION button

    target_separation_names[0] = _("Off");
    target_separation_names[1] = _("Low");
    target_separation_names[2] = _("Medium");
    target_separation_names[3] = _("High");

    m_target_separation_button = new br24RadarControlButton(this, ID_TARGET_SEPARATION, _("Target separation"), CT_TARGET_SEPARATION, false,
        m_ri->target_separation.button);
    m_advanced_4G_sizer->Add(m_target_separation_button, 0, wxALL, BORDER);
    m_target_separation_button->minValue = 0;
    m_target_separation_button->maxValue = ARRAY_SIZE(target_separation_names) - 1;
    m_target_separation_button->names = target_separation_names;
    m_target_separation_button->SetLocalValue(m_ri->target_separation.button); // redraw after adding names

    // The SCAN SPEED button
    scan_speed_names[0] = _("Normal");
    scan_speed_names[1] = _("Fast");
    m_scan_speed_button = new br24RadarControlButton(this, ID_SCAN_SPEED, _("Scan speed"), CT_SCAN_SPEED, false, m_ri->scan_speed.button);
    m_advanced_sizer->Add(m_scan_speed_button, 0, wxALL, BORDER);
    m_scan_speed_button->minValue = 0;
    m_scan_speed_button->maxValue = ARRAY_SIZE(scan_speed_names) - 1;
    m_scan_speed_button->names = scan_speed_names;
    m_scan_speed_button->SetLocalValue(m_ri->scan_speed.button); // redraw after adding names

    // The REFRESHRATE button
    m_refresh_rate_button = new br24RadarControlButton(this, ID_REFRESHRATE, _("Refresh rate"), CT_REFRESHRATE, false, m_pi->m_refresh_rate);
    m_advanced_sizer->Add(m_refresh_rate_button, 0, wxALL, BORDER);
    m_refresh_rate_button->minValue = 1;
    m_refresh_rate_button->maxValue = 5;

    // The INSTALLATION button
    wxButton * bInstallation = new wxButton(this, ID_INSTALLATION, _("Installation"), wxDefaultPosition, g_buttonSize, 0);
    m_advanced_sizer->Add(bInstallation, 0, wxALL, BORDER);
    bInstallation->SetFont(m_pi->m_font);

    // The TIMED TRANSMIT button
    timed_idle_times[0] = _("Off");
    timed_idle_times[1] = _("5 min");
    timed_idle_times[2] = _("10 min");
    timed_idle_times[3] = _("15 min");
    timed_idle_times[4] = _("20 min");
    timed_idle_times[5] = _("25 min");
    timed_idle_times[6] = _("30 min");
    timed_idle_times[7] = _("35 min");
    wxString TT_Label;
    TT_Label << wxT("\n") << _("Back to activate");
    for (int i = 1; i < 8; i++) {
        timed_idle_times[i] << TT_Label;
    }

    m_timed_idle_button = new br24RadarControlButton(this, ID_TIMED_IDLE, _("Timed Transmit"), CT_TIMED_IDLE, false, m_pi->m_settings.timed_idle); //HakanToDo new setting
    m_advanced_sizer->Add(m_timed_idle_button, 0, wxALL, BORDER);
    m_timed_idle_button->minValue = 0;
    m_timed_idle_button->maxValue = ARRAY_SIZE(timed_idle_times) - 1;
    m_timed_idle_button->names = timed_idle_times;
    m_timed_idle_button->SetValue(m_pi->m_settings.timed_idle); // redraw after adding names

    m_top_sizer->Hide(m_advanced_sizer);

    //**************** Installation BOX ******************//
    // These are the controls that the users sees when the Installation button is selected

    m_installation_sizer = new wxBoxSizer(wxVERTICAL);
    m_top_sizer->Add(m_installation_sizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, BORDER);

    // The Back button
    wxString instBackButtonStr;
    instBackButtonStr << wxT("<<\n") << _("Back");
    wxButton * bInstallationBack = new wxButton(this, ID_INSTALLATION_BACK, instBackButtonStr, wxDefaultPosition, g_buttonSize, 0);
    m_installation_sizer->Add(bInstallationBack, 0, wxALL, BORDER);
    bInstallationBack->SetFont(m_pi->m_font);

    // The BEARING ALIGNMENT button
    m_bearing_alignment_button = new br24RadarControlButton(this, ID_BEARING_ALIGNMENT, _("Bearing alignment"), CT_BEARING_ALIGNMENT,
        false, m_ri->bearing_alignment.button);
    m_installation_sizer->Add(m_bearing_alignment_button, 0, wxALL, BORDER);
    m_bearing_alignment_button->SetFont(m_pi->m_font);    // this bearing alignment work opposite to the one defined in the pi!
    m_bearing_alignment_button->minValue = -179;
    m_bearing_alignment_button->maxValue = 180;

    // The ANTENNA HEIGHT button
    m_antenna_height_button = new br24RadarControlButton(this, ID_ANTENNA_HEIGHT, _("Antenna height"),
        CT_ANTENNA_HEIGHT, false, m_ri->antenna_height.button);
    m_installation_sizer->Add(m_antenna_height_button, 0, wxALL, BORDER);
    m_antenna_height_button->minValue = 0;
    m_antenna_height_button->maxValue = 30;

    // The LOCAL INTERFERENCE REJECTION button
    m_local_interference_rejection_button = new br24RadarControlButton(this, ID_LOCAL_INTERFERENCE_REJECTION, _("Local interference rejection"),
        CT_LOCAL_INTERFERENCE_REJECTION, false, m_ri->local_interference_rejection.button);
    m_installation_sizer->Add(m_local_interference_rejection_button, 0, wxALL, BORDER);
    m_local_interference_rejection_button->minValue = 0;
    m_local_interference_rejection_button->maxValue = ARRAY_SIZE(target_separation_names) - 1;   // off, low, medium, high, same as target separation
    m_local_interference_rejection_button->names = target_separation_names;
    m_local_interference_rejection_button->SetLocalValue(m_ri->local_interference_rejection.button);

    // The SIDE LOBE SUPPRESSION button
    m_side_lobe_suppression_button = new br24RadarControlButton(this, ID_SIDE_LOBE_SUPPRESSION, _("Side lobe suppression"), CT_SIDE_LOBE_SUPPRESSION, true,
        m_ri->side_lobe_suppression.button);
    m_installation_sizer->Add(m_side_lobe_suppression_button, 0, wxALL, BORDER);
    m_side_lobe_suppression_button->minValue = 0;
    m_side_lobe_suppression_button->maxValue = 100;
    m_side_lobe_suppression_button->SetLocalValue(m_ri->side_lobe_suppression.button); // redraw after adding names

    m_top_sizer->Hide(m_installation_sizer);


    //**************** CONTROL BOX ******************//
    // These are the controls that the users sees when the dialog is started

    // A box sizer to contain RANGE, GAIN etc button
    m_control_sizer = new wxBoxSizer(wxVERTICAL);
    m_top_sizer->Add(m_control_sizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, BORDER);

    // The Transmit button
    m_radar_state = new wxButton(this, ID_RADAR_STATE, _("Off"), wxDefaultPosition, g_buttonSize, 0);
    m_control_sizer->Add(m_radar_state, 0, wxALL, BORDER);
    m_radar_state->SetFont(m_pi->m_font);
    // Updated when we receive data

    // The RADAR ONLY / OVERLAY button
    wxString overlay = _("Overlay");
    overlay << wxT("\n?");
    m_overlay_button = new wxButton(this, ID_RADAR_OVERLAY, overlay, wxDefaultPosition, g_buttonSize, 0);
    m_control_sizer->Add(m_overlay_button, 0, wxALL, BORDER);
    m_overlay_button->SetFont(m_pi->m_font);

    // The RANGE button
    m_range_button = new br24RadarRangeControlButton(this, ID_RANGE, _("Range"));
    m_control_sizer->Add(m_range_button, 0, wxALL, BORDER);

    // The GAIN button
    m_gain_button = new br24RadarControlButton(this, ID_GAIN, _("Gain"), CT_GAIN, true, m_ri->gain.button);
    m_control_sizer->Add(m_gain_button, 0, wxALL, BORDER);

    // The SEA button
    m_sea_button = new br24RadarControlButton(this, ID_SEA, _("Sea clutter"), CT_SEA, true, m_ri->sea.button);
    m_control_sizer->Add(m_sea_button, 0, wxALL, BORDER);

    // The RAIN button
    m_rain_button = new br24RadarControlButton(this, ID_RAIN, _("Rain clutter"), CT_RAIN, false, m_ri->rain.button);
    m_control_sizer->Add(m_rain_button, 0, wxALL, BORDER);

    // The ADVANCED button
    wxButton * bAdvanced = new wxButton(this, ID_ADVANCED, _("Advanced\ncontrols"), wxDefaultPosition, g_buttonSize, 0);
    m_control_sizer->Add(bAdvanced, 0, wxALL, BORDER);
    bAdvanced->SetFont(m_pi->m_font);

    // The GUARD ZONE 1 button
    m_guard_1_button = new wxButton(this, ID_ZONE1, wxT(""), wxDefaultPosition, g_buttonSize, 0);
    m_control_sizer->Add(m_guard_1_button, 0, wxALL, BORDER);
    m_guard_1_button->SetFont(m_pi->m_font);

    // The GUARD ZONE 2 button
    m_guard_2_button = new wxButton(this, ID_ZONE2, wxT(""), wxDefaultPosition, g_buttonSize, 0);
    m_control_sizer->Add(m_guard_2_button, 0, wxALL, BORDER);
    m_guard_2_button->SetFont(m_pi->m_font);

    // The INFO button
    wxButton * bMessage = new wxButton(this, ID_MESSAGE, _("Info"), wxDefaultPosition, wxSize(width, 25), 0);
    m_control_sizer->Add(bMessage, 0, wxALL, BORDER);
    bMessage->SetFont(m_pi->m_font);

    m_from_sizer = m_control_sizer;
    m_top_sizer->Hide(m_control_sizer);

    UpdateGuardZoneState();

    m_pi->UpdateDisplayParameters();
    DimeWindow(this); // Call OpenCPN to change colours depending on day/night mode
    Fit();
    //wxSize size_min = GetBestSize();
    //SetMinSize(size_min);
    //SetSize(size_min);
}

void br24ControlsDialog::UpdateGuardZoneState()
{
    wxString label1, label2;

    wxString GuardZoneNames[] = {
        _("Off"),
        _("Arc"),
        _("Circle")
    };

    label1 << _("Guard zone") << wxT(" 1\n") << GuardZoneNames[m_ri->guard_zone[0]->type];
    m_guard_1_button->SetLabel(label1);

    label2 << _("Guard zone") << wxT(" 2\n") << GuardZoneNames[m_ri->guard_zone[1]->type];
    m_guard_2_button->SetLabel(label2);
}

void br24ControlsDialog::SetRemoteRangeIndex(size_t index)
{
    m_range_button->isRemote = true;
    m_range_button->SetValueInt(index); // set and recompute the range label
}

void br24ControlsDialog::SetRangeIndex(size_t index)
{
    m_range_button->isRemote = false;
//    m_range_button->SetValueInt(index); // set and recompute the range label
}

void br24ControlsDialog::SetTimedIdleIndex(int index)
{
    m_timed_idle_button->SetValue(index) ; // set and recompute the Timed Idle label
    if (m_pi->m_pIdleDialog && index == 0) m_pi->m_pIdleDialog->Close();
}

void br24ControlsDialog::OnZone1ButtonClick(wxCommandEvent &event)
{
    m_pi->ShowGuardZoneDialog(m_ri->radar, 0);
}

void br24ControlsDialog::OnZone2ButtonClick(wxCommandEvent &event)
{
    m_pi->ShowGuardZoneDialog(m_ri->radar, 1);
}

void br24ControlsDialog::OnClose(wxCloseEvent& event)
{
    m_pi->OnControlDialogClose(m_ri);
}


void br24ControlsDialog::OnIdOKClick(wxCommandEvent& event)
{
    m_pi->OnControlDialogClose(m_ri);
}

void br24ControlsDialog::OnPlusTenClick(wxCommandEvent& event)
{
    m_from_control->SetValue(m_from_control->value + 10);
    wxString label = m_from_control->GetLabel();

    m_value_text->SetLabel(label);
}

void br24ControlsDialog::OnPlusClick(wxCommandEvent& event)
{
    m_from_control->SetValue(m_from_control->value + 1);
    wxString label = m_from_control->GetLabel();

    m_value_text->SetLabel(label);
}

void br24ControlsDialog::OnBackClick(wxCommandEvent &event)
{
    m_top_sizer->Hide(m_edit_sizer);
    m_top_sizer->Show(m_from_sizer);
    if (m_from_sizer == m_advanced_sizer) {
        if (m_ri->radar_type == RT_4G) {
            m_advanced_sizer->Show(m_advanced_4G_sizer);
        } else {
            m_advanced_sizer->Hide(m_advanced_4G_sizer);
        }
    }
    m_top_sizer->Layout();
}

void br24ControlsDialog::OnAutoClick(wxCommandEvent &event)
{
    m_from_control->SetAuto();
    OnBackClick(event);
}

void br24ControlsDialog::OnMultiSweepClick(wxCommandEvent &event)
{
    wxString labelSweep;
    if (m_ri->multi_sweep_filter == 0) {
        labelSweep << _("Multi Sweep Filter") << wxT("\n") << _("ON");
        m_ri->multi_sweep_filter = 1;
    } else {
        labelSweep << _("Multi Sweep Filter") << wxT("\n") << _("Off");
        m_ri->multi_sweep_filter = 0;
    }
    m_multi_sweep_button->SetLabel(labelSweep);
}

void br24ControlsDialog::OnMinusClick(wxCommandEvent& event)
{
    m_from_control->SetValue(m_from_control->value - 1);

    wxString label = m_from_control->GetLabel();
    m_value_text->SetLabel(label);
}

void br24ControlsDialog::OnMinusTenClick(wxCommandEvent& event)
{
    m_from_control->SetValue(m_from_control->value - 10);

    wxString label = m_from_control->GetLabel();
    m_value_text->SetLabel(label);
}

void br24ControlsDialog::OnAdvancedBackButtonClick(wxCommandEvent& event)
{
    m_from_sizer = m_control_sizer;
    m_top_sizer->Hide(m_advanced_sizer);
    m_top_sizer->Show(m_control_sizer);
    m_advanced_sizer->Layout();
    Fit();
    m_top_sizer->Layout();
}

void br24ControlsDialog::OnInstallationBackButtonClick(wxCommandEvent& event)
{
    m_from_sizer = m_advanced_sizer;
    m_top_sizer->Show(m_advanced_sizer);
    m_top_sizer->Hide(m_installation_sizer);
    if (m_ri->radar_type == RT_4G) {
        m_advanced_sizer->Show(m_advanced_4G_sizer);
    }
    else {
        m_advanced_sizer->Hide(m_advanced_4G_sizer);
    }
    m_advanced_sizer->Layout();
    Fit();
    m_top_sizer->Layout();
}

void br24ControlsDialog::OnAdvancedButtonClick(wxCommandEvent& event)
{
    m_from_sizer = m_advanced_sizer;
    m_top_sizer->Show(m_advanced_sizer);
    m_top_sizer->Hide(m_installation_sizer);
    if (m_ri->radar_type == RT_4G) {
        m_advanced_sizer->Show(m_advanced_4G_sizer);
    } else {
        m_advanced_sizer->Hide(m_advanced_4G_sizer);
    }
    m_advanced_sizer->Layout();
    m_top_sizer->Hide(m_control_sizer);
    m_control_sizer->Layout();
    Fit();
    m_top_sizer->Layout();
}

void br24ControlsDialog::OnInstallationButtonClick(wxCommandEvent& event)
{
    m_from_sizer = m_installation_sizer;
    m_top_sizer->Hide(m_advanced_sizer);
    m_top_sizer->Show(m_installation_sizer);
    m_advanced_sizer->Layout();
    m_top_sizer->Hide(m_control_sizer);
    m_control_sizer->Layout();
    Fit();
    m_top_sizer->Layout();
}

void br24ControlsDialog::OnMessageButtonClick(wxCommandEvent& event)
{
//    m_top_sizer->Hide(m_control_sizer);
    if (m_pi->m_pMessageBox) {
        m_pi->m_pMessageBox->Show();
    }
 //   Fit();
 //   m_top_sizer->Layout();
}

void br24ControlsDialog::EnterEditMode(br24RadarControlButton * button)
{
    m_from_control = button; // Keep a record of which button was clicked
    m_value_text->SetLabel(button->GetLabel());
    m_top_sizer->Hide(m_control_sizer);
    m_top_sizer->Hide(m_advanced_sizer);
    m_top_sizer->Hide(m_installation_sizer);
//    Fit();   //  solves the "partial refresh issue" for the control box
    m_top_sizer->Show(m_edit_sizer);
    if (m_from_control->hasAuto) {
        m_auto_button->Show();
    }
    else {
        m_auto_button->Hide();
    }
    if (m_from_control == m_gain_button) {
        m_multi_sweep_button->Show();
    }
    else{
        m_multi_sweep_button->Hide();
    }
    if (m_from_control->maxValue > 20) {
        m_plus_ten_button->Show();
        m_minus_ten_button->Show();
    }
    else {
        m_plus_ten_button->Hide();
        m_minus_ten_button->Hide();
    }
    m_edit_sizer->Layout();
    m_top_sizer->Layout();
}


void br24ControlsDialog::OnRadarControlButtonClick(wxCommandEvent& event)
{
    EnterEditMode((br24RadarControlButton *) event.GetEventObject());
}

void br24ControlsDialog::OnRadarOverlayButtonClick(wxCommandEvent& event)
{
    if (m_pi->m_settings.chart_overlay != m_ri->radar) {
        m_pi->m_settings.chart_overlay = m_ri->radar;
    }
    else {
        m_pi->m_settings.chart_overlay = -1;
    }
    m_ri->overlay.Update(m_pi->m_settings.chart_overlay == m_ri->radar);
    UpdateControlValues(true);
}

void br24ControlsDialog::OnRadarGainButtonClick(wxCommandEvent& event)
{
    EnterEditMode((br24RadarControlButton *) event.GetEventObject());
}

void br24ControlsDialog::OnRadarStateButtonClick(wxCommandEvent& event)
{
    UpdateControlValues(true);   // update control values on the buttons
}

void br24ControlsDialog::OnMove(wxMoveEvent& event)
{
    event.Skip();
}

void br24ControlsDialog::OnSize(wxSizeEvent& event)
{
    event.Skip();
}

void br24ControlsDialog::UpdateControlValues(bool refreshAll)
{
    if (m_ri->state.mod || refreshAll) {
        wxString o = m_ri->state.button ? _("On") : _("Off");
        m_radar_state->SetLabel(o);
        m_ri->overlay.mod = false;
    }

    if (m_ri->overlay.mod || ((m_pi->m_settings.chart_overlay == m_ri->radar) != (m_ri->overlay.button != 0)) || refreshAll) {
        wxString o = _("Overlay");
        o << wxT("\n");
        if (m_ri->overlay.button > 0) {
            o << _("On");
        } else {
            o << _("Off");
        }
        m_overlay_button->SetLabel(o);
        m_ri->overlay.mod = false;
    }

    // first update the range
    if (m_ri->range.mod || refreshAll) {
        if (m_ri->range.button == -1) {
            m_range_button->SetLocalAuto();
        }
        SetRangeIndex(m_ri->range.button);
        m_ri->range.mod = false;
    }  // don't set the actual range here, is still handled elsewhere

    // gain
    if (m_ri->gain.mod || refreshAll) {
        if (m_ri->gain.button == -1) {
            m_gain_button->SetLocalAuto();
        }
        else{
            m_gain_button->SetLocalValue(m_ri->gain.button);
        }
    }

    //  rain
    if (m_ri->rain.mod || refreshAll) {
        m_rain_button->SetLocalValue(m_ri->rain.button);
        m_ri->rain.mod = false;
    }

    //   sea
    if (m_ri->sea.mod || refreshAll) {
        if (m_ri->sea.button == -1) {
            m_sea_button->SetLocalAuto();
        }
        else{
            m_sea_button->SetLocalValue(m_ri->sea.button);
        }
        m_ri->sea.mod = false;
    }

    //   target_boost
    if (m_ri->target_boost.mod || refreshAll) {
        m_target_boost_button->SetLocalValue(m_ri->target_boost.button);
        m_ri->target_boost.mod = false;
    }

    //  noise_rejection
    if (m_ri->noise_rejection.mod || refreshAll) {
        m_noise_rejection_button->SetLocalValue(m_ri->noise_rejection.button);
        m_ri->noise_rejection.mod = false;
    }

    //  target_separation
    if (m_ri->target_separation.mod || refreshAll) {
        m_target_separation_button->SetLocalValue(m_ri->target_separation.button);
        m_ri->target_separation.mod = false;
    }

    //  interference_rejection
    if (m_ri->interference_rejection.mod || refreshAll) {
        m_interference_rejection_button->SetLocalValue(m_ri->interference_rejection.button);
        m_ri->interference_rejection.mod = false;
    }

    // scanspeed
    if (m_ri->scan_speed.mod || refreshAll) {
        m_scan_speed_button->SetLocalValue(m_ri->scan_speed.button);
        m_ri->scan_speed.mod = false;
    }

    //   antenna height
    if (m_ri->antenna_height.mod || refreshAll) {
        m_antenna_height_button->SetLocalValue(m_ri->antenna_height.button);
        m_ri->antenna_height.mod = false;
    }

    //  bearing alignment
    if (m_ri->bearing_alignment.mod || refreshAll) {
        m_bearing_alignment_button->SetLocalValue(m_ri->bearing_alignment.button);
        m_ri->bearing_alignment.mod = false;
    }

    //  local interference rejection
    if (m_ri->local_interference_rejection.mod || refreshAll) {
        m_local_interference_rejection_button->SetLocalValue(m_ri->local_interference_rejection.button);
        m_ri->local_interference_rejection.mod = false;
    }

    // side lobe suppression  // same for A and B
    if (m_ri->side_lobe_suppression.mod || refreshAll) {
        if (m_ri->side_lobe_suppression.button == -1) {
            m_side_lobe_suppression_button->SetLocalAuto();
        }
        else{
            m_side_lobe_suppression_button->SetLocalValue(m_ri->side_lobe_suppression.button);
        }
        m_ri->side_lobe_suppression.mod = false;
    }

}

void br24ControlsDialog::UpdateControl(bool haveOpenGL, bool haveGPS, bool haveHeading, bool haveVariation, bool haveRadar, bool haveData)
{
    if (m_ri->control_box_closed) {
        if (m_pi->m_settings.verbose) {
            wxLogMessage(wxT("br24radar_pi: %s ControlsDialog::UpdateControl explicit closed: Hidden"), m_ri->name);
        }
        Hide();
        return;
    }
    if (m_pi->m_pGuardZoneDialog && m_pi->m_pGuardZoneDialog->IsShown()) {
        if (m_pi->m_settings.verbose) {
            wxLogMessage(wxT("br24radar_pi: %s ControlsDialog::UpdateControl Hidden because GuardZoneDialog is shown"), m_ri->name);
        }
        Hide();
    }

    if (m_ri->control_box_opened) {  // manually opened from context menu
        if (m_pi->m_settings.verbose) {
            wxLogMessage(wxT("br24radar_pi: %s ControlsDialog::UpdateControl manually opened"), m_ri->name);
        }
        if (!m_top_sizer->IsShown(m_control_sizer) && !m_top_sizer->IsShown(m_advanced_sizer) && !m_top_sizer->IsShown(m_edit_sizer) && !m_top_sizer->IsShown(m_installation_sizer)) {
            m_top_sizer->Show(m_control_sizer);
        }
        m_control_sizer->Layout();
        Fit();
        m_top_sizer->Layout();
        Show();
    }

    m_edit_sizer->Layout();
    m_top_sizer->Layout();

    wxString title;

    title << m_ri->name << wxT(" - ");

    if (m_ri->data_seen) {
        title << _("On");
    } else if (m_ri->radar_seen) {
        title << _("Standby");
    } else {
        title << _("Off");
    }

    SetTitle(title);
}

// vim: sw=4:ts=8:
