// DirectShow is based on COM. To write a DirectShow application, you need to understand COM client programming.
// From Robert Laganiere's the DirectShow tutorial at: http://www.laganiere.name/
// "Processing an image sequence", Part 3 of the tutorial (Except I have it all in one file and added pEvent to wait for completion)

#include <atlstr.h>
//#include <stdio.h>
#include <dshow.h>
//#include <streams.h>
//#include <dxutil.h>
#include <vector>
#include <initguid.h>

#include "cv.h"      // include core library interface
#include "highgui.h" // include GUI library interface

#include <iProxyTrans.h>
#include <ProxyTransuids.h>

#include <iostream>

#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }

IplImage *previous= 0;
void process(void* img) {
  IplImage* image = reinterpret_cast<IplImage*>(img);
  cvErode( image, image, 0, 2 );
}

IPin *GetPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir)
{
    BOOL       bFound = FALSE;
    IEnumPins  *pEnum;
    IPin       *pPin;
    IPin       *pPin2;

    pFilter->EnumPins(&pEnum);
    while(pEnum->Next(1, &pPin, 0) == S_OK)
    {
        PIN_DIRECTION PinDirThis;
        pPin->QueryDirection(&PinDirThis);
        if (PinDir == PinDirThis) {

            pPin->ConnectedTo(&pPin2); // is this pin
                                       // connected to another pin ?
            if (pPin2 == 0) {
              bFound= TRUE;
              break;
            }
        }
        pPin->Release();
    }
    
    pEnum->Release();
    return (bFound ? pPin : 0);  
}

IPin *GetPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, int n)
{
    BOOL       bFound = FALSE;
    IEnumPins  *pEnum;
    IPin       *pPin;
    int i=1;

    pFilter->EnumPins(&pEnum);
    while(pEnum->Next(1, &pPin, 0) == S_OK)
    {
        PIN_DIRECTION PinDirThis;
        pPin->QueryDirection(&PinDirThis);
        if (PinDir == PinDirThis) {

            if (i == n) {

              bFound= TRUE;
              break;

            } else {

              i++;
            }
        }
        pPin->Release();
    }
    
    pEnum->Release();
    return (bFound ? pPin : 0);  
}

bool addFilter(REFCLSID filterCLSID, WCHAR* filtername, IGraphBuilder *pGraph,
               IPin **outputPin, int numberOfOutput) {

      // Create the filter.

      IBaseFilter* baseFilter = NULL;
      char tmp[100];

      if(FAILED(CoCreateInstance(
          filterCLSID, NULL, CLSCTX_INPROC_SERVER,
                IID_IBaseFilter, (void**)&baseFilter)) || !baseFilter)
      {
        sprintf(tmp,"Unable to create %ls filter", filtername);
        ::MessageBox( NULL, LPCWSTR(tmp), L"Error", MB_OK | MB_ICONINFORMATION );
        return 0;
      }

      // Obtain the input pin.

      IPin* inputPin= GetPin(baseFilter, PINDIR_INPUT);
      if (!inputPin) {

        sprintf(tmp,"Unable to obtain %ls input pin", filtername);
        ::MessageBox( NULL, LPCWSTR(tmp), L"Error", MB_OK | MB_ICONINFORMATION );
        return 0;
      }

      // Connect the filter to the ouput pin.

      if(FAILED(pGraph->AddFilter( baseFilter, filtername)) ||
         FAILED(pGraph->Connect(*outputPin, inputPin)) )           
      {

        sprintf(tmp,"Unable to connect %ls filter", filtername);
        ::MessageBox( NULL, LPCWSTR(tmp), L"Error", MB_OK | MB_ICONINFORMATION );
        return 0;
      }

      SAFE_RELEASE(inputPin);
      SAFE_RELEASE(*outputPin);

      // Obtain the output pin(s).

      for (int i=0; i<numberOfOutput; i++) {

        outputPin[i]= 0;
        outputPin[i]= GetPin(baseFilter, PINDIR_OUTPUT, i+1);

        if (!outputPin[i]) {

          sprintf(tmp,"Unable to obtain %s output pin (%d)", filtername, i);
          ::MessageBox( NULL, LPCWSTR(tmp), L"Error", MB_OK | MB_ICONINFORMATION );
          return 0;
        }
      }

      SAFE_RELEASE(baseFilter);

      return 1;
}

bool addRenderer(WCHAR* renderername, IGraphBuilder *pGraph, IPin **outputPin) {

      char tmp[100];
      IBaseFilter* pVideo = NULL;

      if(FAILED(CoCreateInstance(CLSID_VideoRenderer, NULL, CLSCTX_INPROC_SERVER,
                IID_IBaseFilter, (void**)&pVideo)) || !pVideo)
      {
        sprintf(tmp,"Unable to create renderer %ls", renderername);
        ::MessageBox( NULL, LPCWSTR(tmp), L"Error", MB_OK | MB_ICONINFORMATION );
        return 0;
      }


      IPin* pVideoIn= GetPin(pVideo, PINDIR_INPUT);
      if (!pVideoIn) {

        sprintf(tmp,"Unable to obtain %ls input pin", renderername);
        ::MessageBox( NULL, LPCWSTR(tmp), L"Error", MB_OK | MB_ICONINFORMATION );
        return 0;
      }
      
      if(FAILED(pGraph->AddFilter( pVideo, renderername)) ||
         FAILED(pGraph->Connect(*outputPin, pVideoIn)) )           
      {
        sprintf(tmp,"Unable to connect %ls renderer", renderername);
        ::MessageBox( NULL, LPCWSTR(tmp), L"Error", MB_OK | MB_ICONINFORMATION );
        return 0;
      }

      SAFE_RELEASE(pVideo);
      SAFE_RELEASE(pVideoIn);
      SAFE_RELEASE(*outputPin);

      return 1;
}

bool addSource(WCHAR* sourcename, IGraphBuilder *pGraph, IPin **outputPin) {

      char tmp[100];
      IBaseFilter*	pSource;

      if(FAILED(pGraph->AddSourceFilter(sourcename,0,&pSource)))
      {
        sprintf(tmp,"Unable to connect to %ls source", sourcename);
        ::MessageBox( NULL, LPCWSTR(tmp), L"Error", MB_OK | MB_ICONINFORMATION );
        return 0;
      }

      *outputPin= 0;
      *outputPin= GetPin(pSource, PINDIR_OUTPUT);

      if (! *outputPin) {

        sprintf(tmp,"Unable to obtain source (%ls) input pin", sourcename);
        ::MessageBox( NULL, LPCWSTR(tmp), L"Error", MB_OK | MB_ICONINFORMATION );
        return 0;
      }

      SAFE_RELEASE(pSource);

      return 1;
}

bool addFileWriter(WCHAR* filename, IGraphBuilder *pGraph, IPin **outputPin) {

      char tmp[100];
      IBaseFilter* pWriter = NULL;
      if(FAILED(CoCreateInstance(CLSID_FileWriter, NULL, CLSCTX_INPROC_SERVER,
                IID_IBaseFilter, (void**)&pWriter)) || !pWriter)
      {
        sprintf(tmp,"Unable to create file writer (%ls)", filename);
        ::MessageBox( NULL, LPCWSTR(tmp), L"Error", MB_OK | MB_ICONINFORMATION );
        return 0;
      }

      IFileSinkFilter2 *psink;
      pWriter->QueryInterface(IID_IFileSinkFilter2, (void **)&psink);

      psink->SetFileName(filename,NULL);
      psink->SetMode(AM_FILE_OVERWRITE);

      IPin* pWriterIn= GetPin(pWriter, PINDIR_INPUT);
      if (!pWriterIn) {

        sprintf(tmp,"Unable to obtain writer input pin (%ls)", filename);
        ::MessageBox( NULL, LPCWSTR(tmp), L"Error", MB_OK | MB_ICONINFORMATION );
        return 0;
      }

      if(FAILED(pGraph->AddFilter( pWriter, filename)) ||
         FAILED(pGraph->Connect(*outputPin, pWriterIn)) )           
      {
        ::MessageBox( NULL, L"Unable to connect writer filter", L"Error", MB_OK | MB_ICONINFORMATION );
        return 0;
      }

      SAFE_RELEASE(pWriter);
      SAFE_RELEASE(pWriterIn);
      SAFE_RELEASE(*outputPin);

      return 1;
}

std::vector<CString> enumFilters(IGraphBuilder *pGraph) {

      IEnumFilters *pEnum = NULL;
      IBaseFilter *pFilter;
      ULONG cFetched;
      std::vector<CString> names;

      pGraph->EnumFilters(&pEnum);
      while(pEnum->Next(1, &pFilter, &cFetched) == S_OK)
      {
        FILTER_INFO FilterInfo;
        char szName[256];
        CString fname;

        pFilter->QueryFilterInfo(&FilterInfo);
        WideCharToMultiByte(CP_ACP, 0, FilterInfo.achName, -1, szName, 256, 0, 0);
        fname= szName;
        names.push_back(fname);

        SAFE_RELEASE(FilterInfo.pGraph);
        SAFE_RELEASE(pFilter);
      }

      SAFE_RELEASE(pEnum);

      return names;
}

int main(void) {
	//printf("hello, world\n");
	std::cout << "hello, world ... press Enter to start directshow processing" << std::endl;
	std::cin.get();
	long evCode;
	
	IGraphBuilder *pGraph;
    IMediaControl *pMediaControl;
	IMediaEvent *pEvent;
    WCHAR* ifilename;
    WCHAR* ofilename;
    ifilename=L"ref_4.avi";
    ofilename=L"zprocessed.avi";

    CoInitialize(NULL);

    pGraph= NULL;
    pMediaControl= NULL;
	pEvent = NULL;
	if (!FAILED(CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC, IID_IGraphBuilder, (void **)&pGraph))) {
        pGraph->QueryInterface(IID_IMediaControl, (void **)&pMediaControl);
		pGraph->QueryInterface(IID_IMediaEvent, (void **)&pEvent);
	}
	
	IPin* pSourceOut[2];
    pSourceOut[0]= pSourceOut[1]= NULL;

	// Video source
    addSource(ifilename, pGraph, pSourceOut);

	// Add the decoding filters
    addFilter(CLSID_AviSplitter, L"Splitter", pGraph, pSourceOut, 1);
    addFilter(CLSID_AVIDec, L"Decoder", pGraph, pSourceOut,1);

    // Insert the first Smart Tee
    addFilter(CLSID_SmartTee, L"SmartTee(1)", pGraph, pSourceOut,2);

    // Add the ProxyTrans filter
	addFilter(CLSID_ProxyTransform, L"ProxyTrans", pGraph, pSourceOut, 1);

    // Set the ProxyTrans callback     
	IBaseFilter* pProxyFilter = NULL;
	IProxyTransform* pProxyTrans = NULL;
    pGraph->FindFilterByName(L"ProxyTrans",&pProxyFilter);
    pProxyFilter->QueryInterface(IID_IProxyTransform, (void**)&pProxyTrans);
    pProxyTrans->set_transform(process, 0);
    SAFE_RELEASE(pProxyTrans);
    SAFE_RELEASE(pProxyFilter);

	// Render the original (decoded) sequence using 2nd SmartTee(1) output pin
    addRenderer(L"Renderer(1)", pGraph, pSourceOut+1);

	// Insert the second Smart Tee
    addFilter(CLSID_SmartTee, L"SmartTee(2)", pGraph, pSourceOut,2);

	// Encode the processed sequence
    addFilter(CLSID_AviDest, L"AVImux", pGraph, pSourceOut, 1);
    addFileWriter(ofilename, pGraph, pSourceOut);

    // Render the transformed sequence using 2nd SmartTee(2) output pin
    addRenderer(L"Renderer(2)", pGraph, pSourceOut+1);
      
	// Run the graph and wait for completion as before	
	pMediaControl->Run(); // Run the graph
	pEvent->WaitForCompletion(INFINITE, &evCode); // Wait for completion

    SAFE_RELEASE(pMediaControl);
	SAFE_RELEASE(pEvent);
    SAFE_RELEASE(pGraph);
	
    CoUninitialize();
    //ofilename=NULL;
    //ifilename=NULL;
	return 1;
}