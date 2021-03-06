/**
 * @file dialog_pad_properties.cpp
 * @brief dialog pad properties editor.
 */

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2018 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2013 Dick Hollenbeck, dick@softplc.com
 * Copyright (C) 2008-2013 Wayne Stambaugh <stambaughw@verizon.net>
 * Copyright (C) 1992-2018 KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <fctsys.h>
#include <common.h>
#include <gr_basic.h>
#include <gal/graphics_abstraction_layer.h>
#include <trigo.h>
#include <class_drawpanel.h>
#include <confirm.h>
#include <pcbnew.h>
#include <pcb_base_frame.h>
#include <base_units.h>
#include <unit_format.h>
#include <board_commit.h>

#include <class_board.h>
#include <class_module.h>
#include <pcb_painter.h>

#include <dialog_pad_properties.h>
#include <html_messagebox.h>


// list of pad shapes, ordered like the pad shape wxChoice in dialog.
static PAD_SHAPE_T code_shape[] = {
    PAD_SHAPE_CIRCLE,
    PAD_SHAPE_OVAL,
    PAD_SHAPE_RECT,
    PAD_SHAPE_TRAPEZOID,
    PAD_SHAPE_ROUNDRECT,
    PAD_SHAPE_CUSTOM,      // choice = CHOICE_SHAPE_CUSTOM_CIRC_ANCHOR
    PAD_SHAPE_CUSTOM       // choice = PAD_SHAPE_CUSTOM_RECT_ANCHOR
};

// the ordered index of the pad shape wxChoice in dialog.
// keep it consistent with code_shape[] and dialog strings
enum CODE_CHOICE {
    CHOICE_SHAPE_CIRCLE = 0,
    CHOICE_SHAPE_OVAL,
    CHOICE_SHAPE_RECT,
    CHOICE_SHAPE_TRAPEZOID,
    CHOICE_SHAPE_ROUNDRECT,
    CHOICE_SHAPE_CUSTOM_CIRC_ANCHOR,
    CHOICE_SHAPE_CUSTOM_RECT_ANCHOR
};



static PAD_ATTR_T code_type[] = {
    PAD_ATTRIB_STANDARD,
    PAD_ATTRIB_SMD,
    PAD_ATTRIB_CONN,
    PAD_ATTRIB_HOLE_NOT_PLATED
};


// Default mask layers setup for pads according to the pad type
static const LSET std_pad_layers[] = {
    // PAD_ATTRIB_STANDARD:
    D_PAD::StandardMask(),

    // PAD_ATTRIB_SMD:
    D_PAD::SMDMask(),

    // PAD_ATTRIB_CONN:
    D_PAD::ConnSMDMask(),

    // PAD_ATTRIB_HOLE_NOT_PLATED:
    D_PAD::UnplatedHoleMask()
};


void PCB_BASE_FRAME::InstallPadOptionsFrame( D_PAD* aPad )
{
    DIALOG_PAD_PROPERTIES dlg( this, aPad );
    dlg.ShowModal();
}


DIALOG_PAD_PROPERTIES::DIALOG_PAD_PROPERTIES( PCB_BASE_FRAME* aParent, D_PAD* aPad ) :
    DIALOG_PAD_PROPERTIES_BASE( aParent ),
    m_OrientValidator( 1, &m_OrientValue )
{
    m_canUpdate  = false;
    m_parent     = aParent;
    m_currentPad = aPad;        // aPad can be NULL, if the dialog is called
                                // from the footprint editor to set default pad setup

    m_board      = m_parent->GetBoard();

    m_OrientValidator.SetRange( -360.0, 360.0 );
    m_PadOrientCtrl->SetValidator( m_OrientValidator );
    m_OrientValidator.SetWindow( m_PadOrientCtrl );

    m_cbShowPadOutline->SetValue( m_drawPadOutlineMode );

    m_padMaster  = &m_parent->GetDesignSettings().m_Pad_Master;
    m_dummyPad   = new D_PAD( (MODULE*) NULL );

    if( aPad )
        *m_dummyPad = *aPad;
    else    // We are editing a "master" pad, i.e. a template to create new pads
        *m_dummyPad = *m_padMaster;

    initValues();

    // Usually, TransferDataToWindow is called by OnInitDialog
    // calling it here fixes all widgets sizes, and FinishDialogSettings can
    // safely fix minsizes
    TransferDataToWindow();

    // Initialize canvas to be able to display the dummy pad:
    prepareCanvas();

    m_sdbSizerOK->SetDefault();
    m_canUpdate = true;

    // Now all widgets have the size fixed, call FinishDialogSettings
    FinishDialogSettings();
}

bool DIALOG_PAD_PROPERTIES::m_drawPadOutlineMode = false;   // Stores the pad draw option during a session


void DIALOG_PAD_PROPERTIES::OnInitDialog( wxInitDialogEvent& event )
{
    m_PadNumCtrl->SetFocus();
    m_PadNumCtrl->SetSelection( -1, -1 );
    m_selectedColor = COLOR4D( 1.0, 1.0, 1.0, 0.7 );

    // Needed on some WM to be sure the pad is redrawn according to the final size
    // of the canvas, with the right zoom factor
    redraw();
}


void DIALOG_PAD_PROPERTIES::OnCancel( wxCommandEvent& event )
{
    // Mandatory to avoid m_panelShowPadGal trying to draw something
    // in a non valid context during closing process:
    if( m_parent->IsGalCanvasActive() )
        m_panelShowPadGal->StopDrawing();

    // Now call default handler for wxID_CANCEL command event
    event.Skip();
}


void DIALOG_PAD_PROPERTIES::enablePrimitivePage( bool aEnable )
{
    // Enable or disable the widgets in page managing custom shape primitives
	m_listCtrlPrimitives->Enable( aEnable );
	m_buttonDel->Enable( aEnable );
	m_buttonEditShape->Enable( aEnable );
	m_buttonAddShape->Enable( aEnable );
	m_buttonDup->Enable( aEnable );
	m_buttonGeometry->Enable( aEnable );
	m_buttonImport->Enable( aEnable );
}


void DIALOG_PAD_PROPERTIES::prepareCanvas()
{
    // Initialize the canvases (legacy or gal) to display the pad
    // Enable the suitable canvas and make some inits

    // Show the X and Y axis. It is usefull because pad shape can have an offset
    // or be a complex shape.
    KIGFX::COLOR4D axis_color = LIGHTBLUE;

    m_axisOrigin = new KIGFX::ORIGIN_VIEWITEM( axis_color,
                                               KIGFX::ORIGIN_VIEWITEM::CROSS,
                                               Millimeter2iu( 0.2 ),
                                               VECTOR2D( m_dummyPad->GetPosition() ) );
    m_axisOrigin->SetDrawAtZero( true );

    if( m_parent->IsGalCanvasActive() )
    {
        m_panelShowPadGal->UseColorScheme( &m_parent->Settings().Colors() );
        m_panelShowPadGal->SwitchBackend( m_parent->GetGalCanvas()->GetBackend() );

        m_panelShowPadGal->Show();
        m_panelShowPad->Hide();

        KIGFX::VIEW* view = m_panelShowPadGal->GetView();

        // fix the pad render mode (filled/not filled)
        KIGFX::PCB_RENDER_SETTINGS* settings =
            static_cast<KIGFX::PCB_RENDER_SETTINGS*>( view->GetPainter()->GetSettings() );
        bool filled = !m_cbShowPadOutline->IsChecked();
        settings->SetSketchMode( LAYER_PADS_TH, !filled );
        settings->SetSketchMode( LAYER_PAD_FR, !filled );
        settings->SetSketchMode( LAYER_PAD_BK, !filled );
        settings->SetSketchModeGraphicItems( !filled );

        // gives a non null grid size (0.001mm) because GAL layer does not like a 0 size grid:
        double gridsize = 0.001 * IU_PER_MM;
        view->GetGAL()->SetGridSize( VECTOR2D( gridsize, gridsize ) );
        view->Add( m_dummyPad );
        view->Add( m_axisOrigin );

        m_panelShowPadGal->StartDrawing();
        Connect( wxEVT_SIZE, wxSizeEventHandler( DIALOG_PAD_PROPERTIES::OnResize ) );
    }
    else
    {
        m_panelShowPad->Show();
        m_panelShowPadGal->Hide();
    }
}


void DIALOG_PAD_PROPERTIES::OnPaintShowPanel( wxPaintEvent& event )
{
    wxPaintDC    dc( m_panelShowPad );
    PAD_DRAWINFO drawInfo;

    COLOR4D color = COLOR4D::BLACK;

    if( m_dummyPad->GetLayerSet()[F_Cu] )
    {
        color = m_parent->Settings().Colors().GetItemColor( LAYER_PAD_FR );
    }

    if( m_dummyPad->GetLayerSet()[B_Cu] )
    {
        color = color.LegacyMix( m_parent->Settings().Colors().GetItemColor( LAYER_PAD_BK ) );
    }

    // What could happen: the pad color is *actually* black, or no
    // copper was selected
    if( color == BLACK )
        color = LIGHTGRAY;

    drawInfo.m_Color     = color;
    drawInfo.m_HoleColor = DARKGRAY;
    drawInfo.m_Offset    = m_dummyPad->GetPosition();
    drawInfo.m_Display_padnum  = true;
    drawInfo.m_Display_netname = true;
    drawInfo.m_ShowPadFilled = !m_drawPadOutlineMode;

    if( m_dummyPad->GetAttribute() == PAD_ATTRIB_HOLE_NOT_PLATED )
        drawInfo.m_ShowNotPlatedHole = true;

    // Shows the local pad clearance
    drawInfo.m_PadClearance = m_dummyPad->GetLocalClearance();

    wxSize dc_size = dc.GetSize();
    dc.SetDeviceOrigin( dc_size.x / 2, dc_size.y / 2 );

    // Calculate a suitable scale to fit the available draw area
    int dim = m_dummyPad->GetBoundingRadius() *2;

    // Invalid x size. User could enter zero, or have deleted all text prior to
    // entering a new value; this is also treated as zero. If dim is left at
    // zero, the drawing scale is zero and we get a crash.
    if( dim == 0 )
    {
        // If drill size has been set, use that. Otherwise default to 1mm.
        dim = m_dummyPad->GetDrillSize().x;
        if( dim == 0 )
            dim = Millimeter2iu( 1.0 );
    }

    if( m_dummyPad->GetLocalClearance() > 0 )
        dim += m_dummyPad->GetLocalClearance() * 2;

    double scale = (double) dc_size.x / dim;

    // If the pad is a circle, use the x size here instead.
    int ysize;

    if( m_dummyPad->GetShape() == PAD_SHAPE_CIRCLE )
        ysize = m_dummyPad->GetSize().x;
    else
        ysize = m_dummyPad->GetSize().y;

    dim = ysize + std::abs( m_dummyPad->GetDelta().x );

    // Invalid y size. See note about x size above.
    if( dim == 0 )
    {
        dim = m_dummyPad->GetDrillSize().y;
        if( dim == 0 )
            dim = Millimeter2iu( 0.1 );
    }

    if( m_dummyPad->GetLocalClearance() > 0 )
        dim += m_dummyPad->GetLocalClearance() * 2;

    double altscale = (double) dc_size.y / dim;
    scale = std::min( scale, altscale );

    // Give a margin
    scale *= 0.7;
    dc.SetUserScale( scale, scale );

    GRResetPenAndBrush( &dc );
    m_dummyPad->DrawShape( NULL, &dc, drawInfo );

    // draw selected primitives:
    long select = m_listCtrlPrimitives->GetFirstSelected();
    wxPoint start, end, center;

    while( select >= 0 )
    {
        PAD_CS_PRIMITIVE& primitive = m_primitives[select];

        // The best way to calculate parameters to draw a primitive is to
        // use a dummy DRAWSEGMENT and use its methods
        // Note: in legacy canvas, the pad has the 0,0 position
        DRAWSEGMENT dummySegment;
        primitive.ExportTo( &dummySegment );
        dummySegment.Rotate( wxPoint( 0, 0), m_dummyPad->GetOrientation() );

        switch( primitive.m_Shape )
        {
        case S_SEGMENT:         // usual segment : line with rounded ends
            if( !m_drawPadOutlineMode )
                GRFilledSegment( NULL, &dc, dummySegment.GetStart(), dummySegment.GetEnd(),
                             primitive.m_Thickness, m_selectedColor );
            else
                GRCSegm( NULL, &dc, dummySegment.GetStart(), dummySegment.GetEnd(),
                         primitive.m_Thickness, m_selectedColor );
            break;

        case S_ARC:             // Arc with rounded ends
            if( !m_drawPadOutlineMode )
                GRArc1( NULL, &dc, dummySegment.GetArcEnd(), dummySegment.GetArcStart(),
                        dummySegment.GetCenter(), primitive.m_Thickness, m_selectedColor );
            else
            {
                GRArc1( NULL, &dc, dummySegment.GetArcEnd(), dummySegment.GetArcStart(),
                        dummySegment.GetCenter(), 0, m_selectedColor );
/*                GRArc1( NULL, &dc, dummySegment.GetArcEnd(), dummySegment.GetArcStart(),
                        dummySegment.GetCenter() - primitive.m_Thickness, 0, m_selectedColor );*/
             }
            break;

        case S_CIRCLE:          //  ring or circle
            if( primitive.m_Thickness )
            {
                if( !m_drawPadOutlineMode )
                    GRCircle( NULL, &dc, dummySegment.GetCenter(), primitive.m_Radius,
                              primitive.m_Thickness, m_selectedColor );
                else
                {
                    GRCircle( NULL, &dc, dummySegment.GetCenter(),
                              primitive.m_Radius + primitive.m_Thickness/2, 0,
                              m_selectedColor );
                    GRCircle( NULL, &dc, dummySegment.GetCenter(),
                              primitive.m_Radius - primitive.m_Thickness/2, 0,
                              m_selectedColor );
                }
            }
            else
            {
                if( !m_drawPadOutlineMode )
                    GRFilledCircle( NULL, &dc, dummySegment.GetCenter(),
                                    primitive.m_Radius, m_selectedColor );
                else
                    GRCircle( NULL, &dc, dummySegment.GetCenter(),
                              primitive.m_Radius, 0, m_selectedColor );
            }
            break;

        case S_POLYGON:         // polygon
        {
            std::vector<wxPoint> poly = dummySegment.BuildPolyPointsList();
            GRClosedPoly( NULL, &dc, poly.size(), &poly[0],
                          m_drawPadOutlineMode ? false : true,
                          primitive.m_Thickness, m_selectedColor, m_selectedColor );
        }
            break;

        default:
            break;
        }

        select = m_listCtrlPrimitives->GetNextSelected( select );
    }

    // Draw X and Y axis. This is particularly useful to show the
    // reference position of pads with offset and no hole, or custom pad shapes
    const int linethickness = 0;
    GRLine( NULL, &dc, -int( dc_size.x/scale ), 0, int( dc_size.x/scale ), 0,
            linethickness, LIGHTBLUE );   // X axis
    GRLine( NULL, &dc, 0, -int( dc_size.y/scale ), 0, int( dc_size.y/scale ),
            linethickness, LIGHTBLUE );   // Y axis

    event.Skip();
}


void DIALOG_PAD_PROPERTIES::updateRoundRectCornerValues()
{
    // Note:
    // To avoid generating a wxEVT_TEXT event from m_tcCornerSizeRatio
    // we use ChangeValue instead of SetValue, to set the displayed string
    if( m_dummyPad->GetShape() == PAD_SHAPE_ROUNDRECT )
    {
        m_tcCornerSizeRatio->ChangeValue( wxString::Format( "%.1f",
                                        m_dummyPad->GetRoundRectRadiusRatio()*100 ) );
        m_staticTextCornerRadiusValue->SetLabel( StringFromValue( g_UserUnit,
                                                 m_dummyPad->GetRoundRectCornerRadius() ) );
    }
    else if( m_dummyPad->GetShape() == PAD_SHAPE_RECT )
    {
        m_tcCornerSizeRatio->ChangeValue( "0" );
        m_staticTextCornerRadiusValue->SetLabel( "0" );
    }
    else
    {
        m_tcCornerSizeRatio->ChangeValue( wxEmptyString );
        m_staticTextCornerRadiusValue->SetLabel( wxEmptyString );
    }
}


void DIALOG_PAD_PROPERTIES::onCornerSizePercentChange( wxCommandEvent& event )
{
    if( m_dummyPad->GetShape() != PAD_SHAPE_ROUNDRECT )
        return;

    wxString value = m_tcCornerSizeRatio->GetValue();
    double rrRadiusRatioPercent;

    if( value.ToDouble( &rrRadiusRatioPercent ) )
    {
        // Clamp rrRadiusRatioPercent to acceptable value (0.0 to 50.0)
        if( rrRadiusRatioPercent < 0.0 )
        {
            rrRadiusRatioPercent = 0.0;
            m_tcCornerSizeRatio->ChangeValue( "0.0" );
        }

        if( rrRadiusRatioPercent > 50.0 )
        {
            rrRadiusRatioPercent = 0.5;
            m_tcCornerSizeRatio->ChangeValue( "50.0" );
        }

        transferDataToPad( m_dummyPad );
        m_staticTextCornerRadiusValue->SetLabel( StringFromValue( g_UserUnit,
                                                 m_dummyPad->GetRoundRectCornerRadius() ) );
        redraw();
    }
}


void DIALOG_PAD_PROPERTIES::initValues()
{
    wxString    msg;
    double      angle;

    // Disable pad net name wxTextCtrl if the caller is the footprint editor
    // because nets are living only in the board managed by the board editor
    m_canEditNetName = m_parent->IsType( FRAME_PCB );


    // Setup layers names from board
    // Should be made first, before calling m_rbCopperLayersSel->SetSelection()
    m_rbCopperLayersSel->SetString( 0, m_board->GetLayerName( F_Cu ) );
    m_rbCopperLayersSel->SetString( 1, m_board->GetLayerName( B_Cu ) );

    m_PadLayerAdhCmp->SetLabel( m_board->GetLayerName( F_Adhes ) );
    m_PadLayerAdhCu->SetLabel( m_board->GetLayerName( B_Adhes ) );
    m_PadLayerPateCmp->SetLabel( m_board->GetLayerName( F_Paste ) );
    m_PadLayerPateCu->SetLabel( m_board->GetLayerName( B_Paste ) );
    m_PadLayerSilkCmp->SetLabel( m_board->GetLayerName( F_SilkS ) );
    m_PadLayerSilkCu->SetLabel( m_board->GetLayerName( B_SilkS ) );
    m_PadLayerMaskCmp->SetLabel( m_board->GetLayerName( F_Mask ) );
    m_PadLayerMaskCu->SetLabel( m_board->GetLayerName( B_Mask ) );
    m_PadLayerECO1->SetLabel( m_board->GetLayerName( Eco1_User ) );
    m_PadLayerECO2->SetLabel( m_board->GetLayerName( Eco2_User ) );
    m_PadLayerDraft->SetLabel( m_board->GetLayerName( Dwgs_User ) );

    m_isFlipped = false;

    if( m_currentPad )
    {
        m_isFlipped = m_currentPad->IsFlipped();

        if( m_isFlipped )
            m_staticModuleSideValue->SetLabel( _( "Back side (footprint is mirrored)" ) );

        // Diplay footprint rotation ( angles are in 0.1 degree )
        MODULE* footprint = m_currentPad->GetParent();

        if( footprint )
            msg.Printf( "%.1f", footprint->GetOrientationDegrees() );
        else
            msg = _("No footprint" );

        m_staticModuleRotValue->SetLabel( msg );
    }

    if( m_isFlipped )
    {
        wxPoint pt = m_dummyPad->GetOffset();
        pt.y = -pt.y;
        m_dummyPad->SetOffset( pt );

        wxSize sz = m_dummyPad->GetDelta();
        sz.y = -sz.y;
        m_dummyPad->SetDelta( sz );

        // flip pad's layers
        m_dummyPad->SetLayerSet( FlipLayerMask( m_dummyPad->GetLayerSet() ) );

        // flip custom pad shapes
        m_dummyPad->FlipPrimitives();
    }

    m_primitives = m_dummyPad->GetPrimitives();

    m_staticTextWarningPadFlipped->Show(m_isFlipped);

    m_PadNumCtrl->SetValue( m_dummyPad->GetName() );
    m_PadNetNameCtrl->SetValue( m_dummyPad->GetNetname() );

    // Set the unit name in dialog:
    wxStaticText* unitTexts[] =
    {
        m_PadPosX_Unit, m_PadPosY_Unit,
        m_PadDrill_X_Unit,  m_PadDrill_Y_Unit,
        m_PadShapeSizeX_Unit, m_PadShapeSizeY_Unit,
        m_PadShapeOffsetX_Unit,m_PadShapeOffsetY_Unit,
        m_PadShapeDelta_Unit, m_PadLengthDie_Unit,
        m_NetClearanceUnits, m_SolderMaskMarginUnits, m_SolderPasteMarginUnits,
        m_ThermalWidthUnits, m_ThermalGapUnits, m_staticTextCornerSizeUnit
    };

    for( unsigned ii = 0; ii < DIM( unitTexts ); ++ii )
        unitTexts[ii]->SetLabel( GetAbbreviatedUnitsLabel( g_UserUnit ) );

    // Display current pad parameters units:
    PutValueInLocalUnits( *m_PadPosition_X_Ctrl, m_dummyPad->GetPosition().x );
    PutValueInLocalUnits( *m_PadPosition_Y_Ctrl, m_dummyPad->GetPosition().y );

    PutValueInLocalUnits( *m_PadDrill_X_Ctrl, m_dummyPad->GetDrillSize().x );
    PutValueInLocalUnits( *m_PadDrill_Y_Ctrl, m_dummyPad->GetDrillSize().y );

    PutValueInLocalUnits( *m_ShapeSize_X_Ctrl, m_dummyPad->GetSize().x );
    PutValueInLocalUnits( *m_ShapeSize_Y_Ctrl, m_dummyPad->GetSize().y );

    PutValueInLocalUnits( *m_ShapeOffset_X_Ctrl, m_dummyPad->GetOffset().x );
    PutValueInLocalUnits( *m_ShapeOffset_Y_Ctrl, m_dummyPad->GetOffset().y );

    if( m_dummyPad->GetDelta().x )
    {
        PutValueInLocalUnits( *m_ShapeDelta_Ctrl, m_dummyPad->GetDelta().x );
        m_trapDeltaDirChoice->SetSelection( 0 );
    }
    else
    {
        PutValueInLocalUnits( *m_ShapeDelta_Ctrl, m_dummyPad->GetDelta().y );
        m_trapDeltaDirChoice->SetSelection( 1 );
    }

    PutValueInLocalUnits( *m_LengthPadToDieCtrl, m_dummyPad->GetPadToDieLength() );

    PutValueInLocalUnits( *m_NetClearanceValueCtrl, m_dummyPad->GetLocalClearance() );
    PutValueInLocalUnits( *m_SolderMaskMarginCtrl, m_dummyPad->GetLocalSolderMaskMargin() );
    PutValueInLocalUnits( *m_ThermalWidthCtrl, m_dummyPad->GetThermalWidth() );
    PutValueInLocalUnits( *m_ThermalGapCtrl, m_dummyPad->GetThermalGap() );

    // These 2 parameters are usually < 0, so prepare entering a negative value, if current is 0
    PutValueInLocalUnits( *m_SolderPasteMarginCtrl, m_dummyPad->GetLocalSolderPasteMargin() );

    if( m_dummyPad->GetLocalSolderPasteMargin() == 0 )
        m_SolderPasteMarginCtrl->SetValue( wxT( "-" ) + m_SolderPasteMarginCtrl->GetValue() );

    msg.Printf( wxT( "%f" ), m_dummyPad->GetLocalSolderPasteMarginRatio() * 100.0 );

    if( m_dummyPad->GetLocalSolderPasteMarginRatio() == 0.0 && msg[0] == '0' )
        // Sometimes Printf adds a sign if the value is small
        m_SolderPasteMarginRatioCtrl->SetValue( wxT( "-" ) + msg );
    else
        m_SolderPasteMarginRatioCtrl->SetValue( msg );

    switch( m_dummyPad->GetZoneConnection() )
    {
    default:
    case PAD_ZONE_CONN_INHERITED:
        m_ZoneConnectionChoice->SetSelection( 0 );
        break;

    case PAD_ZONE_CONN_FULL:
        m_ZoneConnectionChoice->SetSelection( 1 );
        break;

    case PAD_ZONE_CONN_THERMAL:
        m_ZoneConnectionChoice->SetSelection( 2 );
        break;

    case PAD_ZONE_CONN_NONE:
        m_ZoneConnectionChoice->SetSelection( 3 );
        break;
    }

    if( m_dummyPad->GetCustomShapeInZoneOpt() == CUST_PAD_SHAPE_IN_ZONE_CONVEXHULL )
        m_ZoneCustomPadShape->SetSelection( 1 );
    else
        m_ZoneCustomPadShape->SetSelection( 0 );

    if( m_currentPad )
    {
        angle = m_currentPad->GetOrientation();
        MODULE* footprint = m_currentPad->GetParent();

        if( footprint )
            angle -= footprint->GetOrientation();

        if( m_isFlipped )
            angle = -angle;

        m_dummyPad->SetOrientation( angle );
    }

    angle = m_dummyPad->GetOrientation();

    NORMALIZE_ANGLE_180( angle );    // ? normalizing is in D_PAD::SetOrientation()

    // Set layers used by this pad: :
    setPadLayersList( m_dummyPad->GetLayerSet() );

    // Pad Orient
    switch( int( angle ) )
    {
    case 0:
        m_PadOrient->SetSelection( 0 );
        break;

    case 900:
        m_PadOrient->SetSelection( 1 );
        break;

    case -900:
        m_PadOrient->SetSelection( 2 );
        break;

    case 1800:
    case -1800:
        m_PadOrient->SetSelection( 3 );
        break;

    default:
        m_PadOrient->SetSelection( 4 );
        break;
    }

    switch( m_dummyPad->GetShape() )
    {
    default:
    case PAD_SHAPE_CIRCLE:
        m_PadShape->SetSelection( CHOICE_SHAPE_CIRCLE );
        break;

    case PAD_SHAPE_OVAL:
        m_PadShape->SetSelection( CHOICE_SHAPE_OVAL );
        break;

    case PAD_SHAPE_RECT:
        m_PadShape->SetSelection( CHOICE_SHAPE_RECT );
        break;

    case PAD_SHAPE_TRAPEZOID:
        m_PadShape->SetSelection( CHOICE_SHAPE_TRAPEZOID );
        break;

    case PAD_SHAPE_ROUNDRECT:
        m_PadShape->SetSelection( CHOICE_SHAPE_ROUNDRECT );
        break;

    case PAD_SHAPE_CUSTOM:
        if( m_dummyPad->GetAnchorPadShape() == PAD_SHAPE_RECT )
            m_PadShape->SetSelection( CHOICE_SHAPE_CUSTOM_RECT_ANCHOR );
        else
            m_PadShape->SetSelection( CHOICE_SHAPE_CUSTOM_CIRC_ANCHOR );
        break;
    }

    enablePrimitivePage( PAD_SHAPE_CUSTOM == m_dummyPad->GetShape() );

    m_OrientValue = angle / 10.0;

    // Type of pad selection
    m_PadType->SetSelection( 0 );

    for( unsigned ii = 0; ii < DIM( code_type ); ii++ )
    {
        if( code_type[ii] == m_dummyPad->GetAttribute() )
        {
            m_PadType->SetSelection( ii );
            break;
        }
    }

    // Enable/disable Pad name,and pad length die
    // (disable for NPTH pads (mechanical pads)
    bool enable = m_dummyPad->GetAttribute() != PAD_ATTRIB_HOLE_NOT_PLATED;

    m_PadNumCtrl->Enable( enable );
    m_PadNetNameCtrl->Enable( m_canEditNetName && enable && m_currentPad != NULL );
    m_LengthPadToDieCtrl->Enable( enable );

    if( m_dummyPad->GetDrillShape() != PAD_DRILL_SHAPE_OBLONG )
        m_DrillShapeCtrl->SetSelection( 0 );
    else
        m_DrillShapeCtrl->SetSelection( 1 );

    // Update some dialog widgets state (Enable/disable options):
    wxCommandEvent cmd_event;
    setPadLayersList( m_dummyPad->GetLayerSet() );
    OnDrillShapeSelected( cmd_event );
    OnPadShapeSelection( cmd_event );
    updateRoundRectCornerValues();

    // Update basic shapes list
    displayPrimitivesList();
}

// A small helper function, to display coordinates:
static wxString formatCoord( wxPoint aCoord )
{
    return wxString::Format( "(X:%s Y:%s)",
                CoordinateToString( aCoord.x, true ),
                CoordinateToString( aCoord.y, true ) );
}

void DIALOG_PAD_PROPERTIES::displayPrimitivesList()
{
    m_listCtrlPrimitives->ClearAll();

    wxListItem itemCol;
    itemCol.SetImage(-1);

    for( int ii = 0; ii < 5; ++ii )
        m_listCtrlPrimitives->InsertColumn(ii, itemCol);

    wxString bs_info[5];

    for( unsigned ii = 0; ii < m_primitives.size(); ++ii )
    {
        const PAD_CS_PRIMITIVE& primitive = m_primitives[ii];

        for( unsigned jj = 0; jj < 5; ++jj )
            bs_info[jj].Empty();

        bs_info[4] = wxString::Format( _( "width %s" ),
                                       CoordinateToString( primitive.m_Thickness, true ) );

        switch( primitive.m_Shape )
        {
        case S_SEGMENT:         // usual segment : line with rounded ends
            bs_info[0] = _( "Segment" );
            bs_info[1] = _( "from " ) + formatCoord( primitive.m_Start );
            bs_info[2] = _( "to " ) +  formatCoord( primitive.m_End );
            break;

        case S_ARC:             // Arc with rounded ends
            bs_info[0] = _( "Arc" );
            bs_info[1] = _( "center " ) + formatCoord( primitive.m_Start );     // Center
            bs_info[2] = _( "start " ) + formatCoord( primitive.m_End );       // Start point
            bs_info[3] = wxString::Format( _( "angle %s" ), FMT_ANGLE( primitive.m_ArcAngle ) );
            break;

        case S_CIRCLE:          //  ring or circle
            if( primitive.m_Thickness )
                bs_info[0] = _( "ring" );
            else
                bs_info[0] = _( "circle" );

            bs_info[1] = formatCoord( primitive.m_Start );
            bs_info[2] = wxString::Format( _( "radius %s" ),
                                CoordinateToString( primitive.m_Radius, true ) );
            break;

        case S_POLYGON:         // polygon
            bs_info[0] = "Polygon";
            bs_info[1] = wxString::Format( _( "corners count %d" ), primitive.m_Poly.size() );
            break;

        default:
            bs_info[0] = "Unknown primitive";
            break;
        }

        long tmp = m_listCtrlPrimitives->InsertItem(ii, bs_info[0]);
        m_listCtrlPrimitives->SetItemData(tmp, ii);

        for( int jj = 0, col = 0; jj < 5; ++jj )
        {
            m_listCtrlPrimitives->SetItem(tmp, col++, bs_info[jj]);
        }
    }

    // Now columns are filled, ensure correct width of columns
    for( unsigned ii = 0; ii < 5; ++ii )
        m_listCtrlPrimitives->SetColumnWidth( ii, wxLIST_AUTOSIZE );
}

void DIALOG_PAD_PROPERTIES::OnResize( wxSizeEvent& event )
{
    redraw();
    event.Skip();
}


void DIALOG_PAD_PROPERTIES::onChangePadMode( wxCommandEvent& event )
{
    m_drawPadOutlineMode = m_cbShowPadOutline->GetValue();

    if( m_parent->IsGalCanvasActive() )
    {
        KIGFX::VIEW* view = m_panelShowPadGal->GetView();

        // fix the pad render mode (filled/not filled)
        KIGFX::PCB_RENDER_SETTINGS* settings =
            static_cast<KIGFX::PCB_RENDER_SETTINGS*>( view->GetPainter()->GetSettings() );

        settings->SetSketchMode( LAYER_PADS_TH, m_drawPadOutlineMode );
        settings->SetSketchMode( LAYER_PAD_FR, m_drawPadOutlineMode );
        settings->SetSketchMode( LAYER_PAD_BK, m_drawPadOutlineMode );
        settings->SetSketchModeGraphicItems( m_drawPadOutlineMode );
    }

    redraw();
}


void DIALOG_PAD_PROPERTIES::OnPadShapeSelection( wxCommandEvent& event )
{
    bool is_custom = false;

    switch( m_PadShape->GetSelection() )
    {
    case CHOICE_SHAPE_CIRCLE:
        m_ShapeDelta_Ctrl->Enable( false );
        m_trapDeltaDirChoice->Enable( false );
        m_ShapeSize_Y_Ctrl->Enable( false );
        m_ShapeOffset_X_Ctrl->Enable( false );
        m_ShapeOffset_Y_Ctrl->Enable( false );
        break;

    case CHOICE_SHAPE_OVAL:
        m_ShapeDelta_Ctrl->Enable( false );
        m_trapDeltaDirChoice->Enable( false );
        m_ShapeSize_Y_Ctrl->Enable( true );
        m_ShapeOffset_X_Ctrl->Enable( true );
        m_ShapeOffset_Y_Ctrl->Enable( true );
        break;

    case CHOICE_SHAPE_RECT:
        m_ShapeDelta_Ctrl->Enable( false );
        m_trapDeltaDirChoice->Enable( false );
        m_ShapeSize_Y_Ctrl->Enable( true );
        m_ShapeOffset_X_Ctrl->Enable( true );
        m_ShapeOffset_Y_Ctrl->Enable( true );
        break;

    case CHOICE_SHAPE_TRAPEZOID:
        m_ShapeDelta_Ctrl->Enable( true );
        m_trapDeltaDirChoice->Enable( true );
        m_ShapeSize_Y_Ctrl->Enable( true );
        m_ShapeOffset_X_Ctrl->Enable( true );
        m_ShapeOffset_Y_Ctrl->Enable( true );
        break;

    case CHOICE_SHAPE_ROUNDRECT:
        m_ShapeDelta_Ctrl->Enable( false );
        m_trapDeltaDirChoice->Enable( false );
        m_ShapeSize_Y_Ctrl->Enable( true );
        m_ShapeOffset_X_Ctrl->Enable( true );
        m_ShapeOffset_Y_Ctrl->Enable( true );
        // Ensure m_tcCornerSizeRatio contains the right value:
        m_tcCornerSizeRatio->ChangeValue( wxString::Format( "%.1f",
                                m_dummyPad->GetRoundRectRadiusRatio()*100 ) );
        break;

    case CHOICE_SHAPE_CUSTOM_CIRC_ANCHOR:     // PAD_SHAPE_CUSTOM, circular anchor
    case CHOICE_SHAPE_CUSTOM_RECT_ANCHOR:     // PAD_SHAPE_CUSTOM, rect anchor
        is_custom = true;
        m_ShapeDelta_Ctrl->Enable( false );
        m_trapDeltaDirChoice->Enable( false );
        m_ShapeSize_Y_Ctrl->Enable(
            m_PadShape->GetSelection() == CHOICE_SHAPE_CUSTOM_RECT_ANCHOR );
        m_ShapeOffset_X_Ctrl->Enable( false );
        m_ShapeOffset_Y_Ctrl->Enable( false );
        break;
    }

    enablePrimitivePage( is_custom );

    // A few widgets are enabled only for rounded rect pads:
    m_tcCornerSizeRatio->Enable( m_PadShape->GetSelection() == CHOICE_SHAPE_ROUNDRECT );

    // PAD_SHAPE_CUSTOM type has constraints for zone connection and thermal shape:
    // only not connected is allowed to avoid destroying the shape.
    // Enable/disable options only available for custom shaped pads
    m_ZoneConnectionChoice->Enable( !is_custom );
    m_ThermalWidthCtrl->Enable( !is_custom );
    m_ThermalGapCtrl->Enable( !is_custom );

    m_sbSizerZonesSettings->Show( !is_custom );
    m_sbSizerCustomShapedZonesSettings->Show( is_custom );

    transferDataToPad( m_dummyPad );

    updateRoundRectCornerValues();
    redraw();
}


void DIALOG_PAD_PROPERTIES::OnDrillShapeSelected( wxCommandEvent& event )
{
    if( m_PadType->GetSelection() == 1 || m_PadType->GetSelection() == 2 )
    {
        // pad type = SMD or CONN: no hole allowed
        m_PadDrill_X_Ctrl->Enable( false );
        m_PadDrill_Y_Ctrl->Enable( false );
    }
    else
    {
        switch( m_DrillShapeCtrl->GetSelection() )
        {
        case 0:     //CIRCLE:
            m_PadDrill_X_Ctrl->Enable( true );
            m_PadDrill_Y_Ctrl->Enable( false );
            break;

        case 1:     //OVALE:
            m_PadDrill_X_Ctrl->Enable( true );
            m_PadDrill_Y_Ctrl->Enable( true );
            break;
        }
    }

    transferDataToPad( m_dummyPad );
    redraw();
}


void DIALOG_PAD_PROPERTIES::PadOrientEvent( wxCommandEvent& event )
{
    switch( m_PadOrient->GetSelection() )
    {
    case 0:
        m_dummyPad->SetOrientation( 0 );
        break;

    case 1:
        m_dummyPad->SetOrientation( 900 );
        break;

    case 2:
        m_dummyPad->SetOrientation( -900 );
        break;

    case 3:
        m_dummyPad->SetOrientation( 1800 );
        break;

    default:
        break;
    }

    m_OrientValue = m_dummyPad->GetOrientation() / 10.0;
    m_OrientValidator.TransferToWindow();

    transferDataToPad( m_dummyPad );
    redraw();
}


void DIALOG_PAD_PROPERTIES::PadTypeSelected( wxCommandEvent& event )
{
    unsigned ii = m_PadType->GetSelection();

    if( ii >= DIM( code_type ) ) // catches < 0 also
        ii = 0;

    LSET layer_mask = std_pad_layers[ii];
    setPadLayersList( layer_mask );

    // Enable/disable drill dialog items:
    event.SetId( m_DrillShapeCtrl->GetSelection() );
    OnDrillShapeSelected( event );

    if( ii == 0 || ii == DIM( code_type )-1 )
        m_DrillShapeCtrl->Enable( true );
    else
        m_DrillShapeCtrl->Enable( false );

    // Enable/disable Pad name,and pad length die
    // (disable for NPTH pads (mechanical pads)
    bool enable = ii != 3;
    m_PadNumCtrl->Enable( enable );
    m_PadNetNameCtrl->Enable( m_canEditNetName && enable && m_currentPad != NULL );
    m_LengthPadToDieCtrl->Enable( enable );
}


void DIALOG_PAD_PROPERTIES::setPadLayersList( LSET layer_mask )
{
    LSET cu_set = layer_mask & LSET::AllCuMask();

    if( cu_set == LSET( F_Cu ) )
        m_rbCopperLayersSel->SetSelection( 0 );
    else if( cu_set == LSET( B_Cu ) )
        m_rbCopperLayersSel->SetSelection( 1 );
    else if( cu_set.any() )
        m_rbCopperLayersSel->SetSelection( 2 );
    else
        m_rbCopperLayersSel->SetSelection( 3 );

    m_PadLayerAdhCmp->SetValue( layer_mask[F_Adhes] );
    m_PadLayerAdhCu->SetValue( layer_mask[B_Adhes] );

    m_PadLayerPateCmp->SetValue( layer_mask[F_Paste] );
    m_PadLayerPateCu->SetValue( layer_mask[B_Paste] );

    m_PadLayerSilkCmp->SetValue( layer_mask[F_SilkS] );
    m_PadLayerSilkCu->SetValue( layer_mask[B_SilkS] );

    m_PadLayerMaskCmp->SetValue( layer_mask[F_Mask] );
    m_PadLayerMaskCu->SetValue( layer_mask[B_Mask] );

    m_PadLayerECO1->SetValue( layer_mask[Eco1_User] );
    m_PadLayerECO2->SetValue( layer_mask[Eco2_User] );

    m_PadLayerDraft->SetValue( layer_mask[Dwgs_User] );
}


// Called when select/deselect a layer.
void DIALOG_PAD_PROPERTIES::OnSetLayers( wxCommandEvent& event )
{
    transferDataToPad( m_dummyPad );
    redraw();
}


// test if all values are acceptable for the pad
bool DIALOG_PAD_PROPERTIES::padValuesOK()
{
    bool error = transferDataToPad( m_dummyPad );
    bool skip_tstoffset = false;    // the offset prm is not always tested

    wxArrayString error_msgs;
    wxString msg;

    // Test for incorrect values
    if( (m_dummyPad->GetSize().x <= 0) ||
       ((m_dummyPad->GetSize().y <= 0) && (m_dummyPad->GetShape() != PAD_SHAPE_CIRCLE)) )
    {
        error_msgs.Add( _( "Pad size must be greater than zero" ) );
    }

    if( (m_dummyPad->GetSize().x < m_dummyPad->GetDrillSize().x) ||
        (m_dummyPad->GetSize().y < m_dummyPad->GetDrillSize().y) )
    {
        error_msgs.Add(  _( "Incorrect value for pad drill: pad drill bigger than pad size" ) );
        skip_tstoffset = true;  // offset prm will be not tested because if the drill value
                                // is incorrect the offset prm is always seen as incorrect, even if it is 0
    }

    if( m_dummyPad->GetLocalClearance() < 0 )
    {
        error_msgs.Add( _( "Pad local clearance must be zero or greater than zero" ) );
    }

    // Some pads need a negative solder mask clearance (mainly for BGA with small pads)
    // However the negative solder mask clearance must not create negative mask size
    // Therefore test for minimal acceptable negative value
    // Hovewer, a negative value can give strange result with custom shapes, so it is not
    // allowed for custom pad shape
    if( m_dummyPad->GetLocalSolderMaskMargin() < 0 )
    {
        if( m_dummyPad->GetShape() == PAD_SHAPE_CUSTOM )
            error_msgs.Add( _( "Pad local solder mask clearance must be zero or greater than zero" ) );
        else
            {
            int min_smClearance = -std::min( m_dummyPad->GetSize().x, m_dummyPad->GetSize().y )/2;

            if( m_dummyPad->GetLocalSolderMaskMargin() <= min_smClearance )
            {
                error_msgs.Add( wxString::Format(
                                _( "Pad local solder mask clearance must be greater than %s" ),
                                StringFromValue( g_UserUnit, min_smClearance, true ) ) );
            }
        }
    }

    if( m_dummyPad->GetLocalSolderPasteMargin() > 0 )
    {
        error_msgs.Add( _( "Pad local solder paste clearance must be zero or less than zero" ) );
    }

    LSET padlayers_mask = m_dummyPad->GetLayerSet();

    if( padlayers_mask == 0 )
        error_msgs.Add( _( "Error: pad has no layer" ) );

    if( !padlayers_mask[F_Cu] && !padlayers_mask[B_Cu] )
    {
        if( m_dummyPad->GetDrillSize().x || m_dummyPad->GetDrillSize().y )
        {
            // Note: he message is shown in an HTML window
            msg = _( "Error: the pad is not on a copper layer and has a hole" );

            if( m_dummyPad->GetAttribute() == PAD_ATTRIB_HOLE_NOT_PLATED )
            {
                msg += wxT( "<br><br><i>" );
                msg += _( "For NPTH pad, set pad size value to pad drill value,"
                          " if you do not want this pad plotted in gerber files"
                    );
            }

            error_msgs.Add( msg );
        }
    }

    if( !skip_tstoffset )
    {
        wxPoint max_size;
        max_size.x = std::abs( m_dummyPad->GetOffset().x );
        max_size.y = std::abs( m_dummyPad->GetOffset().y );
        max_size.x += m_dummyPad->GetDrillSize().x / 2;
        max_size.y += m_dummyPad->GetDrillSize().y / 2;

        if( ( m_dummyPad->GetSize().x / 2 < max_size.x ) ||
            ( m_dummyPad->GetSize().y / 2 < max_size.y ) )
        {
            error_msgs.Add( _( "Incorrect value for pad offset" ) );
        }
    }

    if( error )
    {
        error_msgs.Add(  _( "Too large value for pad delta size" ) );
    }

    switch( m_dummyPad->GetAttribute() )
    {
    case PAD_ATTRIB_HOLE_NOT_PLATED:   // Not plated, but through hole, a hole is expected
    case PAD_ATTRIB_STANDARD :         // Pad through hole, a hole is also expected
        if( m_dummyPad->GetDrillSize().x <= 0 )
            error_msgs.Add( _( "Error: Through hole pad: drill diameter set to 0" ) );
        break;

    case PAD_ATTRIB_CONN:      // Connector pads are smd pads, just they do not have solder paste.
        if( padlayers_mask[B_Paste] || padlayers_mask[F_Paste] )
            error_msgs.Add( _( "Error: Connector pads are not on the solder paste layer\n"
                               "Use SMD pads instead" ) );
        // Fall trough
    case PAD_ATTRIB_SMD:       // SMD and Connector pads (One external copper layer only)
        {
        LSET innerlayers_mask = padlayers_mask & LSET::InternalCuMask();

        if( ( padlayers_mask[F_Cu] && padlayers_mask[B_Cu] ) ||
            innerlayers_mask.count() != 0 )
            error_msgs.Add( _( "Error: only one external copper layer allowed for SMD or Connector pads" ) );
        }
        break;
    }


    if( m_dummyPad->GetShape() == PAD_SHAPE_ROUNDRECT )
    {
        wxString value = m_tcCornerSizeRatio->GetValue();
        double rrRadiusRatioPercent;

        if( !value.ToDouble( &rrRadiusRatioPercent ) )
            error_msgs.Add( _( "Incorrect corner size value" ) );
        else
        {
            if( rrRadiusRatioPercent < 0.0 )
                error_msgs.Add( _( "Incorrect (negative) corner size value" ) );
            else if( rrRadiusRatioPercent > 50.0 )
                error_msgs.Add( _( "Corner size value must be smaller than 50%" ) );
        }
    }

    if( m_dummyPad->GetShape() == PAD_SHAPE_CUSTOM )
    {
        if( !m_dummyPad->MergePrimitivesAsPolygon( ) )
            error_msgs.Add(
                _( "Incorrect pad shape: the shape must be equivalent to only one polygon" ) );
    }


    if( error_msgs.GetCount() )
    {
        HTML_MESSAGE_BOX dlg( this, _("Pad setup errors list" ) );
        dlg.ListSet( error_msgs );
        dlg.ShowModal();
    }

    return error_msgs.GetCount() == 0;
}


void DIALOG_PAD_PROPERTIES::redraw()
{
    if( m_parent->IsGalCanvasActive() )
    {
        KIGFX::VIEW* view = m_panelShowPadGal->GetView();
        m_panelShowPadGal->StopDrawing();

        // The layer used to place primitive items selected when editing custom pad shapes
        // we use here a layer never used in a pad:
        #define SELECTED_ITEMS_LAYER Dwgs_User

        view->SetTopLayer( SELECTED_ITEMS_LAYER );
        KIGFX::PCB_RENDER_SETTINGS* settings =
            static_cast<KIGFX::PCB_RENDER_SETTINGS*>( view->GetPainter()->GetSettings() );
        settings->SetLayerColor( SELECTED_ITEMS_LAYER, m_selectedColor );

        view->Update( m_dummyPad );

        // delete previous items if highlight list
        while( m_highligth.size() )
        {
            delete m_highligth.back(); // the dtor also removes item from view
            m_highligth.pop_back();
        }

        // highlight selected primitives:
        long select = m_listCtrlPrimitives->GetFirstSelected();

        while( select >= 0 )
        {
            PAD_CS_PRIMITIVE& primitive = m_primitives[select];

            DRAWSEGMENT* dummySegment = new DRAWSEGMENT;
            dummySegment->SetLayer( SELECTED_ITEMS_LAYER );
            primitive.ExportTo( dummySegment );
            dummySegment->Rotate( wxPoint( 0, 0), m_dummyPad->GetOrientation() );
            dummySegment->Move( m_dummyPad->GetPosition() );

            // Update selected primitive (highligth selected)
            switch( primitive.m_Shape )
            {
            case S_SEGMENT:
            case S_ARC:
                break;

            case S_CIRCLE:          //  ring or circle
                if( primitive.m_Thickness == 0 )    // filled circle
                {   // the filled circle option does not exist in a DRAWSEGMENT
                    // but it is easy to create it with a circle having the
                    // right radius and outline width
                    wxPoint end = dummySegment->GetCenter();
                    end.x += primitive.m_Radius/2;
                    dummySegment->SetEnd( end );
                    dummySegment->SetWidth( primitive.m_Radius );
                }
                break;

            case S_POLYGON:
                break;

            default:
                delete dummySegment;
                dummySegment = nullptr;
                break;
            }

            if( dummySegment )
            {
                view->Add( dummySegment );
                m_highligth.push_back( dummySegment );
            }

            select = m_listCtrlPrimitives->GetNextSelected( select );
        }

        BOX2I bbox = m_dummyPad->ViewBBox();

        if( bbox.GetSize().x > 0 && bbox.GetSize().y > 0 )
        {
            // gives a size to the full drawable area
            BOX2I drawbox;
            drawbox.Move( m_dummyPad->GetPosition() );
            drawbox.Inflate( bbox.GetSize().x*2, bbox.GetSize().y*2 );
            view->SetBoundary( drawbox );

            // Autozoom
            view->SetViewport( BOX2D( bbox.GetOrigin(), bbox.GetSize() ) );

            // Add a margin
            view->SetScale( m_panelShowPadGal->GetView()->GetScale() * 0.7 );

            m_panelShowPadGal->StartDrawing();
            m_panelShowPadGal->Refresh();
        }
    }
    else
    {
        m_panelShowPad->Refresh();
    }
}


bool DIALOG_PAD_PROPERTIES::TransferDataToWindow()
{
    if( !wxDialog::TransferDataToWindow() )
        return false;

    if( !m_panelGeneral->TransferDataToWindow() )
        return false;

    if( !m_localSettingsPanel->TransferDataToWindow() )
        return false;

    return true;
}


bool DIALOG_PAD_PROPERTIES::TransferDataFromWindow()
{
    BOARD_COMMIT commit( m_parent );

    if( !wxDialog::TransferDataFromWindow() )
        return false;

    if( !m_panelGeneral->TransferDataFromWindow() )
        return false;

    if( !m_localSettingsPanel->TransferDataFromWindow() )
        return false;

    if( !padValuesOK() )
        return false;

    bool rastnestIsChanged = false;
    int  isign = m_isFlipped ? -1 : 1;

    transferDataToPad( m_padMaster );
    // m_padMaster is a pattern: ensure there is no net for this pad:
    m_padMaster->SetNetCode( NETINFO_LIST::UNCONNECTED );

    if( !m_currentPad )   // Set current Pad parameters
        return true;

    commit.Modify( m_currentPad );

    // redraw the area where the pad was, without pad (delete pad on screen)
    m_currentPad->SetFlags( DO_NOT_DRAW );
    m_parent->GetCanvas()->RefreshDrawingRect( m_currentPad->GetBoundingBox() );
    m_currentPad->ClearFlags( DO_NOT_DRAW );

    // Update values
    m_currentPad->SetShape( m_padMaster->GetShape() );
    m_currentPad->SetAttribute( m_padMaster->GetAttribute() );

    if( m_currentPad->GetPosition() != m_padMaster->GetPosition() )
    {
        m_currentPad->SetPosition( m_padMaster->GetPosition() );
        rastnestIsChanged = true;
    }

    wxSize  size;
    MODULE* footprint = m_currentPad->GetParent();

    if( footprint )
    {
        footprint->SetLastEditTime();

        // compute the pos 0 value, i.e. pad position for footprint with orientation = 0
        // i.e. relative to footprint origin (footprint position)
        wxPoint pt = m_currentPad->GetPosition() - footprint->GetPosition();
        RotatePoint( &pt, -footprint->GetOrientation() );
        m_currentPad->SetPos0( pt );
        m_currentPad->SetOrientation( m_padMaster->GetOrientation() * isign
                                        + footprint->GetOrientation() );
    }

    m_currentPad->SetSize( m_padMaster->GetSize() );

    size = m_padMaster->GetDelta();
    size.y *= isign;
    m_currentPad->SetDelta( size );

    m_currentPad->SetDrillSize( m_padMaster->GetDrillSize() );
    m_currentPad->SetDrillShape( m_padMaster->GetDrillShape() );

    wxPoint offset = m_padMaster->GetOffset();
    offset.y *= isign;
    m_currentPad->SetOffset( offset );

    m_currentPad->SetPadToDieLength( m_padMaster->GetPadToDieLength() );

    if( m_padMaster->GetShape() != PAD_SHAPE_CUSTOM )
        m_padMaster->DeletePrimitivesList();


    m_currentPad->SetAnchorPadShape( m_padMaster->GetAnchorPadShape() );
    m_currentPad->SetPrimitives( m_padMaster->GetPrimitives() );

    if( m_isFlipped )
    {
        m_currentPad->SetLayerSet( FlipLayerMask( m_currentPad->GetLayerSet() ) );
        m_currentPad->FlipPrimitives();
    }

    if( m_currentPad->GetLayerSet() != m_padMaster->GetLayerSet() )
    {
        rastnestIsChanged = true;
        m_currentPad->SetLayerSet( m_padMaster->GetLayerSet() );
    }

    if( m_isFlipped )
    {
        m_currentPad->SetLayerSet( FlipLayerMask( m_currentPad->GetLayerSet() ) );
    }

    m_currentPad->SetName( m_padMaster->GetName() );

    wxString padNetname;

    // For PAD_ATTRIB_HOLE_NOT_PLATED, ensure there is no net name selected
    if( m_padMaster->GetAttribute() != PAD_ATTRIB_HOLE_NOT_PLATED  )
        padNetname = m_PadNetNameCtrl->GetValue();

    if( m_currentPad->GetNetname() != padNetname )
    {
        const NETINFO_ITEM* netinfo = m_board->FindNet( padNetname );

        if( !padNetname.IsEmpty() && netinfo == NULL )
        {
            DisplayError( NULL, _( "Unknown netname, netname not changed" ) );
        }
        else if( netinfo )
        {
            rastnestIsChanged = true;
            m_currentPad->SetNetCode( netinfo->GetNet() );
        }
    }

    m_currentPad->SetLocalClearance( m_padMaster->GetLocalClearance() );
    m_currentPad->SetLocalSolderMaskMargin( m_padMaster->GetLocalSolderMaskMargin() );
    m_currentPad->SetLocalSolderPasteMargin( m_padMaster->GetLocalSolderPasteMargin() );
    m_currentPad->SetLocalSolderPasteMarginRatio( m_padMaster->GetLocalSolderPasteMarginRatio() );
    m_currentPad->SetThermalWidth( m_padMaster->GetThermalWidth() );
    m_currentPad->SetThermalGap( m_padMaster->GetThermalGap() );
    m_currentPad->SetRoundRectRadiusRatio( m_padMaster->GetRoundRectRadiusRatio() );

    if( m_currentPad->GetShape() == PAD_SHAPE_CUSTOM )
        m_currentPad->SetZoneConnection( PAD_ZONE_CONN_NONE );
    else
        m_currentPad->SetZoneConnection( m_padMaster->GetZoneConnection() );


    // rounded rect pads with radius ratio = 0 are in fact rect pads.
    // So set the right shape (and perhaps issues with a radius = 0)
    if( m_currentPad->GetShape() == PAD_SHAPE_ROUNDRECT &&
        m_currentPad->GetRoundRectRadiusRatio() == 0.0 )
    {
        m_currentPad->SetShape( PAD_SHAPE_RECT );
    }

    // define the way the clearance area is defined in zones
    m_currentPad->SetCustomShapeInZoneOpt( m_padMaster->GetCustomShapeInZoneOpt() );

    if( footprint )
        footprint->CalculateBoundingBox();

    m_parent->SetMsgPanel( m_currentPad );

    // redraw the area where the pad was
    m_parent->GetCanvas()->RefreshDrawingRect( m_currentPad->GetBoundingBox() );

    commit.Push( _( "Modify pad" ) );

    if( rastnestIsChanged )  // The net ratsnest must be recalculated
        m_board->m_Status_Pcb = 0;

    return true;
}


bool DIALOG_PAD_PROPERTIES::transferDataToPad( D_PAD* aPad )
{
    wxString    msg;
    int         x, y;

    if( !Validate() )
        return true;
    if( !m_panelGeneral->Validate() )
        return true;
    if( !m_localSettingsPanel->Validate() )
        return true;

    m_OrientValidator.TransferFromWindow();

    aPad->SetAttribute( code_type[m_PadType->GetSelection()] );
    aPad->SetShape( code_shape[m_PadShape->GetSelection()] );
    aPad->SetAnchorPadShape( m_PadShape->GetSelection() == CHOICE_SHAPE_CUSTOM_RECT_ANCHOR ?
                                PAD_SHAPE_RECT : PAD_SHAPE_CIRCLE );

    if( aPad->GetShape() == PAD_SHAPE_CUSTOM )
        aPad->SetPrimitives( m_primitives );

    // Read pad clearances values:
    aPad->SetLocalClearance( ValueFromTextCtrl( *m_NetClearanceValueCtrl ) );
    aPad->SetLocalSolderMaskMargin( ValueFromTextCtrl( *m_SolderMaskMarginCtrl ) );
    aPad->SetLocalSolderPasteMargin( ValueFromTextCtrl( *m_SolderPasteMarginCtrl ) );
    aPad->SetThermalWidth( ValueFromTextCtrl( *m_ThermalWidthCtrl ) );
    aPad->SetThermalGap( ValueFromTextCtrl( *m_ThermalGapCtrl ) );
    double dtmp = 0.0;
    msg = m_SolderPasteMarginRatioCtrl->GetValue();
    msg.ToDouble( &dtmp );

    // A -50% margin ratio means no paste on a pad, the ratio must be >= -50%
    if( dtmp < -50.0 )
        dtmp = -50.0;
    // A margin ratio is always <= 0
    // 0 means use full pad copper area
    if( dtmp > 0.0 )
        dtmp = 0.0;

    aPad->SetLocalSolderPasteMarginRatio( dtmp / 100 );

    switch( m_ZoneConnectionChoice->GetSelection() )
    {
    default:
    case 0:
        aPad->SetZoneConnection( PAD_ZONE_CONN_INHERITED );
        break;

    case 1:
        aPad->SetZoneConnection( PAD_ZONE_CONN_FULL );
        break;

    case 2:
        aPad->SetZoneConnection( PAD_ZONE_CONN_THERMAL );
        break;

    case 3:
        aPad->SetZoneConnection( PAD_ZONE_CONN_NONE );
        break;
    }

    // Read pad position:
    x = ValueFromTextCtrl( *m_PadPosition_X_Ctrl );
    y = ValueFromTextCtrl( *m_PadPosition_Y_Ctrl );

    aPad->SetPosition( wxPoint( x, y ) );
    aPad->SetPos0( wxPoint( x, y ) );

    // Read pad drill:
    x = ValueFromTextCtrl( *m_PadDrill_X_Ctrl );
    y = ValueFromTextCtrl( *m_PadDrill_Y_Ctrl );

    if( m_DrillShapeCtrl->GetSelection() == 0 )
    {
        aPad->SetDrillShape( PAD_DRILL_SHAPE_CIRCLE );
        y = x;
    }
    else
        aPad->SetDrillShape( PAD_DRILL_SHAPE_OBLONG );

    aPad->SetDrillSize( wxSize( x, y ) );

    // Read pad shape size:
    x = ValueFromTextCtrl( *m_ShapeSize_X_Ctrl );
    y = ValueFromTextCtrl( *m_ShapeSize_Y_Ctrl );

    if( aPad->GetShape() == PAD_SHAPE_CIRCLE )
        y = x;

    // for custom shped pads, the pad size is the anchor pad size:
    if( aPad->GetShape() == PAD_SHAPE_CUSTOM && aPad->GetAnchorPadShape() == PAD_SHAPE_CIRCLE )
        y = x;

    aPad->SetSize( wxSize( x, y ) );

    // Read pad length die
    aPad->SetPadToDieLength( ValueFromTextCtrl( *m_LengthPadToDieCtrl ) );

    // For a trapezoid, test delta value (be sure delta is not too large for pad size)
    // remember DeltaSize.x is the Y size variation
    bool   error    = false;

    if( aPad->GetShape() == PAD_SHAPE_TRAPEZOID )
    {
        wxSize delta;

        // For a trapezoid, only one of delta.x or delta.y is not 0, depending on
        // the direction.
        if( m_trapDeltaDirChoice->GetSelection() == 0 )
            delta.x = ValueFromTextCtrl( *m_ShapeDelta_Ctrl );
        else
            delta.y = ValueFromTextCtrl( *m_ShapeDelta_Ctrl );

        if( delta.x < 0 && delta.x <= -aPad->GetSize().y )
        {
            delta.x = -aPad->GetSize().y + 2;
            error = true;
        }

        if( delta.x > 0 && delta.x >= aPad->GetSize().y )
        {
            delta.x = aPad->GetSize().y - 2;
            error = true;
        }

        if( delta.y < 0 && delta.y <= -aPad->GetSize().x )
        {
            delta.y = -aPad->GetSize().x + 2;
            error = true;
        }

        if( delta.y > 0 && delta.y >= aPad->GetSize().x )
        {
            delta.y = aPad->GetSize().x - 2;
            error = true;
        }

        aPad->SetDelta( delta );
    }

    // Read pad shape offset:
    x = ValueFromTextCtrl( *m_ShapeOffset_X_Ctrl );
    y = ValueFromTextCtrl( *m_ShapeOffset_Y_Ctrl );
    aPad->SetOffset( wxPoint( x, y ) );

    aPad->SetOrientation( m_OrientValue * 10.0 );
    aPad->SetName( m_PadNumCtrl->GetValue() );

    // Check if user has set an existing net name
    const NETINFO_ITEM* netinfo = m_board->FindNet( m_PadNetNameCtrl->GetValue() );

    if( netinfo != NULL )
        aPad->SetNetCode( netinfo->GetNet() );
    else
        aPad->SetNetCode( NETINFO_LIST::UNCONNECTED );

    // Clear some values, according to the pad type and shape
    switch( aPad->GetShape() )
    {
    case PAD_SHAPE_CIRCLE:
        aPad->SetOffset( wxPoint( 0, 0 ) );
        aPad->SetDelta( wxSize( 0, 0 ) );
        x = aPad->GetSize().x;
        aPad->SetSize( wxSize( x, x ) );
        break;

    case PAD_SHAPE_RECT:
        aPad->SetDelta( wxSize( 0, 0 ) );
        break;

    case PAD_SHAPE_OVAL:
        aPad->SetDelta( wxSize( 0, 0 ) );
        break;

    case PAD_SHAPE_TRAPEZOID:
        break;

    case PAD_SHAPE_ROUNDRECT:
        aPad->SetDelta( wxSize( 0, 0 ) );
        break;

    case PAD_SHAPE_CUSTOM:
        aPad->SetOffset( wxPoint( 0, 0 ) );
        aPad->SetDelta( wxSize( 0, 0 ) );

        // The pad custom has a "anchor pad" (a basic shape: round or rect pad)
        // that is the minimal area of this pad, and is usefull to ensure a hole
        // diameter is acceptable, and is used in Gerber files as flashed area
        // reference
        if( aPad->GetAnchorPadShape() == PAD_SHAPE_CIRCLE )
        {
            x = aPad->GetSize().x;
            aPad->SetSize( wxSize( x, x ) );
        }

        // define the way the clearance area is defined in zones
        aPad->SetCustomShapeInZoneOpt( m_ZoneCustomPadShape->GetSelection() == 0 ?
                                       CUST_PAD_SHAPE_IN_ZONE_OUTLINE :
                                       CUST_PAD_SHAPE_IN_ZONE_CONVEXHULL );

        break;

    default:
        ;
    }

    switch( aPad->GetAttribute() )
    {
    case PAD_ATTRIB_STANDARD:
        break;

    case PAD_ATTRIB_CONN:
    case PAD_ATTRIB_SMD:
        // SMD and PAD_ATTRIB_CONN has no hole.
        // basically, SMD and PAD_ATTRIB_CONN are same type of pads
        // PAD_ATTRIB_CONN has just a default non technical layers that differs from SMD
        // and are intended to be used in virtual edge board connectors
        // However we can accept a non null offset,
        // mainly to allow complex pads build from a set of basic pad shapes
        aPad->SetDrillSize( wxSize( 0, 0 ) );
        break;

    case PAD_ATTRIB_HOLE_NOT_PLATED:
        // Mechanical purpose only:
        // no offset, no net name, no pad name allowed
        aPad->SetOffset( wxPoint( 0, 0 ) );
        aPad->SetName( wxEmptyString );
        aPad->SetNetCode( NETINFO_LIST::UNCONNECTED );
        break;

    default:
        DisplayError( NULL, wxT( "Error: unknown pad type" ) );
        break;
    }

    if( aPad->GetShape() == PAD_SHAPE_ROUNDRECT )
    {
        wxString value = m_tcCornerSizeRatio->GetValue();
        double rrRadiusRatioPercent;

        if( value.ToDouble( &rrRadiusRatioPercent ) )
            aPad->SetRoundRectRadiusRatio( rrRadiusRatioPercent / 100.0 );
    }

    LSET padLayerMask;

    switch( m_rbCopperLayersSel->GetSelection() )
    {
    case 0:
        padLayerMask.set( F_Cu );
        break;

    case 1:
        padLayerMask.set( B_Cu );
        break;

    case 2:
        padLayerMask |= LSET::AllCuMask();
        break;

    case 3:     // No copper layers
        break;
    }

    if( m_PadLayerAdhCmp->GetValue() )
        padLayerMask.set( F_Adhes );

    if( m_PadLayerAdhCu->GetValue() )
        padLayerMask.set( B_Adhes );

    if( m_PadLayerPateCmp->GetValue() )
        padLayerMask.set( F_Paste );

    if( m_PadLayerPateCu->GetValue() )
        padLayerMask.set( B_Paste );

    if( m_PadLayerSilkCmp->GetValue() )
        padLayerMask.set( F_SilkS );

    if( m_PadLayerSilkCu->GetValue() )
        padLayerMask.set( B_SilkS );

    if( m_PadLayerMaskCmp->GetValue() )
        padLayerMask.set( F_Mask );

    if( m_PadLayerMaskCu->GetValue() )
        padLayerMask.set( B_Mask );

    if( m_PadLayerECO1->GetValue() )
        padLayerMask.set( Eco1_User );

    if( m_PadLayerECO2->GetValue() )
        padLayerMask.set( Eco2_User );

    if( m_PadLayerDraft->GetValue() )
        padLayerMask.set( Dwgs_User );

    aPad->SetLayerSet( padLayerMask );

    return error;
}


void DIALOG_PAD_PROPERTIES::OnValuesChanged( wxCommandEvent& event )
{
    if( m_canUpdate )
    {
        transferDataToPad( m_dummyPad );
        // If the pad size has changed, update the displayed values
        // for rounded rect pads
        updateRoundRectCornerValues();

        redraw();
    }
}

void DIALOG_PAD_PROPERTIES::editPrimitive()
{
    long select = m_listCtrlPrimitives->GetFirstSelected();

    if( select < 0 )
    {
        wxMessageBox( _( "No shape selected" ) );
        return;
    }

    PAD_CS_PRIMITIVE& shape = m_primitives[select];

    if( shape.m_Shape == S_POLYGON )
    {
        DIALOG_PAD_PRIMITIVE_POLY_PROPS dlg( this, &shape );

        if( dlg.ShowModal() != wxID_OK )
            return;

        dlg.TransferDataFromWindow();
    }

    else
    {
        DIALOG_PAD_PRIMITIVES_PROPERTIES dlg( this, &shape );

        if( dlg.ShowModal() != wxID_OK )
            return;

        dlg.TransferDataFromWindow();
    }

    displayPrimitivesList();

    if( m_canUpdate )
    {
        transferDataToPad( m_dummyPad );
        redraw();
    }
}


void DIALOG_PAD_PROPERTIES::OnPrimitiveSelection( wxListEvent& event )
{
    // Called on a double click on the basic shapes list
    // To Do: highligth the primitive(s) currently selected.
    redraw();
}


/// Called on a double click on the basic shapes list
void DIALOG_PAD_PROPERTIES::onPrimitiveDClick( wxMouseEvent& event )
{
    editPrimitive();
}


// Called on a click on basic shapes list panel button
void DIALOG_PAD_PROPERTIES::onEditPrimitive( wxCommandEvent& event )
{
    editPrimitive();
}

// Called on a click on basic shapes list panel button
void DIALOG_PAD_PROPERTIES::onDeletePrimitive( wxCommandEvent& event )
{
    long select = m_listCtrlPrimitives->GetFirstSelected();

    if( select < 0 )
        return;

    // Multiple selections are allowed. get them and remove corresponding shapes
    std::vector<long> indexes;
    indexes.push_back( select );

    while( ( select = m_listCtrlPrimitives->GetNextSelected( select ) ) >= 0 )
        indexes.push_back( select );

    // Erase all select shapes
    for( unsigned ii = indexes.size(); ii > 0; --ii )
        m_primitives.erase( m_primitives.begin() + indexes[ii-1] );

    displayPrimitivesList();

    if( m_canUpdate )
    {
        transferDataToPad( m_dummyPad );
        redraw();
    }
}


void DIALOG_PAD_PROPERTIES::onAddPrimitive( wxCommandEvent& event )
{
    // Ask user for shape type
    wxString shapelist[] =
    {
        _( "Segment" ), _( "Arc" ), _( "ring/circle" ), _( "polygon" )
    };

    int type = wxGetSingleChoiceIndex( wxEmptyString, _( "Select shape type:" ),
                    DIM( shapelist ), shapelist, 0 );

    STROKE_T listtype[] =
    {
        S_SEGMENT, S_ARC, S_CIRCLE, S_POLYGON
    };

    PAD_CS_PRIMITIVE primitive( listtype[type] );

    if( listtype[type] == S_POLYGON )
    {
        DIALOG_PAD_PRIMITIVE_POLY_PROPS dlg( this, &primitive );

        if( dlg.ShowModal() != wxID_OK )
            return;
    }
    else
    {
        DIALOG_PAD_PRIMITIVES_PROPERTIES dlg( this, &primitive );

        if( dlg.ShowModal() != wxID_OK )
            return;
    }

    m_primitives.push_back( primitive );

    displayPrimitivesList();

    if( m_canUpdate )
    {
        transferDataToPad( m_dummyPad );
        redraw();
    }
}


void DIALOG_PAD_PROPERTIES::onImportPrimitives( wxCommandEvent& event )
{
    wxMessageBox( "Not yet available" );
}


void DIALOG_PAD_PROPERTIES::onGeometryTransform( wxCommandEvent& event )
{
    long select = m_listCtrlPrimitives->GetFirstSelected();

    if( select < 0 )
    {
        wxMessageBox( _( "No shape selected" ) );
        return;
    }

    // Multiple selections are allowed. Build selected shapes list
    std::vector<long> indexes;
    indexes.push_back( select );

    std::vector<PAD_CS_PRIMITIVE*> shapeList;
    shapeList.push_back( &m_primitives[select] );

    while( ( select = m_listCtrlPrimitives->GetNextSelected( select ) ) >= 0 )
    {
        indexes.push_back( select );
        shapeList.push_back( &m_primitives[select] );
    }

    DIALOG_PAD_PRIMITIVES_TRANSFORM dlg( this, shapeList, false );

    if( dlg.ShowModal() != wxID_OK )
        return;

    // Transfert new settings:
    dlg.Transform();

    displayPrimitivesList();

    if( m_canUpdate )
    {
        transferDataToPad( m_dummyPad );
        redraw();
    }
}


void DIALOG_PAD_PROPERTIES::onDuplicatePrimitive( wxCommandEvent& event )
{
    long select = m_listCtrlPrimitives->GetFirstSelected();

    if( select < 0 )
    {
        wxMessageBox( _( "No shape selected" ) );
        return;
    }

    // Multiple selections are allowed. Build selected shapes list
    std::vector<long> indexes;
    indexes.push_back( select );

    std::vector<PAD_CS_PRIMITIVE*> shapeList;
    shapeList.push_back( &m_primitives[select] );

    while( ( select = m_listCtrlPrimitives->GetNextSelected( select ) ) >= 0 )
    {
        indexes.push_back( select );
        shapeList.push_back( &m_primitives[select] );
    }

    DIALOG_PAD_PRIMITIVES_TRANSFORM dlg( this, shapeList, true );

    if( dlg.ShowModal() != wxID_OK )
        return;

    // Transfert new settings:
    dlg.Transform( &m_primitives, dlg.GetDuplicateCount() );

    displayPrimitivesList();

    if( m_canUpdate )
    {
        transferDataToPad( m_dummyPad );
        redraw();
    }
}
