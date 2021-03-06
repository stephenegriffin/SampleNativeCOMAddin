#pragma once
#include "stdafx.h"

class ExplorerEventsSink:
	public IDispEventSimpleImpl<1, ExplorerEventsSink, &__uuidof(ExplorerEvents_10)>
{
public:
	ExplorerEventsSink(_ExplorerPtr piExplorer);
	virtual ~ExplorerEventsSink();

	static _ATL_FUNC_INFO FolderSwitchInfo;
	static _ATL_FUNC_INFO OnCloseInfo;

	BEGIN_SINK_MAP(ExplorerEventsSink)
		SINK_ENTRY_INFO(1, __uuidof(ExplorerEvents_10), dispidEventFolderSwitch, FolderSwitch, &FolderSwitchInfo)
		SINK_ENTRY_INFO(1, __uuidof(ExplorerEvents_10), dispidEventClose, OnClose, &OnCloseInfo)
	END_SINK_MAP()

	// ExplorerEvents Methods
	void __stdcall OnClose();
	void __stdcall FolderSwitch();

private:
	_ExplorerPtr m_piExplorer = nullptr;
};