/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2017-2018 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <dialog_shim.h>

class GAL_OPTIONS_PANEL;
class FOOTPRINT_EDIT_FRAME;
class STEPPED_SLIDER;
class wxCheckBox;

class DIALOG_MODEDIT_DISPLAY_OPTIONS : public DIALOG_SHIM
{
public:
    DIALOG_MODEDIT_DISPLAY_OPTIONS( FOOTPRINT_EDIT_FRAME& aParent );

    static bool Invoke( FOOTPRINT_EDIT_FRAME& aCaller );

protected:
    void OnScaleSlider( wxScrollEvent& aEvent );
    void OnScaleAuto( wxCommandEvent& aEvent );

private:

    bool TransferDataToWindow() override;
    bool TransferDataFromWindow() override;

    FOOTPRINT_EDIT_FRAME& m_parent;

    // subpanel
    GAL_OPTIONS_PANEL* m_galOptsPanel;

    int m_last_scale;
    wxCheckBox* m_scaleAuto;
    STEPPED_SLIDER* m_scaleSlider;
};
