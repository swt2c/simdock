/*  
 *   Copyright 2007 Simone Della Longa <simonedll@yahoo.it>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "main.h"


using namespace std;

enum
{
  ID_Quit = 1,
  ID_About = 2,
  ID_blur_Timer = 3,
  ID_mouse_Timer = 4,
  ID_Settings = 5,
  ID_Add = 6,
  ID_Edit = 7,
  ID_Delete = 8,
  ID_hover_Timer = 9,
  ID_Keep = 10,
  ID_animation = 11,
};


BEGIN_EVENT_TABLE (MyFrame, wxFrame)
EVT_MIDDLE_DOWN (MyFrame::OnMiddleDown)
EVT_MIDDLE_UP (MyFrame::OnMiddleUp)
EVT_LEFT_UP (MyFrame::OnLeftUp)
EVT_LEFT_DOWN (MyFrame::OnLeftDown)
EVT_RIGHT_DOWN (MyFrame::OnRightClick)
EVT_CONTEXT_MENU (MyFrame::OnContextMenu) 
EVT_MOTION (MyFrame::OnMouseMove)
// EVT_MOVE (MyFrame::OnFrameMove)
EVT_CLOSE (MyFrame::OnClose)
EVT_LEAVE_WINDOW (MyFrame::OnMouseLeave)
EVT_ENTER_WINDOW (MyFrame::OnMouseEnter)
EVT_TIMER (ID_blur_Timer, MyFrame::OnBlurTimerTick)
EVT_TIMER (ID_hover_Timer, MyFrame::OnHoverTimerTick)
EVT_TIMER (ID_animation, MyFrame::OnAnimationTick)
/*
* Menu 
*/
EVT_MENU (ID_Quit, MyFrame::OnQuit)
EVT_MENU (ID_About, MyFrame::OnAbout)
EVT_MENU (ID_Settings, MyFrame::OnSettings)
EVT_MENU (ID_Add, MyFrame::OnAdd)
EVT_MENU (ID_Keep, MyFrame::OnKeep)
EVT_MENU (ID_Edit, MyFrame::OnEdit) 
EVT_MENU (ID_Delete, MyFrame::OnDelete)
/*
* Painting 
*/
EVT_PAINT (MyFrame::OnPaint)
END_EVENT_TABLE ()

wxSize
ImageToShadow (int w, int h, int scaling)
{
  return wxSize (w, h / scaling);
}

double
zoom (int stand_dev, float dist_center, int maximum_size)
{
  return maximum_size * exp (-pow (dist_center, 2) / pow (stand_dev, 2));
}



void
fade (wxImage * img, const int &intensity)
{
  register unsigned int len = img->GetWidth () * img->GetHeight ();
  unsigned char *v = img->GetAlpha ();

  unsigned char *v2 = (unsigned char *) malloc (sizeof (unsigned char) * len);
  register unsigned int i;
  for (i = 0; i < len; i++)
    {
      if ((int) v[i] <= intensity)
	v2[i] = (unsigned char) 0;
      else
	v2[i] = (unsigned char) ((int) v[i] - intensity);
    }
  img->SetAlpha (v2);
}

/* Recursive binary search. Images are sorted by position.
 * given a point and a portion of the image list returns 
 * the closest image to the point.
 */
int
positionToId (const wxPoint & p, ImagesArray * list, int min, int max)
{
  if (min == max)
    {
      return max;
    }

  int mid = (min + max) / 2;

  if (p.x > (*list)[mid]->x)
    {
      if (p.x < (*list)[mid]->x + (*list)[mid]->w)
	return mid;
      else
	return positionToId (p, list, mid + 1, max);
    }

  return positionToId (p, list, min, mid);

}

/*
 * Draws the given bitmap starting from (x,y) stretching it to the given
 * width and height 
 */
void
drawBmp (wxGCDC* dc, const wxBitmap & bmp, const int &x, const int &y,
	 const int &w, const int &h)
{
    double xFactor = w / (double) bmp.GetWidth ();
    double yFactor = h / (double) bmp.GetHeight ();
    dc->SetUserScale (xFactor, yFactor);
    //the last parameter enables transparency, used when no X-background
    //pixmap found
    dc->DrawBitmap (bmp, wxCoord (x / xFactor), wxCoord (y / yFactor), true);
    dc->SetUserScale (1, 1);
}

void drawTooltip(wxGCDC* dc, wxString tooltip, simImage* hoveringIcon)
{
    wxPoint iconCenter = hoveringIcon->center();
    int iconHeight = hoveringIcon->img.GetHeight();
    wxCoord* textWidth = new wxCoord();
    wxCoord* textHeight = new wxCoord();
    dc->GetTextExtent(tooltip, textWidth, textHeight);

    int x = iconCenter.x - ((int)*textWidth /2);
    int y = iconHeight +  5;

    dc->DrawText(tooltip, x , y);
}




MyFrame::MyFrame (wxWindow * parent, simSettings set, ImagesArray * array,
		  wxWindowID id, const wxString & title, const wxPoint & pos,
		  const wxSize & size, long style):
wxFrame (parent, id, title, pos, size, style)
{    
    settings = set;
    ImagesList = array;
    markBitmap = NULL;
    appBackground = NULL;
    src_dc = NULL;
    backImage = NULL;
    firstPaint = true;

    blurTimer = new wxTimer (this, ID_blur_Timer);
    blurTimer->Start (settings.BLUR_TIMEOUT);
    hoverTimer = new wxTimer(this, ID_hover_Timer);
    animation = new wxTimer(this, ID_animation);
    /*
    * Menu stuff 
    */
    clickedID = -1;
    middleClicked = false;

    /*
    * End menu stuff 
    */
    disposed = false;

    dragging = false;
    moving = false;
    draggedID = -1;
    draggedStart = 0;

    showTooltip = false;
    
    SetBackgroundStyle(wxBG_STYLE_PAINT); // needed since wx2.9.1 for the autodc in onPaint
}


void
MyFrame::SetBG (wxImage * newImg)
{
  if (appBackground)
    {
      appBackground->Destroy ();
    }
  appBackground = newImg;

}

wxImage *
MyFrame::GetBG ()
{
    return appBackground;
}

void
MyFrame::SetWallpaper (wxBitmap * newBitmap)
{
    if (backImage)
        delete (backImage);
    if (src_dc)
        delete src_dc;

    backImage = newBitmap;
    if (backImage->GetHeight() > 0) { 
        src_dc = new wxMemoryDC(*backImage);
    }
    Refresh(true);
}

wxBitmap *
MyFrame::GetWallpaper ()
{
  return backImage;
}


  
void MyFrame::SetMarkBitmap (wxBitmap * newBmp)
{
  	if (markBitmap)
  	{
  		delete markBitmap;
  	}
  	markBitmap = newBmp;
  	
  }
  wxBitmap * MyFrame::GetMarkBitmap ()
  {
  	return markBitmap;
}

wxMenu* MyFrame::GetPopMenu()
{
    popMenu = new wxMenu;
    popMenu->Append (ID_Settings, _T ("S&ettings"));
    popMenu->Append (ID_Add, _T ("A&dd Launcher"));
    popMenu->AppendSeparator ();
    if (hoveringIcon != None && hoveringIcon->task) {
        popMenu->Append (ID_Keep, _T ("&Keep as Launcher"));
    }
    EditMenuItem = popMenu->Append (ID_Edit, _T ("Ed&it Launcher"));
    DeleteMenuItem = popMenu->Append (ID_Delete, _T ("Dele&te Launcher"));
    popMenu->AppendSeparator ();
    popMenu->Append (ID_About, _T ("&About..."));
    popMenu->Append (ID_Quit, _T ("E&xit"));
    if (hoveringIcon == None || hoveringIcon->task) {
        DeleteMenuItem->Enable (false);
        EditMenuItem->Enable (false);
    }
    return popMenu;
}

void
MyFrame::updateSize ()
{
    int width = (settings.ICONW + settings.SPACER) * (ImagesList->GetCount ()) +
                    settings.LEFT_BORDER + settings.RIGHT_BORDER;
    int height = settings.MAXSIZE + settings.BOTTOM_BORDER;

    SetSize (width, height);
    if (settings.AUTO_POSITION) {
        wxSize sz = wxGetDisplaySize();
        Move(sz.GetWidth() /2 - width /2,GetPosition().y);
    }
}

void
MyFrame::OnMouseMove (wxMouseEvent & event)
{
    hoverTimer->Stop();
    showTooltip = false;
    
    if (middleClicked) {
        if (abs(middleClick.x - event.m_x) > 5 || abs(middleClick.y - event.m_y) > 5) {
            // moving should indicate that the icon was clearly moved and not only accidentally dragged a bit while clicking
            moving = true;
        }
        wxPoint framePos = this->GetScreenPosition ();
        framePos.x += event.m_x - middleClick.x;
        framePos.y += event.m_y - middleClick.y;
        this->Move (framePos);
        return;
    }
    
    if (dragging) {
        if (abs(draggedStart - event.m_x) > 5) {
            // moving should indicate that the icon was clearly moved and not only accidentally dragged a bit while clicking
            moving = true;
        }
        draggedPos.x = event.m_x;
        draggedPos.y = event.m_y;
        Refresh (false);
    }
    
    simImage *img = this->getClickedIcon(event);
    if (img != None && hoveringIcon != img ) {
        OnMouseEnterIcon(event, img);
    }

    if (hoveringIcon != None) {
        hoverTimer->Start(3000, wxTIMER_ONE_SHOT);
    }
}


void
MyFrame::OnMiddleDown (wxMouseEvent & event) {
    middleClick = wxPoint (event.m_x, event.m_y);
    middleClicked = true;
}

void
MyFrame::OnMiddleUp (wxMouseEvent & event) {
    if (! moving) {
        simImage* img = this->getClickedIcon(event);
        if (img) {
            /* process identifier */
            int pid;		
            pid = fork ();
            if (pid < 0) {
                wxDialog dlg (this, -1, wxT ("Damn, could not fork...."));
                dlg.ShowModal ();
            }

            if (pid == 0) {
                system(wx2std(img->link).c_str());
                exit (0);
            }
            
            img->blurStatus = STATUS_INCREASING;

            if (! blurTimer->IsRunning()) {
                blurTimer->Start (settings.BLUR_TIMEOUT);
            }
        } 
    }
    // else "Already moving";
    middleClicked = false;
    moving = false;
}

void
MyFrame::OnRightClick (wxMouseEvent & event)
{
    showTooltip = false;
    wxPoint p = event.GetPosition ();

    clickedID = -1;
    for (unsigned int i = 0; i < ImagesList->GetCount (); i++)
    {
        simImage *img = (*ImagesList)[i];
        if (img->isIn (p.x, p.y))
        {
            clickedID = i;
            break;
        }
    }
    event.Skip ();
    Refresh(false);
}

void
MyFrame::OnContextMenu (wxContextMenuEvent & event)
{
    showTooltip = false;
    hoverTimer->Stop();

    // prevent the menu on being under the mouse-pointer, which lead to
    // activating exit on a single short right-click
    wxPoint relativeCursorPosition = wxPoint();
    relativeCursorPosition.x = wxGetMousePosition().x - this->GetScreenPosition().x;
    relativeCursorPosition.y = wxGetMousePosition().y - this->GetScreenPosition().y;

    relativeCursorPosition.x += 2;
    PopupMenu (MyFrame::GetPopMenu(), relativeCursorPosition);
}

void
MyFrame::OnQuit (wxCommandEvent & WXUNUSED (event))
{
  Close (TRUE);
}

void
MyFrame::OnAbout (wxCommandEvent & WXUNUSED (event))
{
    info = new wxAboutDialogInfo ();
    info->SetVersion (_T (SIMDOCK_VERSION));
    info->SetCopyright (_T
                  (" (C) 2007 Simone Della Longa <simonedll@yahoo.it>\n"
                   " (C) 2011 Malte Paskuda <malte@paskuda.biz>\n"
               "This program is free software; you can redistribute it and/or modify\n"
               "it under the terms of the GNU General Public License as published by\n"
               "the Free Software Foundation; either version 2 of the License, or\n"
               "any later version.\n"));
    info->AddDeveloper (_T("Simone Della Longa (Original Developer)"));
    info->AddDeveloper (_T("Malte Paskuda (Developer)"));
    info->AddDeveloper (_T("Marco Garzola (Contributor)"));

    info->SetName (_T ("SimDock"));
    info->SetWebSite (_T (SIMDOCK_WEBSITE));
    wxGenericAboutBox (*info);
}

void
MyFrame::OnSettings (wxCommandEvent & WXUNUSED (event))
{
    settingsDialog = new SettingsDialog (this, &settings);
    settingsDialog->Show();

    // the setting-window spawns wrongly only on the workspace simdock
    // was started on, so show it now on the current workspace
    GdkWindow * settingsWindow = gtk_widget_get_window(settingsDialog->GetHandle());
    gdk_x11_window_move_to_current_desktop(settingsWindow);
    settingsDialog->ShowModal();
}

void
MyFrame::OnAdd (wxCommandEvent & event)
{
    simImage *sim = new simImage ();
    LauncherDialog *dlg = new LauncherDialog (this, sim);
    if (dlg->ShowModal () == wxID_OK) {
        if (dlg->saveChanges ()) {
            sim->w = settings.ICONW;
            sim->h = settings.ICONH;
            sim->future_w = settings.ICONW;
            sim->future_h = settings.ICONH;
            sim->y = (settings.MAXSIZE + settings.BOTTOM_BORDER) - settings.ICONH - settings.BOTTOM_BORDER;
            ImagesList->Add (sim);
            bool changeIcons[ImagesList->GetCount()];
            fill_n(changeIcons, ImagesList->GetCount(), true);
            appSize = PositionIcons (settings, ImagesList, changeIcons);
            updateSize();

            saveLaunchers(ImagesList);
            Refresh (false);
        } else {
            delete sim;
        }
    } else {
        delete sim;
    }
    dlg->Destroy ();
}

void MyFrame::OnKeep(wxCommandEvent & event) {
    if (clickedID < 0 || (unsigned int) clickedID > ImagesList->GetCount ())
    {
        cout << "Error! invalid ClickedID value! " << clickedID << endl;
        return;
    }

    simImage* newLauncher = (*ImagesList)[clickedID];
    
    taskInfo ti;
    //it is quite possible that TaskInfo cant find the data of the window
    //(e.g. xterm)
	if (ti.Init(newLauncher->getWindow())) {
        newLauncher->link = ti.path;
        wxString home = wxGetHomeDir ();
        wxString dirPath = home + _T ("/") + _T (CONF_DIR);
        wxString iconPath = dirPath + _T ("/") + ti.name + _T(".png");
        ti.icon.SaveFile(iconPath, wxBITMAP_TYPE_PNG);
        newLauncher->img_link = iconPath;
        newLauncher->task = false;
        saveLaunchers(ImagesList);
    } else {
        cout << "Not enough data to create launcher.";
    }
}

void
MyFrame::OnEdit (wxCommandEvent & event)
{
  if (clickedID < 0 || (unsigned int) clickedID > ImagesList->GetCount ())
    {
      cout << "Error! invalid ClickedID value! " << clickedID << endl;
      return;
    }

  LauncherDialog *dlg = new LauncherDialog (this, (*ImagesList)[clickedID]);
  if (dlg->ShowModal () == wxID_OK) {
        dlg->saveChanges ();
        Refresh (false);
        saveLaunchers(ImagesList);
    }

  dlg->Destroy ();

}

void
MyFrame::OnDelete (wxCommandEvent & event)
{
    if (clickedID < 0 || (unsigned int) clickedID > ImagesList->GetCount ())
    {
        cout << "Error! invalid ClickedID value! " << clickedID << endl;
        return;
    }
    simImage* oldLauncher = (*ImagesList)[clickedID];
    if (oldLauncher->windowCount() > 0) {
        oldLauncher->task = true;
    } else {
        delete (*ImagesList)[clickedID];
        ImagesList->RemoveAt (clickedID);
    }
    saveLaunchers(ImagesList);
    updateSize();
    Refresh (false);
}

void
MyFrame::OnMouseLeave (wxMouseEvent & event)
{
#ifdef SIMDOCK_DEBUG
    cout << "OnMouseLeave" << endl;
#endif
    showTooltip = false;
    OnMouseLeaveIcon(event);
    Refresh (false);
    setFutures();
    if (! animation->IsRunning()) {
        animation->Start(16);   // 60 fps
    }
}

void
MyFrame::OnMouseEnter (wxMouseEvent & event)
{
#ifdef SIMDOCK_DEBUG
    cout << "OnmouseEnter" << endl;
#endif

    if (!wxGetApp ().onTop) {
        Refresh (false);
    }
}

//Pseudo-event, called manually
void MyFrame::OnMouseEnterIcon (wxMouseEvent & event, simImage* img)
{
    hoveringIcon = img;
    setFutures();
    if (! animation->IsRunning()) {
        animation->Start(16);   // 60 fps
    }
}

//Pseudo-event, called manually
void MyFrame::OnMouseLeaveIcon (wxMouseEvent & event)
{
    hoveringIcon = None;
    setFutures();
    if (! animation->IsRunning()) {
        animation->Start(16);   // 60 fps
    }
}

void
MyFrame::OnLeftDown (wxMouseEvent & event)
{
    simImage *img = this->getClickedIcon(event);
    if (img != None) {
        draggedID = positionToId(wxPoint (event.m_x, event.m_y), ImagesList, 0,
                            ImagesList->GetCount () - 1);
        draggedPos.x = event.m_x;
        draggedStart = draggedPos.x;
        draggedPos.y = event.m_y;
        dragging = true;
    }
}

simImage*
MyFrame::getClickedIcon(wxMouseEvent & event) {
    for (unsigned int i = 0; i < ImagesList->GetCount (); i++) {
        simImage *img = (*ImagesList)[i];
        if (img->isIn (event.m_x, event.m_y)) {
            return img;
        }
    }
    return None;
}



void
MyFrame::OnLeftUp (wxMouseEvent & event) {
    if (dragging && moving) {
        dragging = false;
        moving = false;
        int id = positionToId (wxPoint (event.m_x, event.m_y), ImagesList, 0,
                                ImagesList->GetCount () - 1);
        if (draggedID != id) {
            simImage *oldImg = (*ImagesList)[draggedID];
            ImagesList->RemoveAt (draggedID);
            ImagesList->Insert (oldImg, id);
            
            saveLaunchers(ImagesList);
            bool changeIcons[ImagesList->GetCount()];
            fill_n(changeIcons, ImagesList->GetCount(), true);
            appSize = PositionIcons (settings, ImagesList, changeIcons);
        }
        Refresh (false);
        return;
    }
    dragging = false;
    moving = false;
    Refresh (false);
    simImage* img = this->getClickedIcon(event);
    if (img != None) {
        if (img->windowCount() > 0) {
            if (settings.ENABLE_MINIMIZE && img->active) {
                if (img->allNotMinimized()) {
                    img->cycleMinimize = true;
                }
                if (img->allMinimized()) {
                    img->cycleMinimize = false;
                }
                
                if (img->cycleMinimize) {
                    tasks_minimize(img->getWindow());
                } else {
                    tasks_raise(img->getWindow());
                }
            } else {
                tasks_raise(img->getWindow());
            }
            return;
        }

        /* process identifier */
        int pid;		

        pid = fork ();
        if (pid < 0) {
            wxDialog dlg (this, -1, wxT ("Damn, could not fork...."));
            dlg.ShowModal ();
        }

        if (pid == 0) {
            exit (system(wx2std(img->link).c_str()));
        }
        
        img->blurStatus = STATUS_INCREASING;

        if (! blurTimer->IsRunning()) {
            blurTimer->Start (settings.BLUR_TIMEOUT);
        }
    }
}

void
MyFrame::OnFrameMove (wxMoveEvent & event)
{
  cout << event.GetPosition ().x << "," << event.GetPosition ().y << endl;
}



#if 0
void
fade (wxImage * img, const int &intensity)
{
  if (intensity == 0)
    return;
  /*
   * Get the full alpha channel and modify it instead of changing every
   * single bit? will it be faster? Look above .. :( 
   */
  unsigned char v;
  for (int i = 0; i < img->GetWidth (); i++)
    {
      for (int j = 0; j < img->GetHeight (); j++)
	{
	  v = img->GetAlpha (i, j);
	  if ((int) v <= intensity)
	    {
	      v = (unsigned char) intensity;
	    }
	  unsigned char v2 = (unsigned char) ((int) v - intensity);
	  img->SetAlpha (i, j, v2);
	}
    }
}
#endif



void
MyFrame::OnClose (wxCloseEvent & event)
{
  lastPosition = GetPosition ();
  disposed = true;
  Destroy ();
}


void
MyFrame::OnBlurTimerTick (wxTimerEvent & event)
{
    bool changed = false;		// Some node is changing status
    for (unsigned int i = 0; i < ImagesList->GetCount (); i++) {
        simImage *img = (*ImagesList)[i];

        switch (img->blurStatus) {
            case STATUS_NONE:
                break;
            default:
                img->handleStatus ();
                changed = true;
                break;
        }
    }
    Refresh(true);
    if (!changed) {
        blurTimer->Stop ();
    }
}

void MyFrame::OnHoverTimerTick(wxTimerEvent & event)
{
    showTooltip = true;
    Refresh(true);
}

void MyFrame::OnAnimationTick(wxTimerEvent & event) {
    if (approachFutures()) {
        animation->Stop();
    }
    Refresh(true);
}

// set icons width and position to the value they shall have in the future, after the animation
void
MyFrame::setFutures() {
    int neededSpace = 0;
    unsigned int imgCount = ImagesList->GetCount();
    int availableSpace = settings.LEFT_BORDER +
                            imgCount * (settings.ICONW + settings.SPACER) +
                            settings.RIGHT_BORDER - settings.SPACER;
                            
    for (unsigned int i = 0; i < imgCount; i++) {
        simImage *img = (*ImagesList)[i];
        if (img == hoveringIcon) {
            img->future_w = zoom(settings.RANGE, 0, settings.MAXSIZE);
        } else {
            img->future_w  = settings.ICONW;
        }
        img->future_h = img->future_w;
        neededSpace += img->future_w + settings.SPACER;
    }
    neededSpace -= settings.SPACER;
    double borderRatio = (double)settings.LEFT_BORDER / (settings.LEFT_BORDER + settings.RIGHT_BORDER);
    int positionX = (availableSpace - neededSpace) * borderRatio;
    
    for (unsigned int i = 0; i < imgCount; i++) {
        simImage *img = (*ImagesList)[i];
        img->future_x = positionX;
        positionX += img->future_w + settings.SPACER;
    }
}


// set icons width and position closer to their future position
// return true if everything was changed
bool
MyFrame::approachFutures() {
    unsigned int imgCount = ImagesList->GetCount();
    bool ready = true;
    int zoomChange = 1;
    if (settings.FAST_ANIMATIONS) {
        zoomChange = 2;
    }
    for (unsigned int i = 0; i < imgCount; i++) {
        simImage *img = (*ImagesList)[i];
        if (img->w > img->future_w) {
            img->w = max(img->w - zoomChange, img->future_w);
            ready = false;
            if (img->h > img->future_h) {
                img->y += img->h - img->w;  // we don't want to set this in different steps than the width, as this leads to hidden images under current X with Mesa 10.4.0-devel
                img->h = img->w;
            }
        } else if (img->w < img->future_w) {
            img->w = min(img->w + zoomChange, img->future_w);
            ready = false;
            if (img->h < img->future_h) {
                img->y -= img->w - img->h;
                img->h = img->w;
            }
        }
        if (img->x > img->future_x) {
            img->x = max(img->x - zoomChange, img->future_x);
            ready = false;
        } else if (img->x < img->future_x) {
            img->x = min(img->x + zoomChange, img->future_x);
            ready = false;
        }
    }
    return ready;
}

void
MyFrame::OnPaint (wxPaintEvent & event) {
    wxAutoBufferedPaintDC dc (this);
    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    wxGCDC* gdc = new wxGCDC(gc);
    wxPoint framePos = this->GetScreenPosition ();
    wxSize sz = GetClientSize ();

    dc.Blit (0, 0, sz.GetWidth (), sz.GetHeight (), src_dc, framePos.x,
	   framePos.y);

    
    drawBmp (gdc, wxBitmap (*appBackground), 0,
	   sz.GetHeight () - settings.BG_HEIGHT, appSize.GetWidth (),
	   settings.BG_HEIGHT);


    for (unsigned int i = 0; i < ImagesList->GetCount (); i++)
    {
        simImage *img = (*ImagesList)[i];

        if (img->blurStatus != STATUS_NONE)
        {

            wxImage wxImage2 (img->img);
            fade (&wxImage2, img->blur * 20);

            wxBitmap bmp (wxImage2);
            drawBmp (gdc, bmp, img->x, img->y, img->w, img->h);
        }
        else
        {
            drawBmp (gdc, wxBitmap (img->img), img->x, img->y, img->w, img->h);
        }
        wxSize shadowSize;
        if (settings.SHOW_REFLEXES)
        {
            wxImage wxImage3 (img->reflex);
            fade (&wxImage3, settings.REFLEX_ALPHA + img->blur * 10);
            shadowSize =
                ImageToShadow (img->w, img->h, settings.REFLEX_SCALING);
            drawBmp (gdc, wxBitmap (wxImage3), img->x, img->y + img->h,
                shadowSize.GetWidth (), shadowSize.GetHeight ());
        }
		if (img->windowCount() > 0)
        {
            drawBmp(gdc,*markBitmap,img->x+img->w/2 -markBitmap->GetWidth()/2,img->y+img->h+shadowSize.GetHeight (), markBitmap->GetWidth(),5);
				
        }
    }


    if (dragging && moving)
    {
      simImage *img = (*ImagesList)[draggedID];
      drawBmp (gdc, wxBitmap (img->img), draggedPos.x,
	       draggedPos.y, settings.ICONW, settings.ICONH);
    }

    if (showTooltip && hoveringIcon != None) {
        wxString tooltip = hoveringIcon->name;
        drawTooltip(gdc, tooltip, hoveringIcon);
    }

    WnckScreen *defaultScreen = wnck_screen_get_default ();
    wnck_screen_force_update (defaultScreen);
    GList *windowz = wnck_screen_get_windows (defaultScreen);

    if (firstPaint) {
        // The X window is only ready after the first paint started, but we need to do this only once
        GtkWidget* widget = GetHandle();
        XID xid = GDK_WINDOW_XWINDOW(widget->window);
        WnckWindow *xWin = wnck_window_get(xid);
        wnck_window_make_above(xWin);
        wnck_window_stick(xWin);
        wnck_window_pin(xWin);
        wnck_window_set_window_type(xWin, WNCK_WINDOW_DOCK);
        wnck_window_set_skip_pager(xWin, true);
        wnck_window_set_skip_tasklist(xWin, true);

        // this helps wxwidgets to match internal and X height. Without setting the height here, the window will jump into form after an application is started
        int xp, yp, widthp, heightp;
        wnck_window_get_geometry(xWin, &xp, &yp, &widthp, &heightp);
        int height = settings.MAXSIZE + settings.BOTTOM_BORDER;
        wnck_window_set_geometry(xWin, WNCK_WINDOW_GRAVITY_CURRENT, WNCK_WINDOW_CHANGE_HEIGHT, xp, yp, widthp, height);
        firstPaint = false;
    }

}
