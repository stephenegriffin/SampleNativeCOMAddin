/*!-----------------------------------------------------------------------
	connect.cpp

	The main implementation of the addin. It includes the implementation
	for IDTExtensibility2, IRibbonExtensibility, ICustomTaskPaneConsumer,
	and FormRegionStartup.
-----------------------------------------------------------------------!*/
#include "connect.h"
#include "formregionwrapper.h"
#include "MAPIx.h"
#include "MAPIAux.h"
#include "MAPI\Defaults.h"
#include <string>

using std::wstring;

/*!-----------------------------------------------------------------------
	CConnect implementation
-----------------------------------------------------------------------!*/

_ATL_FUNC_INFO CConnect::OptionsPagesAddInfo = { CC_STDCALL, VT_EMPTY, 1, {VT_DISPATCH} };
_ATL_FUNC_INFO CConnect::MapiLogonCompleteInfo = { CC_STDCALL, VT_EMPTY, 0 };
_ATL_FUNC_INFO CConnect::FolderSwitchInfo = { CC_STDCALL, VT_EMPTY, 0, 0 };
_ATL_FUNC_INFO CConnect::OnCloseInfo = { CC_STDCALL, VT_EMPTY, 0, 0 };
_ATL_FUNC_INFO CConnect::ItemSendInfo = { CC_STDCALL, VT_EMPTY, 2,{ VT_DISPATCH, VT_BOOL | VT_BYREF } };

CConnect::CConnect()
{
	m_bMAPIInitialized = false;
}

void TestMAPI(wstring caller, bool bOnline)
{
	LPMAPISESSION lpMAPISession = NULL;

	auto hRes = MAPILogonEx(0, NULL, NULL,
		MAPI_LOGON_UI,
		&lpMAPISession);
	if (SUCCEEDED(hRes))
	{
		LPMDB lpMDB = NULL;
		hRes = OpenDefaultMessageStore(lpMAPISession, bOnline ? MDB_ONLINE : NULL, &lpMDB);
		if (SUCCEEDED(hRes))
		{
			LPMAPIFOLDER lpInbox = NULL;
			hRes = OpenInbox(lpMDB, bOnline ? MAPI_NO_CACHE : NULL, &lpInbox);
			if (SUCCEEDED(hRes))
			{
				auto message = caller + L": got Inbox " + (bOnline ? L"online" : L"cached");
				MessageBoxW(NULL, message.c_str(), L"Sample Add-In", MB_OK | MB_ICONINFORMATION);
			}

			if (lpInbox) lpInbox->Release();
			lpInbox = NULL;
		}

		if (lpMDB) lpMDB->Release();
		lpMDB = NULL;
	}

	if (lpMAPISession) lpMAPISession->Release();
	lpMAPISession = NULL;
}

STDMETHODIMP CConnect::OnConnection(
	IDispatch *pApplication,
	ext_ConnectMode /* ConnectMode */,
	IDispatch* /* pAddInInst */,
	SAFEARRAY ** /* custom */)
{
	if (!pApplication)
		return E_POINTER;

	if (!m_bMAPIInitialized)
	{
		HRESULT hRes = MAPIInitialize(NULL);
		if (SUCCEEDED(hRes))
		{
			m_bMAPIInitialized = true;
			//TestMAPI();
		}
	}

	m_pApplication = pApplication;

	//MessageBoxW(NULL, L"OnConnection fired", L"Sample Add-In", MB_OK | MB_ICONINFORMATION);

	ApplicationEventSink::DispEventAdvise(m_pApplication);

	m_pApplication->ActiveExplorer(&m_pExplorer);
	ExplorerEventSink::DispEventAdvise(m_pExplorer, &__uuidof(Outlook::ExplorerEvents));

	return S_OK;
}

STDMETHODIMP CConnect::OnDisconnection(ext_DisconnectMode /*RemoveMode*/, SAFEARRAY ** /*custom*/)
{

	if (m_pExplorer)
	{
		ExplorerEventSink::DispEventUnadvise(m_pExplorer);
		m_pExplorer.Release();
	}

	if (m_pApplication)
	{
		ApplicationEventSink::DispEventUnadvise(m_pApplication);
		m_pApplication.Release();
	}

	if (m_bMAPIInitialized)
	{
		MAPIUninitialize();
		m_bMAPIInitialized = false;
	}

	return S_OK;
}

STDMETHODIMP CConnect::OnAddInsUpdate(SAFEARRAY ** /*custom*/)
{
	return S_OK;
}

STDMETHODIMP CConnect::OnStartupComplete(SAFEARRAY ** /*custom*/)
{
	return S_OK;
}

STDMETHODIMP CConnect::OnBeginShutdown(SAFEARRAY ** /*custom*/)
{
	return S_OK;
}

STDMETHODIMP CConnect::Invoke(
	DISPID dispidMember,
	const IID &riid,
	LCID lcid,
	WORD wFlags,
	DISPPARAMS *pdispparams,
	VARIANT *pvarResult,
	EXCEPINFO *pexceptinfo,
	UINT *puArgErr)
{
	HRESULT hr;

	// Currently the CConnect object can get away with only one implementation
	// of Invoke because the only interfaces that Outlook calls Invoke on are
	// the ribbon callbacks and the form region startup. The other interfaces
	// IRibbonExtensibility, IDTExtensibility, and ICustomTaskPaneConsumer are
	// currently called via the virtual table and not the automation invoke
	// method although this could potentially change in the future. The key
	// thing to remember about using a common Invoke for multiple interfaces
	// is to ensure that dispids for the different interfaces don't overlap.

	// This is assuming the ribbon callback dispids are low and they not
	// intersect with any of the form region startup dispids
	hr = IRibbonCallbackImpl::Invoke(dispidMember, riid, lcid, wFlags, pdispparams, pvarResult, pexceptinfo, puArgErr);

	if (DISP_E_MEMBERNOTFOUND == hr)
		hr = FormRegionStartupImpl::Invoke(dispidMember, riid, lcid, wFlags, pdispparams, pvarResult, pexceptinfo, puArgErr);

	if (DISP_E_MEMBERNOTFOUND == hr)
		hr = IDTExtensibilityImpl::Invoke(dispidMember, riid, lcid, wFlags, pdispparams, pvarResult, pexceptinfo, puArgErr);

	if (DISP_E_MEMBERNOTFOUND == hr)
	{
		// TODO: Figure out how to tell which one to invoke.
		hr = ApplicationEventSink::Invoke(dispidMember, riid, lcid, wFlags, pdispparams, pvarResult, pexceptinfo, puArgErr);
		hr = ExplorerEventSink::Invoke(dispidMember, riid, lcid, wFlags, pdispparams, pvarResult, pexceptinfo, puArgErr);
	}

	// There is no use trying to call Invoke on IRibbonExtensiblilityImpl and
	// ICustomTaskPaneConsumerImpl because they only contain one method with
	// dispid 1 and the other interfaces above most likely already cover dispid
	// 1, namely IDTExtensibility2, plus as already mention they are usually
	// called via the virtual table instead of IDispatch::Invoke.

	return hr;
}

/*!-----------------------------------------------------------------------
	FormRegionStartup interface implementation
-----------------------------------------------------------------------!*/

STDMETHODIMP CConnect::GetFormRegionStorage(
	BSTR /* bstrFormRegionName */,
	IDispatch * /* pDispItem */,
	long /* LCID */,
	Outlook::OlFormRegionMode /* formRegionMode */,
	Outlook::OlFormRegionSize /* formRegionSize */,
	__out VARIANT * pVarStorage)
{
	SAFEARRAY* pSafeArray = GetOFSResource(IDS_FORMREGIONSTORAGE);

	if (!pSafeArray)
		return E_UNEXPECTED;

	V_VT(pVarStorage) = VT_ARRAY | VT_UI1;
	V_ARRAY(pVarStorage) = pSafeArray;
	return S_OK;
}

STDMETHODIMP CConnect::BeforeFormRegionShow(_FormRegion *pFormRegion)
{
	return FormRegionWrapper::Setup(pFormRegion);
}

STDMETHODIMP CConnect::GetFormRegionManifest(
	BSTR /* bstrFormRegionName */,
	long /* LCID */,
	__out VARIANT * pvarManifest)
{
	BSTR bstr = GetXMLResource(IDS_FORMREGIONMANIFEST);

	if (!bstr)
		return E_UNEXPECTED;

	V_VT(pvarManifest) = VT_BSTR;
	V_BSTR(pvarManifest) = bstr;
	return S_OK;
}

STDMETHODIMP CConnect::GetFormRegionIcon(
	BSTR /* bstrFormRegionName */,
	long /* LCID */,
	Outlook::OlFormRegionIcon /* formRegionIcon */,
	__out VARIANT* /* pvarIcon */)
{
	return S_OK;
}

/*!-----------------------------------------------------------------------
	ICustomTaskPaneConsumer interface implementation
-----------------------------------------------------------------------!*/

HRESULT CConnect::CTPFactoryAvailable(ICTPFactory *CTPFactoryInst)
{
	m_pCTPFactory = CTPFactoryInst;

	return HrCreateSampleTaskPane();
}

/*!-----------------------------------------------------------------------
	IRibbonExtensibility interface implementation
-----------------------------------------------------------------------!*/

HRESULT CConnect::GetCustomUI(BSTR /* ribbonID */, BSTR *ribbonXml)
{
	if (!ribbonXml)
		return E_POINTER;

	// Get the same ribbon xml for every ribbonID
	*ribbonXml = GetXMLResource(IDS_CUSTOMRIBBON);
	return S_OK;
}

HRESULT CConnect::Button1Clicked(IDispatch* /* ribbonControl */)
{
	MessageBoxW(NULL,
		L"Going to create a task pane now!",
		L"Message from ribbon button.",
		MB_OK | MB_ICONINFORMATION);

	return HrCreateSampleTaskPane();
}

HRESULT CConnect::OptionsPagesAdd(IDispatch *pages)
{
	if (!pages)
		return E_POINTER;

	PropertyPagesPtr spPages(pages);

	if (!spPages)
		return E_UNEXPECTED;

	return spPages->Add(variant_t(SAMPLECONTROL_PROGID), bstr_t("Sample Options"));
}

HRESULT CConnect::MapiLogonComplete()
{
	//MessageBoxW(NULL, L"MapiLogonComplete", L"Sample Add-In", MB_OK | MB_ICONINFORMATION);
	return S_OK;
}

HRESULT CConnect::ItemSend()
{
	//MessageBoxW(NULL, L"ItemSend", L"Sample Add-In", MB_OK | MB_ICONINFORMATION);
	return S_OK;
}

void CConnect::OnClose()
{
	//MessageBoxW(NULL, L"OnClose", L"Sample Add-In", MB_OK | MB_ICONINFORMATION);
}

void CConnect::FolderSwitch()
{
	//MessageBoxW(NULL, L"FolderSwitch", L"Sample Add-In", MB_OK | MB_ICONINFORMATION);
	//TestMAPI(L"FolderSwitch", true);
}

HRESULT CConnect::HrCreateSampleTaskPane()
{
	if (!m_pCTPFactory)
		return E_POINTER;

	_CustomTaskPanePtr ctp;

	HRESULT hr = m_pCTPFactory->CreateCTP(bstr_t(SAMPLECONTROL_PROGID), bstr_t("Sample Task Pane"), vtMissing, &ctp);

	if (SUCCEEDED(hr))
		ctp->put_Visible(VARIANT_TRUE);

	return hr;
}