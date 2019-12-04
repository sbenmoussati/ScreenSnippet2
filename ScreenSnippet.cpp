#ifndef UNICODE
	#define UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>

#pragma comment( lib, "user32.lib" )
#pragma comment( lib, "gdi32.lib" )
#pragma comment( lib, "gdiplus.lib" )

#define WINDOW_CLASS_NAME L"SymphonyScreenSnippetTool"

static struct {
	char const* language;
	wchar_t const* done;
	wchar_t const* erase;
	wchar_t const* highlight;
	wchar_t const* pen;
	wchar_t const* title;
} localization[] {
	{ "en-US", L"Done", L"Erase", L"Highlight", L"Pen", L"Snipping Tool", },
	{ "fr-FR", L"Terminé", L"Effacer", L"Surligner", L"Stylo", L"Outil Capture", },
	{ "ja-JP", L"完了", L"消去する", L"ハイライト", L"ペン", L"タイトル", },
};


struct Window {
	HWND hwnd;
	HDC backbuffer;
	POINT prevTopLeft;	
	POINT prevBottomRight;	
};


struct SelectRegionData {
	HPEN pen;	
	HPEN eraser;	
	HBRUSH background;
	HBRUSH transparent;
	POINT topLeft;	
	POINT bottomRight;	
	BOOL dragging;
	BOOL done;
	BOOL aborted;
	RECT region;
	int windowCount;
	Window windows[ 256 ];
};


void swap( int* a, int* b ) {
	int t = *a;
	*a = *b;
	*b = t;
}


void swap( LONG* a, LONG* b ) {
	LONG t = *a;
	*a = *b;
	*b = t;
}


void drawRect( HWND hwnd, HDC dc, POINT a, POINT b ) {
	ScreenToClient( hwnd, &a );
	ScreenToClient( hwnd, &b );
	BeginPath( dc );
	MoveToEx( dc, a.x, a.y, NULL);
	LineTo( dc, b.x, a.y );
	LineTo( dc, b.x, b.y );
	LineTo( dc, a.x, b.y );
	LineTo( dc, a.x, a.y );
	EndPath( dc );
}


static void CALLBACK updateMouseTimerProc( HWND hwnd, UINT message, UINT_PTR id, DWORD ms ) {
	POINT p;
	GetCursorPos( &p );

	struct SelectRegionData* selectRegionData = (struct SelectRegionData*) id;

	if( (GetAsyncKeyState( VK_ESCAPE ) & 0x8000 ) != 0 ) {
		selectRegionData->aborted = TRUE;
	}

	if( selectRegionData->dragging ) {
		selectRegionData->bottomRight.x = p.x;
		selectRegionData->bottomRight.y = p.y;
		for( int i = 0; i < selectRegionData->windowCount; ++i ) {
			InvalidateRect( selectRegionData->windows[ i ].hwnd, NULL, FALSE );
		}
	}

	if( selectRegionData->dragging && ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) == 0 ) {
		selectRegionData->dragging = FALSE;
		selectRegionData->done = TRUE;
		POINT topLeft = selectRegionData->topLeft;
		POINT bottomRight = selectRegionData->bottomRight;
		selectRegionData->region.left = topLeft.x;
		selectRegionData->region.top = topLeft.y;
		selectRegionData->region.right = bottomRight.x;
		selectRegionData->region.bottom = bottomRight.y;
		for( int i = 0; i < selectRegionData->windowCount; ++i ) {
			InvalidateRect( selectRegionData->windows[ i ].hwnd, NULL, FALSE );
		}
	}

	SetTimer( hwnd, (UINT_PTR)selectRegionData, 16, updateMouseTimerProc );
}


static LRESULT CALLBACK selectRegionWndProc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	struct SelectRegionData* selectRegionData = (struct SelectRegionData*) GetWindowLongPtrA( hwnd, GWLP_USERDATA );
	switch( message ) {
		case WM_ERASEBKGND: {
			if( selectRegionData && selectRegionData->dragging ) {
				return 1;
			}
		} break;
		case WM_PAINT: {
			struct Window* window = NULL;
			int index = 0;
			for( int i = 0; i < selectRegionData->windowCount; ++i ) {
				if( selectRegionData->windows[ i ].hwnd == hwnd ) {
					window = &selectRegionData->windows[ i ];
					break;
				}
			}
			if( window ) {
				// erase previous rect
				drawRect( hwnd, window->backbuffer, window->prevTopLeft, window->prevBottomRight );
				SelectObject( window->backbuffer, selectRegionData->eraser );
				SelectObject( window->backbuffer, selectRegionData->background );
				StrokeAndFillPath( window->backbuffer );
			
				// draw current rect
				drawRect( hwnd, window->backbuffer, selectRegionData->topLeft, selectRegionData->bottomRight );
				SelectObject( window->backbuffer, selectRegionData->pen );
				SelectObject( window->backbuffer, selectRegionData->transparent );
				StrokeAndFillPath( window->backbuffer );

				POINT cp1 = selectRegionData->topLeft;
				POINT cp2 = selectRegionData->bottomRight;
				if( cp1.x > cp2.x ) {
					swap( &cp1.x, &cp2.x );
				}
				if( cp1.y > cp2.y ) {
					swap( &cp1.y, &cp2.y );
				}
				ScreenToClient( hwnd, &cp1 );
				ScreenToClient( hwnd, &cp2 );
				RECT curr = { cp1.x, cp1.y, cp2.x, cp2.y };
				POINT pp1 = window->prevTopLeft;
				POINT pp2 = window->prevBottomRight;
				if( pp1.x > pp2.x ) {
					swap( &pp1.x, &pp2.x );
				}
				if( pp1.y > pp2.y ) {
					swap( &pp1.y, &pp2.y );
				}
				ScreenToClient( hwnd, &pp1 );
				ScreenToClient( hwnd, &pp2 );
				RECT prev = { pp1.x, pp1.y, pp2.x, pp2.y };
				RECT frame;
				UnionRect( &frame, &curr, &prev );			

				RECT client;
				GetClientRect( hwnd, &client );

				RECT bounds;
				IntersectRect( &bounds, &frame, &client );
				InflateRect( &bounds, 4, 4 );
			
				window->prevTopLeft = selectRegionData->topLeft;
				window->prevBottomRight = selectRegionData->bottomRight;

				PAINTSTRUCT ps; 
				HDC dc = BeginPaint( hwnd, &ps );
				BitBlt( dc, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, 
					window->backbuffer, bounds.left, bounds.top, SRCCOPY );
				EndPaint( hwnd, &ps );
			}
		} break;
		case WM_LBUTTONDOWN: {
			POINT p = { GET_X_LPARAM( lparam ), GET_Y_LPARAM( lparam ) };
			ClientToScreen( hwnd, &p );
			selectRegionData->topLeft.x = p.x;
			selectRegionData->topLeft.y = p.y; 
			selectRegionData->bottomRight.x = p.x;
			selectRegionData->bottomRight.y = p.y;
			selectRegionData->dragging = TRUE;
			InvalidateRect( hwnd, NULL, FALSE );
		} break;
	}
    return DefWindowProc( hwnd, message, wparam, lparam);
}

struct FindScreensData {
	int count;
	RECT bounds[ 256 ];
};


static BOOL CALLBACK findScreens( HMONITOR monitor, HDC dc, LPRECT rect, LPARAM lparam ) {	
	struct FindScreensData* findScreensData = (struct FindScreensData*) lparam;
	findScreensData->bounds[ findScreensData->count++ ] = *rect;
	return TRUE;
}


int selectRegion( RECT* region ) {
	struct FindScreensData findScreensData = { 0 };
	EnumDisplayMonitors( NULL, NULL, findScreens, (LPARAM) &findScreensData );
	if( findScreensData.count <= 0 ) {
		return EXIT_FAILURE;
	}

	COLORREF frame = RGB( 255, 255, 255 );
	COLORREF transparent = RGB( 0, 255, 0 );
	COLORREF background = RGB( 0, 0, 0 );

	struct SelectRegionData selectRegionData = { 
		CreatePen( PS_SOLID, 2, frame ),
		CreatePen( PS_SOLID, 2, background ),
		CreateSolidBrush( background ),
		CreateSolidBrush( transparent ),
	};


	WNDCLASSW wc = { 
		CS_OWNDC | CS_HREDRAW | CS_VREDRAW, // style
		(WNDPROC) selectRegionWndProc, 		// lpfnWndProc
		0, 									// cbClsExtra
		0, 									// cbWndExtra
		GetModuleHandleA( NULL ), 			// hInstance
		NULL, 								// hIcon;
		LoadCursor( NULL, IDC_CROSS ), 		// hCursor
		selectRegionData.background,  		// hbrBackground
		NULL, 								// lpszMenuName
		WINDOW_CLASS_NAME 					// lpszClassName
	};
    RegisterClassW( &wc );


	int count = findScreensData.count;
	HWND hwnd[ 256 ] = { NULL };
	selectRegionData.windowCount = count;
	for( int i = 0; i < count; ++i ) {
		struct Window* window = &selectRegionData.windows[ i ];
		RECT bounds = findScreensData.bounds[ i ];
		hwnd[ i ] = window->hwnd = CreateWindowExW( WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, wc.lpszClassName, 
			NULL, WS_VISIBLE, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, 
			NULL, NULL, GetModuleHandleA( NULL ), 0 );
		

		HDC dc = GetDC( window->hwnd );
		HBITMAP backbuffer = CreateCompatibleBitmap( dc, bounds.right - bounds.left, bounds.bottom - bounds.top );
		window->backbuffer = CreateCompatibleDC( dc );
		SelectObject( window->backbuffer, backbuffer );
		ReleaseDC( window->hwnd, dc );

		SetWindowLongPtrA( hwnd[ i ], GWLP_USERDATA, (LONG_PTR)&selectRegionData );
		SetWindowLongA(hwnd[ i ], GWL_STYLE, WS_VISIBLE );
		SetLayeredWindowAttributes(hwnd[ i ], transparent, 100, LWA_ALPHA | LWA_COLORKEY);
		UpdateWindow( hwnd[ i ] );
	}

	updateMouseTimerProc( hwnd[ 0 ], 0, (UINT_PTR)&selectRegionData, 0 );

	int running = count;
	while( running && !selectRegionData.done && !selectRegionData.aborted )  {
		for( int i = 0; i < count; ++i ) {
			if( hwnd[ i ] ) {
				MSG msg = { NULL };
				while( PeekMessageA( &msg, hwnd[ i ], 0, 0, PM_REMOVE ) ) {
					if( msg.message == WM_CLOSE ) {
						DestroyWindow( hwnd[ i ] );
						hwnd[ i ] = NULL;
						--running;
					}
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				Sleep( 10 );
			}
		}
	}

	for( int i = 0; i < count; ++i ) {
		if( hwnd[ i ] ) {
			DestroyWindow( hwnd[ i ] );
		}
	}

	DeleteObject( selectRegionData.pen );
	DeleteObject( selectRegionData.eraser );
	DeleteObject( selectRegionData.background );
	DeleteObject( selectRegionData.transparent );
	UnregisterClassW( wc.lpszClassName, GetModuleHandleW( NULL ) );
	
	if( selectRegionData.aborted ) {
		return EXIT_FAILURE;
	}

	*region = selectRegionData.region;
	return EXIT_SUCCESS;
}


struct Stroke {
	BOOL highlighter;
	int penIndex;
	int capacity;
	int count;
	POINT* points;
};


struct MakeAnnotationsData {
	RECT bounds;
	Gdiplus::Pen** pens;	
	Gdiplus::Pen** highlighters;	
	Gdiplus::Pen* penEraser;
	Gdiplus::Pen* highlightEraser;
	HDC backbuffer;
	HDC snippet;
	BOOL highlighter;
	int penIndex;
	int highlightIndex;
	BOOL eraser;
	BOOL penDown;
	int strokeCount;
	struct Stroke strokes[ 256 ];
	HWND penButton;
	HWND highlightButton;
	HWND eraseButton;
	HWND doneButton;
	HMENU penMenu;
	HMENU highlightMenu;
    BOOL completed;
};


void newStroke( struct MakeAnnotationsData* data, BOOL highlighter, int penIndex ) {
	if( data->strokeCount < sizeof( data->strokes ) / sizeof( *data->strokes ) ) {
		struct Stroke* stroke = &data->strokes[ data->strokeCount ];
		stroke->highlighter = highlighter;
		stroke->penIndex = penIndex;
		stroke->capacity = 256;
		stroke->count = 0;
		stroke->points = (POINT*) malloc( sizeof( POINT ) * stroke->capacity );
		++data->strokeCount;
	}
}


void addStrokePoint( struct MakeAnnotationsData* data, POINT* p, BOOL force ) {
	if( data->strokeCount > 0 ) {
		struct Stroke* stroke = &data->strokes[ data->strokeCount - 1 ];
		if( stroke->count > 0 ) {
			int dx = stroke->points[ stroke->count - 1 ].x - p->x;
			int dy = stroke->points[ stroke->count - 1 ].y - p->y;
			int dist = (int)sqrtf( (float)( dx * dx + dy * dy ) );
			if( dist < ( force ? 5 : ( data->highlighter ? 30 : 15 ) ) ) {
				return;
			}
		}

		if( stroke->count >= stroke->capacity ) {
			stroke->capacity *= 2;
			stroke->points = (POINT*) realloc( stroke->points, sizeof( POINT ) * stroke->capacity );
		}
		stroke->points[ stroke->count++ ] = *p;
	}
}


static LRESULT CALLBACK makeAnnotationsWndProc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	struct MakeAnnotationsData* data = (struct MakeAnnotationsData*) GetWindowLongPtrA( hwnd, GWLP_USERDATA );
	switch( message ) {
		case WM_COMMAND: {
			if( HIWORD( wparam ) == BN_CLICKED ) {
				if( (HWND) lparam == data->doneButton ) {
                    data->completed = TRUE;
					RECT bounds = data->bounds;
					BitBlt( data->snippet, bounds.left, bounds.top, bounds.right - bounds.left, 
						bounds.bottom - bounds.top, data->backbuffer, 0, 0, SRCCOPY );
					PostQuitMessage( 0 );
				}
				if( (HWND) lparam == data->penButton ) {
					RECT bounds;
					GetWindowRect( data->penButton, &bounds );
					POINT p = { bounds.left, bounds.bottom };
					DWORD item = TrackPopupMenu( data->penMenu, TPM_RETURNCMD, p.x, p.y, 0, hwnd, NULL );				
					if( item > 0 ) {
						data->penIndex = item - 1;
					}
					data->highlighter = FALSE;
					data->eraser = FALSE;
					SetCursor( LoadCursor( NULL, IDC_CROSS ) );
				}
				if( (HWND) lparam == data->highlightButton ) {
					RECT bounds;
					GetWindowRect( data->highlightButton, &bounds );
					POINT p = { bounds.left, bounds.bottom };
					DWORD item = TrackPopupMenu( data->highlightMenu,  TPM_RETURNCMD, p.x, p.y, 0, hwnd, NULL );				
					if( item > 0 ) {
						data->highlightIndex = item - 1;
					}
					data->highlighter = TRUE;
					data->eraser = FALSE;
					SetCursor( LoadCursor( NULL, IDC_CROSS ) );
				}
				if( (HWND) lparam == data->eraseButton ) {
					data->penDown = FALSE;
					data->eraser = TRUE;
					SetCursor( LoadCursor( NULL, IDC_NO ) );
				}
			}
		} break;
		case WM_SETCURSOR: {
			if( data->eraser ) {
				SetCursor( LoadCursor( NULL, IDC_NO ) );
			} else {
				SetCursor( LoadCursor( NULL, IDC_CROSS ) );
			}
		} break;
			
		case WM_CLOSE: {
			PostQuitMessage( 0 );
		} break;
		case WM_ERASEBKGND: {
			if( data ) {
				return 1;
			}
		} break;
		case WM_SIZE: {
			HDC dc = GetDC( hwnd );
			RECT r;
			GetClientRect( hwnd, &r  );
			FillRect( dc, &r, GetStockBrush( LTGRAY_BRUSH ) );
			ReleaseDC( hwnd, dc );
		} break;
		case WM_PAINT: {
			RECT bounds = data->bounds;
			HDC backbuffer = data->backbuffer;
			BitBlt( backbuffer, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, 
				data->snippet, 0, 0, SRCCOPY );

			Gdiplus::Graphics graphics( backbuffer );
			graphics.SetSmoothingMode( Gdiplus::SmoothingModeHighQuality );

			for( int i = 0; i < data->strokeCount; ++i ) {
				struct Stroke* stroke = &data->strokes[ i ];
				if( stroke->count > 1 ) {			
					Gdiplus::Point* points = new Gdiplus::Point[ stroke->count ];
					for( int j = 0; j < stroke->count; ++j ) {
						points[ j ].X = stroke->points[ j ].x;
						points[ j ].Y = stroke->points[ j ].y;
					}
					Gdiplus::Pen* pen = stroke->highlighter ? 
						data->highlighters[ stroke->penIndex ] : 
						data->pens[ stroke->penIndex ];
					graphics.DrawCurve( pen, points, stroke->count );
				}
			}

			PAINTSTRUCT ps; 
			HDC dc = BeginPaint( hwnd, &ps );
			BitBlt( dc, bounds.left, bounds.top + 50, bounds.right - bounds.left, bounds.bottom - bounds.top, 
				backbuffer, 0, 0, SRCCOPY );
			EndPaint( hwnd, &ps );
		} break;
		case WM_LBUTTONDOWN: {
			if( !data->eraser ) {
				data->penDown = TRUE;
				newStroke( data, data->highlighter, 
					data->highlighter ? data->highlightIndex : data->penIndex );
				POINT p = { GET_X_LPARAM( lparam ), GET_Y_LPARAM( lparam ) - 50 };
				addStrokePoint( data, &p, TRUE );
				InvalidateRect( hwnd, NULL, FALSE );
			    break; // If we are in 'eraser' mode, fall through into the "RBUTTONDOWN" eraser code below
			}
		}
		case WM_RBUTTONDOWN: {
			Gdiplus::Point p = { GET_X_LPARAM( lparam ), GET_Y_LPARAM( lparam ) - 50 };
			HDC backbuffer = data->backbuffer;
			Gdiplus::Graphics graphics( backbuffer );
			graphics.SetSmoothingMode( Gdiplus::SmoothingModeHighQuality );
			if( data->strokeCount > 0 ) {
				for( int i = data->strokeCount - 1; i >= 0 ; --i ) {
					struct Stroke* stroke = &data->strokes[ i ];
					if( stroke->count > 1 ) {			
						Gdiplus::Point* points = new Gdiplus::Point[ stroke->count ];
						for( int j = 0; j < stroke->count; ++j ) {
							points[ j ].X = stroke->points[ j ].x;
							points[ j ].Y = stroke->points[ j ].y;
						}
						Gdiplus::Pen* pen = stroke->highlighter ? 
							data->highlightEraser : 
							data->penEraser;
						Gdiplus::GraphicsPath path;
						path.AddCurve( points, stroke->count );
						if( path.IsOutlineVisible( p, pen, &graphics ) ) {
							stroke->count = 0;
							InvalidateRect( hwnd, NULL, TRUE );
							//break; // Only break if we want to limit erasing to one shape at a time
						}
					}
				}
			}


		} break;
		case WM_LBUTTONUP: {
			if( data->penDown ) {
				data->penDown = FALSE;
				POINT p = { GET_X_LPARAM( lparam ), GET_Y_LPARAM( lparam ) - 50 };
				addStrokePoint( data, &p, TRUE );
				InvalidateRect( hwnd, NULL, FALSE );
			}
		} break;
		case WM_MOUSEMOVE: {
			if( data->penDown ) {
				POINT p = { GET_X_LPARAM( lparam ), GET_Y_LPARAM( lparam ) - 50 };
				addStrokePoint( data, &p, FALSE );
				InvalidateRect( hwnd, NULL, FALSE );
			}
		} break;
	}
    return DefWindowProc( hwnd, message, wparam, lparam);
}


HBITMAP WINAPI penIcon( Gdiplus::Pen* pen ) 
{ 
	int const width = 120;
	int const height = 20; 
	HWND hwndDesktop = GetDesktopWindow(); 
    HDC hdcDesktop = GetDC(hwndDesktop); 
    HDC hdcMem = CreateCompatibleDC(hdcDesktop); 
    COLORREF clrMenu = GetSysColor(COLOR_MENU); 
    HBRUSH hbrOld; 
    HBITMAP hbmOld; 
	HBITMAP paHbm;
 
     // Create a brush using the menu background color, 
     // and select it into the memory DC. 
 
    hbrOld = (HBRUSH)SelectObject(hdcMem, CreateSolidBrush(clrMenu)); 
 
     // Create the bitmaps. Select each one into the memory 
     // DC that was created and draw in it. 
 
    // Create the bitmap and select it into the DC. 
 
    paHbm = CreateCompatibleBitmap(hdcDesktop, width, height); 
    hbmOld = (HBITMAP) SelectObject(hdcMem, paHbm); 
 

    PatBlt(hdcMem, 0, 0, width, height, PATCOPY); 
	Gdiplus::Graphics graphics( hdcMem );
	graphics.SetSmoothingMode( Gdiplus::SmoothingModeHighQuality );
	graphics.DrawLine( pen, Gdiplus::Point( 20, height / 2 ), Gdiplus::Point( width, height / 2 ) );

    SelectObject(hdcMem, hbmOld); 
 
    // Delete the brush and select the original brush. 
 
    DeleteObject(SelectObject(hdcMem, hbrOld)); 
 
    // Delete the memory DC and release the desktop DC. 
 
    DeleteDC(hdcMem); 
    ReleaseDC(hwndDesktop, hdcDesktop); 
	return paHbm;
} 


int makeAnnotations( HBITMAP snippet, RECT bounds, int lang ) {
    // save bitmap to clipboard
    OpenClipboard(NULL);
    EmptyClipboard();
    SetClipboardData(CF_BITMAP, snippet);
    CloseClipboard();

	WNDCLASSW wc = { 
		CS_OWNDC | CS_HREDRAW | CS_VREDRAW,		// style
		(WNDPROC) makeAnnotationsWndProc, 		// lpfnWndProc
		0, 										// cbClsExtra
		0, 										// cbWndExtra
		GetModuleHandleA( NULL ), 				// hInstance
		NULL, 									// hIcon;
		NULL, 									// hCursor
		(HBRUSH) GetStockBrush( LTGRAY_BRUSH ),	// hbrBackground
		NULL, 									// lpszMenuName
		WINDOW_CLASS_NAME 						// lpszClassName
	};
    RegisterClassW( &wc );

	HWND hwnd = CreateWindowW( wc.lpszClassName, 
		localization[ lang ].title, WS_VISIBLE | WS_OVERLAPPEDWINDOW, 
		CW_USEDEFAULT, CW_USEDEFAULT, max( 600, bounds.right - bounds.left ) + 50 , bounds.bottom - bounds.top + 150,
		NULL, NULL, GetModuleHandleW( NULL ), 0 );

	COLORREF background = RGB( 0, 0, 0 );

	Gdiplus::Pen* pens[] = {
		new Gdiplus::Pen( Gdiplus::Color( 255, 0, 0, 0 ), 5 ),
		new Gdiplus::Pen( Gdiplus::Color( 255, 0, 0, 255 ), 5 ),
		new Gdiplus::Pen( Gdiplus::Color( 255, 255, 0, 0 ), 5 ),
		new Gdiplus::Pen( Gdiplus::Color( 255, 0, 128, 0 ), 5 ),
	};

	Gdiplus::Pen* highlights[] = {
		new Gdiplus::Pen( Gdiplus::Color( 128, 255, 255, 64 ), 34 ),
		new Gdiplus::Pen( Gdiplus::Color( 128, 150, 100, 150), 34 )
	};

	Gdiplus::Pen* penEraser = new Gdiplus::Pen( Gdiplus::Color( 0, 0, 0, 0), 25 );
	Gdiplus::Pen* highlightEraser = new Gdiplus::Pen( Gdiplus::Color( 0, 0, 0, 0), 50 );

	struct MakeAnnotationsData makeAnnotationsData = {		
		bounds,
		pens,
		highlights,
		penEraser,
		highlightEraser,
	};


	HMENU penMenu = CreatePopupMenu();
	MENUITEMINFOA penItems[] =  { 
		{ sizeof( *penItems ), MIIM_ID | MIIM_BITMAP | MIIM_DATA, 0, 0, 1, 0, 0, 0, 0, 0, 0, penIcon( pens[ 0 ] ) },
		{ sizeof( *penItems ), MIIM_ID | MIIM_BITMAP | MIIM_DATA, 0, 0, 2, 0, 0, 0, 0, 0, 0, penIcon( pens[ 1 ] ) },
		{ sizeof( *penItems ), MIIM_ID | MIIM_BITMAP | MIIM_DATA, 0, 0, 3, 0, 0, 0, 0, 0, 0, penIcon( pens[ 2 ] ) },
		{ sizeof( *penItems ), MIIM_ID | MIIM_BITMAP | MIIM_DATA, 0, 0, 4, 0, 0, 0, 0, 0, 0, penIcon( pens[ 3 ] ) },
	};
	for( int i = 0; i < sizeof( penItems ) / sizeof( *penItems ); ++i ) {
		InsertMenuItemA( penMenu, i, TRUE, &penItems[ i ] );
	}
	makeAnnotationsData.penMenu = penMenu;

	HMENU highlightMenu = CreatePopupMenu();
	MENUITEMINFOA highlightItems[] =  { 
		{ sizeof( *penItems ), MIIM_ID | MIIM_BITMAP | MIIM_DATA, 0, 0, 1, 0, 0, 0, 0, 0, 0, penIcon( highlights[ 0 ] ) },
		{ sizeof( *penItems ), MIIM_ID | MIIM_BITMAP | MIIM_DATA, 0, 0, 2, 0, 0, 0, 0, 0, 0, penIcon( highlights[ 1 ] ) },
	};
	for( int i = 0; i < sizeof( highlightItems ) / sizeof( *highlightItems ); ++i ) {
		InsertMenuItemA( highlightMenu, i, TRUE, &highlightItems[ i ] );
	}
	makeAnnotationsData.highlightMenu = highlightMenu;

	makeAnnotationsData.penButton = CreateWindowW( L"BUTTON", localization[ lang ].pen,
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT,
		7, 10, 113, 30, hwnd, NULL, GetModuleHandleW( NULL ), NULL );

	makeAnnotationsData.highlightButton = CreateWindowW( L"BUTTON", localization[ lang ].highlight,
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT,
		126, 10, 113, 30, hwnd, NULL, GetModuleHandleW( NULL ), NULL );

	makeAnnotationsData.eraseButton = CreateWindowW( L"BUTTON", localization[ lang ].erase,
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT,
		244, 10, 113, 30, hwnd, NULL, GetModuleHandleW( NULL ), NULL );

	makeAnnotationsData.doneButton = CreateWindowW( L"BUTTON", localization[ lang ].done,
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT,
		375, 10, 113, 30, hwnd, NULL, GetModuleHandleW( NULL ), NULL );

	SetWindowLongPtrA( hwnd, GWLP_USERDATA, (LONG_PTR)&makeAnnotationsData );
	UpdateWindow( hwnd );

	HDC dc = GetDC( hwnd );
	HBITMAP backbuffer = CreateCompatibleBitmap( dc, bounds.right - bounds.left, bounds.bottom - bounds.top );
	makeAnnotationsData.backbuffer = CreateCompatibleDC( dc );
	SelectObject( makeAnnotationsData.backbuffer, backbuffer );
	makeAnnotationsData.snippet = CreateCompatibleDC( dc );
	SelectObject( makeAnnotationsData.snippet, snippet );
	ReleaseDC( hwnd, dc );
	InvalidateRect( hwnd, NULL, TRUE );
	

	MSG msg = { NULL };
	while( GetMessage( &msg, NULL, 0, 0 ) ) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	delete penEraser;
	delete highlightEraser;
	for( int i = 0; i < sizeof( pens ) / sizeof( *pens ); ++i ) {
		delete pens[ i ];
	}

	for( int i = 0; i < sizeof( highlights ) / sizeof( *highlights ); ++i ) {
		delete highlights[ i ];
	}

	DestroyWindow( hwnd );
	DeleteObject( makeAnnotationsData.snippet );
	DeleteObject( makeAnnotationsData.backbuffer );
	DeleteObject( backbuffer );

	UnregisterClassW( wc.lpszClassName, GetModuleHandleW( NULL ) );

    if( !makeAnnotationsData.completed ) {
        return EXIT_FAILURE;
    }
    
	return EXIT_SUCCESS;
}


HBITMAP grabSnippet( POINT a, POINT b ) {
    // copy screen to bitmap
    HDC     hScreen = GetDC(NULL);
    HDC     hDC     = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, abs(b.x-a.x), abs(b.y-a.y));
    HGDIOBJ old_obj = SelectObject(hDC, hBitmap);
    BOOL    bRet    = BitBlt(hDC, 0, 0, abs(b.x-a.x), abs(b.y-a.y), hScreen, a.x, a.y, SRCCOPY);
 

    // clean up
    SelectObject(hDC, old_obj);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);

	return hBitmap;
}


namespace Gdiplus {

	int GetEncoderClsid( const WCHAR* format, CLSID* pClsid ) {
		UINT  num = 0;          // number of image encoders
		UINT  size = 0;         // size of the image encoder array in bytes

		ImageCodecInfo* pImageCodecInfo = NULL;

		GetImageEncodersSize( &num, &size );
		if ( size == 0 ) {
			return -1;  // Failure
		}

		pImageCodecInfo = (ImageCodecInfo*)( malloc( size ) );
		if ( pImageCodecInfo == NULL ) {
			return -1;  // Failure
		}

		GetImageEncoders( num, size, pImageCodecInfo );

		for ( UINT j = 0; j < num; ++j ) {
			if ( wcscmp( pImageCodecInfo[j].MimeType, format ) == 0 ) {
				*pClsid = pImageCodecInfo[j].Clsid;
				free( pImageCodecInfo );
				return j;  // Success
			}
		}

		free( pImageCodecInfo );
		return -1;  // Failure
	}

}


int main( int argc, char* argv[] ) {
	FreeConsole();
	SetProcessDPIAware();

	int lang = 0;
	if( argc == 3 ) {
		char const* lang_str = argv[ 2 ];
		for( int i = 0; i < sizeof( localization ) / sizeof( *localization ); ++i ) {
			if( _stricmp( localization[ i ].language, lang_str ) == 0 ) {
				lang = i;
				break;
			}
		}
	}

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	RECT region;
	if( selectRegion( &region ) == EXIT_SUCCESS ) {
		POINT topLeft = { region.left, region.top };
		POINT bottomRight = { region.right, region.bottom };
		if( topLeft.x > bottomRight.x ) {
			swap( &topLeft.x, &bottomRight.x );
		}
		if( topLeft.y > bottomRight.y ) {
			swap( &topLeft.y, &bottomRight.y );
		}
		if( bottomRight.x - topLeft.x < 32 ) {
			bottomRight.x = topLeft.x + 32;
		}
		if( bottomRight.y - topLeft.y < 32 ) {
			bottomRight.y = topLeft.y + 32;
		}
		HBITMAP snippet = grabSnippet( topLeft, bottomRight );
		RECT bounds = { 0, 0, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y };
		int result = makeAnnotations( snippet, bounds, lang );
		if( result == EXIT_SUCCESS ) {
			Gdiplus::Bitmap bmp( snippet, (HPALETTE)0 );
			CLSID pngClsid;
			Gdiplus::GetEncoderClsid( L"image/png", &pngClsid );
			wchar_t* filename = 0;
			if( argc > 1 ) {
			    size_t len = strlen( argv[ 1 ] );
			    filename = new wchar_t[ len + 1 ];
			    mbstowcs_s( 0, filename, len + 1, argv[ 1 ], len );
			}
			bmp.Save( filename ? filename : L"image.png", &pngClsid, NULL );
			if( filename ) {
			    delete[] filename;
			}
        }

		DeleteObject( snippet );
		return EXIT_SUCCESS;
	}
	return EXIT_SUCCESS;
}